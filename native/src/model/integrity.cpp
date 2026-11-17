#include <msr/model/integrity.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <vector>

#include <fcntl.h>
#include <openssl/evp.h>
#include <unistd.h>

namespace msr::model {
namespace {

constexpr std::size_t kChunk = 1u << 20;

struct EvpCtx {
    EVP_MD_CTX* ctx = nullptr;
    EvpCtx() : ctx(EVP_MD_CTX_new()) {}
    ~EvpCtx() { if (ctx) EVP_MD_CTX_free(ctx); }
    EvpCtx(const EvpCtx&) = delete;
    EvpCtx& operator=(const EvpCtx&) = delete;
};

bool cancelled(const std::atomic<bool>* flag) {
    return flag && flag->load();
}

}  // namespace

std::string digest_hex(const Digest& d) {
    static const char* digits = "0123456789abcdef";
    std::string s;
    s.reserve(64);
    for (unsigned char b : d) {
        s.push_back(digits[b >> 4]);
        s.push_back(digits[b & 0x0f]);
    }
    return s;
}

bool parse_digest_hex(const std::string& hex, Digest& out) {
    if (hex.size() != 64) return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < 32; ++i) {
        int hi = nibble(hex[2 * i]);
        int lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}

Status sha256_bytes(const void* data, std::size_t len, Digest& out,
                    std::string* err) {
    EvpCtx md;
    if (!md.ctx || EVP_DigestInit_ex(md.ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(md.ctx, data, len) != 1) {
        if (err) *err = "EVP hashing failed";
        return Status::IoError;
    }
    unsigned int dlen = 0;
    if (EVP_DigestFinal_ex(md.ctx, out.data(), &dlen) != 1 || dlen != 32) {
        if (err) *err = "EVP final failed";
        return Status::IoError;
    }
    return Status::Ok;
}

Status sha256_file(const std::string& path, const std::atomic<bool>* cancel,
                   Digest& out, std::string* err) {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        if (err) *err = "open failed: " + std::string(std::strerror(errno));
        return Status::IoError;
    }
    EvpCtx md;
    if (!md.ctx || EVP_DigestInit_ex(md.ctx, EVP_sha256(), nullptr) != 1) {
        ::close(fd);
        if (err) *err = "EVP init failed";
        return Status::IoError;
    }
    std::vector<unsigned char> buf(kChunk);
    for (;;) {
        if (cancelled(cancel)) {
            ::close(fd);
            if (err) *err = "hash cancelled";
            return Status::Cancelled;
        }
        ssize_t n = ::read(fd, buf.data(), buf.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            if (err) *err = "read failed: " + std::string(std::strerror(errno));
            return Status::IoError;
        }
        if (n == 0) break;
        if (EVP_DigestUpdate(md.ctx, buf.data(), static_cast<std::size_t>(n)) != 1) {
            ::close(fd);
            if (err) *err = "EVP update failed";
            return Status::IoError;
        }
    }
    ::close(fd);
    unsigned int len = 0;
    if (EVP_DigestFinal_ex(md.ctx, out.data(), &len) != 1 || len != 32) {
        if (err) *err = "EVP final failed";
        return Status::IoError;
    }
    return Status::Ok;
}

Status sha256_range(storage::ModelFile& file, std::uint64_t offset,
                    std::uint64_t len, Digest& out, std::string* err) {
    if (offset > file.size() || len > file.size() - offset) {
        if (err) *err = "hash range past end of file";
        return Status::Eof;
    }
    EvpCtx md;
    if (!md.ctx || EVP_DigestInit_ex(md.ctx, EVP_sha256(), nullptr) != 1) {
        if (err) *err = "EVP init failed";
        return Status::IoError;
    }
    std::vector<unsigned char> buf(kChunk);
    std::uint64_t left = len;
    std::uint64_t pos = offset;
    while (left > 0) {
        if (file.cancelled()) {
            if (err) *err = "hash cancelled";
            return Status::Cancelled;
        }
        std::size_t take = static_cast<std::size_t>(
            std::min<std::uint64_t>(left, buf.size()));
        Status st = file.pread_exact(pos, buf.data(), take, err);
        if (st != Status::Ok) return st;
        if (EVP_DigestUpdate(md.ctx, buf.data(), take) != 1) {
            if (err) *err = "EVP update failed";
            return Status::IoError;
        }
        pos += take;
        left -= take;
    }
    unsigned int dlen = 0;
    if (EVP_DigestFinal_ex(md.ctx, out.data(), &dlen) != 1 || dlen != 32) {
        if (err) *err = "EVP final failed";
        return Status::IoError;
    }
    return Status::Ok;
}

Status verify_file_sha256(const std::string& path, const std::string& expected_hex,
                          Digest& actual, std::string* err) {
    Digest expected{};
    if (!parse_digest_hex(expected_hex, expected)) {
        if (err) *err = "malformed expected SHA-256 hex";
        return Status::InvalidArgument;
    }
    Status st = sha256_file(path, nullptr, actual, err);
    if (st != Status::Ok) return st;
    if (actual != expected) {
        if (err) *err = "SHA-256 mismatch: got " + digest_hex(actual) +
                        ", expected " + expected_hex;
        return Status::IntegrityMismatch;
    }
    return Status::Ok;
}

}  // namespace msr::model
