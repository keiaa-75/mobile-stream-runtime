#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <msr/model/gguf.hpp>
#include <msr/status.hpp>

namespace msr::streaming {

struct LayerUnit {
    std::string name;
    std::uint64_t bytes = 0;
    std::uint64_t elements = 0;
    std::vector<std::string> tensor_names;
};

struct ResidencyPlan {
    std::vector<const LayerUnit*> resident;
    std::vector<const LayerUnit*> streamed;
    std::uint64_t resident_bytes = 0;        // payload bytes, for I/O accounting
    std::uint64_t resident_alloc_bytes = 0;  // page-rounded arena consumption
    std::uint64_t max_unit_bytes = 0;

    bool fits(std::uint64_t budget) const { return resident_alloc_bytes <= budget; }
};

Status build_layer_plan(const model::GgufHeader& header,
                        std::vector<LayerUnit>& out, std::string* err);

Status choose_residency(const std::vector<LayerUnit>& units, std::uint64_t budget,
                        ResidencyPlan& out, std::string* err);

}  // namespace msr::streaming
