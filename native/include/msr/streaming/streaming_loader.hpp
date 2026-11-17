#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <msr/backend/backend_adapter.hpp>
#include <msr/memory/memory_manager.hpp>
#include <msr/model/gguf.hpp>
#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>
#include <msr/streaming/layer_plan.hpp>

namespace msr::streaming {

struct RunStats {
    std::uint64_t tokens = 0;
    std::uint64_t resident_read_bytes = 0;
    std::uint64_t streamed_read_bytes = 0;
    std::uint64_t total_read_bytes = 0;
    std::uint64_t peak_weight_bytes = 0;
    std::uint64_t read_calls = 0;
    std::uint64_t backend_calls = 0;

    double read_amplification(std::uint64_t payload_bytes) const {
        return payload_bytes ? static_cast<double>(total_read_bytes) / payload_bytes : 0.0;
    }
};

/// Stages each unit into a bounded buffer, hands it to a backend, then
/// releases streamed units. Resident units stay pinned. No full-model load.
class StreamingLoader {
public:
    StreamingLoader(storage::ModelFile& file, memory::MemoryManager& memory,
                    bool use_direct_io = false);

    Status run(const model::GgufHeader& header,
               const std::vector<LayerUnit>& units,
               const ResidencyPlan& plan,
               backend::BackendAdapter& backend,
               std::uint64_t tokens,
               RunStats& stats,
               std::string* err);

private:
    Status stage_unit(const model::GgufHeader& header, const LayerUnit& unit,
                      memory::Buffer& buffer, RunStats& stats,
                      bool streamed, std::string* err);

    storage::ModelFile& file_;
    memory::MemoryManager& memory_;
    bool use_direct_io_;
    storage::AlignedBuffer scratch_;
};

}  // namespace msr::streaming
