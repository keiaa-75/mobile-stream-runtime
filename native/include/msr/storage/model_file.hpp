#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <msr/status.hpp>

namespace msr::storage {

/// Page-aligned scratch buffer used by O_DIRECT read paths.
class AlignedBuffer {
public:
    AlignedBuffer() = default;
    ~AlignedBuffer();
    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;

    bool allocate(std::size_t bytes, std::size_t alignment = 4096);
    void* data() const { return data_; }
    std::size_t size() const { return size_; }

private:
    void* data_ = nullptr;
    std::size_t size_ = 0;
};

/// Random-access, positional reader for a model file. No mapping is created;
/// the whole file is never loaded.
class ModelFile {
public:
    ~ModelFile();

    ModelFile(const ModelFile&) = delete;
    ModelFile& operator=(const ModelFile&) = delete;

    static Status open(const std::string& path,
                       bool request_direct_io,
                       std::unique_ptr<ModelFile>& out,
                       std::string* err);

    std::uint64_t size() const { return size_; }
    bool direct_io() const { return direct_io_; }
    const std::string& path() const { return path_; }

    void cancel() { cancelled_.store(true); }
    void reset_cancel() { cancelled_.store(false); }
    bool cancelled() const { return cancelled_.load(); }

    /// Exact positional read. Never advances a shared file offset.
    Status pread_exact(std::uint64_t offset, void* dst, std::size_t len,
                       std::string* err) const;

    /// Reads [offset, offset+len) into `dst`. For O_DIRECT, rounds down to a
    /// page boundary into `scratch` and copies out only the requested bytes.
    Status read_aligned(std::uint64_t offset, void* dst, std::size_t len,
                        AlignedBuffer& scratch, std::string* err) const;

private:
    ModelFile() = default;

    int fd_ = -1;
    std::uint64_t size_ = 0;
    bool direct_io_ = false;
    std::string path_;
    mutable std::atomic<bool> cancelled_{false};
};

}  // namespace msr::storage
