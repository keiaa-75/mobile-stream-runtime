#include <msr/streaming/streaming_loader.hpp>

#include <algorithm>
#include <unordered_map>

namespace msr::streaming {

StreamingLoader::StreamingLoader(storage::ModelFile& file,
                                 memory::MemoryManager& memory,
                                 bool use_direct_io)
    : file_(file), memory_(memory), use_direct_io_(use_direct_io) {}

Status StreamingLoader::stage_unit(const model::GgufHeader& header,
                                   const LayerUnit& unit,
                                   memory::Buffer& buffer, RunStats& stats,
                                   bool streamed, std::string* err) {
    Status st = memory_.acquire(unit.bytes, buffer, err);
    if (st != Status::Ok) return st;

    std::unordered_map<std::string, const model::TensorInfo*> by_name;
    by_name.reserve(header.tensors.size());
    for (const auto& t : header.tensors) by_name[t.name] = &t;

    std::uint64_t cursor = 0;
    for (const auto& name : unit.tensor_names) {
        if (file_.cancelled()) {
            if (err) *err = "streaming cancelled";
            return Status::Cancelled;
        }
        auto it = by_name.find(name);
        if (it == by_name.end()) {
            if (err) *err = "tensor not found in header: " + name;
            return Status::ParseError;
        }
        const model::TensorInfo& t = *it->second;
        st = read_tensor_range(file_, header, t, 0, static_cast<std::size_t>(t.bytes),
                               static_cast<char*>(buffer.data) + cursor,
                               use_direct_io_, scratch_, err);
        if (st != Status::Ok) return st;
        cursor += t.bytes;
        ++stats.read_calls;
        if (streamed) stats.streamed_read_bytes += t.bytes;
        else stats.resident_read_bytes += t.bytes;
    }
    return Status::Ok;
}

Status StreamingLoader::run(const model::GgufHeader& header,
                            const std::vector<LayerUnit>& units,
                            const ResidencyPlan& plan,
                            backend::BackendAdapter& backend,
                            std::uint64_t tokens,
                            RunStats& stats,
                            std::string* err) {
    stats = RunStats{};
    if (tokens == 0) {
        if (err) *err = "token count must be positive";
        return Status::InvalidArgument;
    }
    if (plan.resident_alloc_bytes > memory_.budget()) {
        if (err) *err = "plan exceeds budget";
        return Status::InvalidArgument;
    }

    std::vector<memory::Buffer> resident_buffers;
    resident_buffers.reserve(plan.resident.size());
    for (const LayerUnit* unit : plan.resident) {
        memory::Buffer buf{};
        Status st = stage_unit(header, *unit, buf, stats, false, err);
        if (st != Status::Ok) return st;
        st = memory_.pin(buf.id, err);
        if (st != Status::Ok) return st;
        resident_buffers.push_back(buf);
    }

    for (std::uint64_t token = 0; token < tokens; ++token) {
        if (file_.cancelled()) {
            if (err) *err = "streaming cancelled";
            return Status::Cancelled;
        }
        for (std::size_t i = 0; i < plan.resident.size(); ++i) {
            Status st = backend.execute(plan.resident[i]->name,
                                        resident_buffers[i].data,
                                        plan.resident[i]->bytes, err);
            if (st != Status::Ok) return st;
            ++stats.backend_calls;
        }
        for (const LayerUnit* unit : plan.streamed) {
            if (file_.cancelled()) {
                if (err) *err = "streaming cancelled";
                return Status::Cancelled;
            }
            memory::Buffer buf{};
            Status st = stage_unit(header, *unit, buf, stats, true, err);
            if (st != Status::Ok) return st;
            st = backend.execute(unit->name, buf.data, unit->bytes, err);
            if (st != Status::Ok) return st;
            ++stats.backend_calls;
            st = memory_.release(buf.id, err);
            if (st != Status::Ok) return st;
        }
    }

    for (const auto& buf : resident_buffers) {
        Status st = memory_.unpin(buf.id, err);
        if (st != Status::Ok) return st;
        st = memory_.release(buf.id, err);
        if (st != Status::Ok) return st;
    }

    stats.tokens = tokens;
    stats.total_read_bytes = stats.resident_read_bytes + stats.streamed_read_bytes;
    stats.peak_weight_bytes = memory_.stats().peak_bytes;

    // Hard rule: a budget below the model size must never become a full load.
    if (memory_.budget() < header.payload_bytes &&
        stats.peak_weight_bytes >= header.payload_bytes) {
        if (err) *err = "peak weight bytes reached full model size (silent fallback)";
        return Status::ParseError;
    }
    (void)units;
    return Status::Ok;
}

}  // namespace msr::streaming
