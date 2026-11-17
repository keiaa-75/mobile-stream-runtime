#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <msr/backend/backend_adapter.hpp>
#include <msr/memory/memory_manager.hpp>
#include <msr/model/gguf.hpp>
#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>
#include <msr/streaming/layer_plan.hpp>
#include <msr/streaming/streaming_loader.hpp>

using namespace msr;

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    } else {
        std::cout << "ok   " << what << "\n";
    }
}

void test_memory_manager() {
    memory::MemoryManager mm(1u << 20);
    memory::Buffer a{}, b{};
    std::string err;
    check(mm.acquire(700000, a, &err) == Status::Ok, "arena acquires 700000 B");
    check(mm.acquire(500000, b, &err) == Status::InvalidArgument,
          "second allocation fails when budget exhausted");
    check(mm.stats().fail_count == 1, "failure is counted");
    check(mm.release(a.id, &err) == Status::Ok, "buffer released");
    check(mm.acquire(500000, b, &err) == Status::Ok, "freed range is reused");
    check(mm.pin(b.id, &err) == Status::Ok, "buffer pinned");
    check(mm.release(b.id, &err) == Status::InvalidArgument, "pinned buffer cannot be released");
    check(mm.unpin(b.id, &err) == Status::Ok, "buffer unpinned");
    check(mm.release(b.id, &err) == Status::Ok, "unpinned buffer released");
    check(mm.stats().peak_bytes == 700416, "peak is rounded allocation size");
    check(mm.stats().current_bytes == 0, "all bytes returned");
}

}  // namespace

int main(int argc, char** argv) {
    std::string model = (argc > 1)
        ? argv[1]
        : "/storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf";

    test_memory_manager();

    std::unique_ptr<storage::ModelFile> file;
    std::string err;
    Status st = storage::ModelFile::open(model, false, file, &err);
    check(st == Status::Ok, "real model opens: " + err);
    if (st != Status::Ok) return 1;

    model::GgufHeader header;
    st = parse_gguf(*file, header, model::ValidationOptions{}, &err);
    check(st == Status::Ok, "real model parses: " + err);
    if (st != Status::Ok) return 1;

    std::vector<streaming::LayerUnit> units;
    st = build_layer_plan(header, units, &err);
    check(st == Status::Ok, "layer plan builds: " + err);
    check(units.size() == 36, "36 units (embedding + output_norm + 34 layers)");
    bool embedding_first = !units.empty() && units.front().name == "token_embd.weight";
    check(embedding_first, "tied embedding is first unit");
    bool layers_ordered = true;
    int seen = 0;
    for (const auto& u : units) {
        if (u.name.rfind("blk.", 0) == 0) {
            int n = std::stoi(u.name.substr(4));
            if (n != seen++) layers_ordered = false;
        }
    }
    check(layers_ordered && seen == 34, "34 layer blocks in numeric order");

    streaming::ResidencyPlan tiny;
    st = choose_residency(units, 1024, tiny, &err);
    check(st == Status::InvalidArgument,
          "plan fails loudly when largest unit exceeds budget");

    constexpr std::uint64_t kBudget = 640ull << 20;  // 640 MiB > 525 MiB embedding
    streaming::ResidencyPlan plan;
    st = choose_residency(units, kBudget, plan, &err);
    check(st == Status::Ok, "640 MiB residency plan succeeds: " + err);
    check(plan.fits(kBudget), "resident set fits budget");
    check(!plan.streamed.empty(), "some units are streamed");
    check(plan.max_unit_bytes <= kBudget, "largest unit fits budget");

    // MedGemma-specific practical gate budget: still below the 2.313 GiB
    // payload, but only a handful of layers need streaming per token.
    constexpr std::uint64_t kMedGemmaBudget = 2ull << 30;
    streaming::ResidencyPlan med_plan;
    st = choose_residency(units, kMedGemmaBudget, med_plan, &err);
    check(st == Status::Ok, "2 GiB MedGemma plan succeeds: " + err);
    check(med_plan.fits(kMedGemmaBudget), "2 GiB resident set fits");
    check(!med_plan.streamed.empty(), "2 GiB budget still streams some layers");
    check(med_plan.streamed.size() <= 12,
          "2 GiB budget streams at most 12 of 34 layers");
    std::uint64_t med_free = kMedGemmaBudget - med_plan.resident_alloc_bytes;
    bool med_stream_fits = true;
    for (const auto* u : med_plan.streamed) {
        std::uint64_t cost = ((u->bytes + 4095) / 4096) * 4096;
        if (cost > med_free) med_stream_fits = false;
    }
    check(med_stream_fits, "every 2 GiB streamed unit fits the leftover arena");

    std::uint64_t streamed_bytes = 0;
    for (const auto* u : plan.streamed) streamed_bytes += u->bytes;

    memory::MemoryManager mm(kBudget);
    backend::HashBackend backend;
    streaming::StreamingLoader loader(*file, mm, false);
    streaming::RunStats stats;
    auto started = std::chrono::steady_clock::now();
    st = loader.run(header, units, plan, backend, 2, stats, &err);
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();

    check(st == Status::Ok, "two-token streamed pass completes: " + err);
    check(stats.peak_weight_bytes <= kBudget, "peak weight bytes <= budget");
    check(stats.peak_weight_bytes < header.payload_bytes,
          "peak weight bytes << full model payload");
    check(stats.resident_read_bytes == plan.resident_bytes,
          "resident units read exactly once");
    check(stats.streamed_read_bytes == 2 * streamed_bytes,
          "streamed units re-read per token");
    check(stats.total_read_bytes == stats.resident_read_bytes + stats.streamed_read_bytes,
          "total read accounting consistent");
    check(mm.stats().current_bytes == 0, "streaming returns all buffers");
    check(backend.calls() == 2 * units.size(), "backend called once per unit per token");

    const model::Digest* norm = backend.digest("output_norm.weight");
    check(norm != nullptr &&
          model::digest_hex(*norm) ==
              "ff0f152b2ca0307c9a8a3fd867da8cd8c2b99f356fec90b35fd6dfcfc27efa5c",
          "staged output_norm bytes match Phase 1 hash");

    std::cout << "PROOF budget_bytes=" << kBudget
              << " model_payload_bytes=" << header.payload_bytes
              << " peak_weight_bytes=" << stats.peak_weight_bytes
              << " total_read_bytes=" << stats.total_read_bytes
              << " read_amplification=" << stats.read_amplification(header.payload_bytes)
              << " seconds=" << elapsed << "\n";

    std::cout << (failures ? "FAILED " : "PASSED ") << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
