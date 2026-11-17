#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include <msr/status.hpp>
#include <msr/storage/model_file.hpp>

namespace msr::model {

using Digest = std::array<unsigned char, 32>;

std::string digest_hex(const Digest& d);
bool parse_digest_hex(const std::string& hex, Digest& out);

Status sha256_bytes(const void* data, std::size_t len, Digest& out,
                    std::string* err);

Status sha256_file(const std::string& path, const std::atomic<bool>* cancel,
                   Digest& out, std::string* err);

Status sha256_range(storage::ModelFile& file, std::uint64_t offset,
                    std::uint64_t len, Digest& out, std::string* err);

Status verify_file_sha256(const std::string& path, const std::string& expected_hex,
                          Digest& actual, std::string* err);

}  // namespace msr::model
