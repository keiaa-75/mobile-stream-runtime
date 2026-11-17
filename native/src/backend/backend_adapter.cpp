#include <msr/backend/backend_adapter.hpp>

namespace msr::backend {

Status HashBackend::execute(const std::string& unit_name, const void* data,
                            std::size_t bytes, std::string* err) {
    model::Digest d{};
    Status st = model::sha256_bytes(data, bytes, d, err);
    if (st != Status::Ok) return st;
    digests_[unit_name] = d;
    ++calls_;
    return Status::Ok;
}

const model::Digest* HashBackend::digest(const std::string& unit_name) const {
    auto it = digests_.find(unit_name);
    return it == digests_.end() ? nullptr : &it->second;
}

}  // namespace msr::backend
