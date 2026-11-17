#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

#include <msr/model/integrity.hpp>
#include <msr/status.hpp>

namespace msr::backend {

/// Minimal boundary between the streaming runtime and a compute backend.
/// Execute receives a unit name and its weights already staged in one buffer.
class BackendAdapter {
public:
    virtual ~BackendAdapter() = default;
    virtual Status execute(const std::string& unit_name, const void* data,
                           std::size_t bytes, std::string* err) = 0;
};

/// Test adapter only: hashes each staged unit instead of running a layer.
/// It proves buffer contents are delivered intact; it is NOT an inference
/// backend and must never be reported as one.
class HashBackend : public BackendAdapter {
public:
    Status execute(const std::string& unit_name, const void* data,
                   std::size_t bytes, std::string* err) override;

    bool has(const std::string& unit_name) const {
        return digests_.find(unit_name) != digests_.end();
    }
    const model::Digest* digest(const std::string& unit_name) const;
    std::uint64_t calls() const { return calls_; }

private:
    std::unordered_map<std::string, model::Digest> digests_;
    std::uint64_t calls_ = 0;
};

}  // namespace msr::backend
