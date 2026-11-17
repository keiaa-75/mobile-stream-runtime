#include <msr/streaming/layer_plan.hpp>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <tuple>

namespace msr::streaming {
namespace {

int rank_of(const std::string& name, std::uint64_t& index) {
    index = 0;
    if (name == "token_embd.weight") return 0;
    if (name == "output_norm.weight") return 1;
    if (name.rfind("blk.", 0) == 0) {
        index = std::strtoull(name.c_str() + 4, nullptr, 10);
        return 2;
    }
    if (name.rfind("v.block.", 0) == 0) {
        index = std::strtoull(name.c_str() + 8, nullptr, 10);
        return 3;
    }
    if (name.rfind("mm.", 0) == 0) return 4;
    return 5;
}

std::string unit_key(const std::string& name, int rank, std::uint64_t index) {
    if (rank == 2) return "blk." + std::to_string(index);
    if (rank == 3) return "v.block." + std::to_string(index);
    if (rank == 4 || rank == 5) return name;
    return name;
}

}  // namespace

Status build_layer_plan(const model::GgufHeader& header,
                        std::vector<LayerUnit>& out, std::string* err) {
    out.clear();
    std::map<std::string, LayerUnit> units;
    for (const auto& t : header.tensors) {
        std::uint64_t index = 0;
        int rank = rank_of(t.name, index);
        std::string key = unit_key(t.name, rank, index);
        auto& u = units[key];
        if (u.name.empty()) u.name = key;
        u.bytes += t.bytes;
        u.elements += t.elements;
        u.tensor_names.push_back(t.name);
    }

    std::vector<std::tuple<int, std::uint64_t, std::string>> order;
    for (const auto& kv : units) {
        std::uint64_t idx = 0;
        int rank = rank_of(kv.first, idx);
        order.emplace_back(rank, idx, kv.first);
    }
    std::sort(order.begin(), order.end());

    out.reserve(order.size());
    for (const auto& entry : order) {
        auto it = units.find(std::get<2>(entry));
        if (it == units.end()) {
            if (err) *err = "internal layer-plan error";
            return Status::ParseError;
        }
        out.push_back(it->second);
    }
    if (out.empty()) {
        if (err) *err = "GGUF has no tensors";
        return Status::ParseError;
    }
    return Status::Ok;
}

Status choose_residency(const std::vector<LayerUnit>& units, std::uint64_t budget,
                        ResidencyPlan& out, std::string* err) {
    out = ResidencyPlan{};
    std::uint64_t total_cost = 0;
    std::uint64_t max_cost = 0;
    std::uint64_t max_stream_candidate = 0;
    for (std::size_t i = 0; i < units.size(); ++i) {
        const auto& u = units[i];
        std::uint64_t cost = ((u.bytes + 4095) / 4096) * 4096;
        out.max_unit_bytes = std::max(out.max_unit_bytes, u.bytes);
        max_cost = std::max(max_cost, cost);
        // The first unit (tied embedding) is the natural resident one; the
        // largest possible streamed unit is therefore among the remaining ones.
        if (i > 0) max_stream_candidate = std::max(max_stream_candidate, cost);
        total_cost += cost;
    }

    if (max_cost > budget) {
        if (err) *err = "largest unit (" + std::to_string(out.max_unit_bytes) +
                        " B) exceeds budget (" + std::to_string(budget) +
                        " B); refusing silent full-model fallback";
        return Status::InvalidArgument;
    }

    // If every unit fits, keep everything resident. Otherwise reserve one
    // streamed-unit buffer so a pinned resident set cannot starve streaming.
    std::uint64_t resident_limit =
        (total_cost <= budget) ? budget : budget - max_stream_candidate;
    for (const auto& u : units) {
        std::uint64_t cost = ((u.bytes + 4095) / 4096) * 4096;
        if (out.resident_alloc_bytes + cost <= resident_limit) {
            out.resident.push_back(&u);
            out.resident_bytes += u.bytes;
            out.resident_alloc_bytes += cost;
        } else {
            out.streamed.push_back(&u);
        }
    }
    if (out.resident.empty()) {
        if (err) *err = "budget cannot hold resident units plus a stream buffer";
        return Status::InvalidArgument;
    }
    return Status::Ok;
}

}  // namespace msr::streaming
