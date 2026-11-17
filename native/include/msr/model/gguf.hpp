#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>

namespace msr::model {

struct MetaValue {
    std::uint32_t type = 0;
    std::string text;
};

using Metadata = std::unordered_map<std::string, MetaValue>;

struct TensorInfo {
    std::string name;
    std::uint32_t n_dims = 0;
    std::vector<std::uint64_t> dims;
    std::uint32_t type = 0;
    std::uint64_t offset = 0;      // relative to data_start
    std::uint64_t elements = 0;
    std::uint64_t bytes = 0;
    std::uint32_t block_size = 1;
};

struct GgufHeader {
    std::uint32_t version = 0;
    std::uint64_t tensor_count = 0;
    std::uint64_t kv_count = 0;
    std::uint32_t alignment = 32;
    std::uint64_t header_bytes = 0;
    std::uint64_t data_start = 0;
    std::uint64_t payload_bytes = 0;
    Metadata metadata;
    std::vector<TensorInfo> tensors;

    const TensorInfo* find(const std::string& name) const;
};

struct ValidationOptions {
    bool check_alignment = true;
    bool check_overlap = true;
    bool check_block_size = true;
    bool allow_zero_length_tensor = false;
};

/// Parses the GGUF header and tensor table with positional reads only.
Status parse_gguf(storage::ModelFile& file, GgufHeader& out,
                  const ValidationOptions& options, std::string* err);

/// Byte-exact bounded read of a tensor slice into `dst`.
Status read_tensor_range(storage::ModelFile& file, const GgufHeader& header,
                         const TensorInfo& tensor, std::uint64_t slice_offset,
                         std::size_t len, void* dst, bool use_direct_io,
                         storage::AlignedBuffer& scratch, std::string* err);

bool meta_as_u64(const Metadata& meta, const std::string& key, std::uint64_t& out);
bool meta_as_string(const Metadata& meta, const std::string& key, std::string& out);
const char* quant_type_name(std::uint32_t type);

}  // namespace msr::model
