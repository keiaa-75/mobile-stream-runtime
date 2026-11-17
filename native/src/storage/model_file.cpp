#include <msr/storage/model_file.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace msr::storage {

AlignedBuffer::~AlignedBuffer() {
    if (data_) std::free(data_);
}

bool AlignedBuffer::allocate(std::size_t bytes, std::size_t alignment) {
    if (bytes == 0) bytes = 1;
    if ((alignment & (alignment - 1)) != 0) return false;
    void* p = nullptr;
    if (posix_memalign(&p, alignment, bytes) != 0) return false;
    if (data_) std::free(data_);
    data_ = p;
    size_ = bytes;
    std::memset(data_, 0, bytes);
    return true;
}

ModelFile::~ModelFile() {
    if (fd_ >= 0) ::close(fd_);
}

Status ModelFile::open(const std::string& path, bool request_direct_io,
                       std::unique_ptr<ModelFile>& out, std::string* err) {
    int flags = O_RDONLY | O_CLOEXEC;
#ifdef O_DIRECT
    if (request_direct_io) flags |= O_DIRECT;
#endif
    int fd = ::open(path.c_str(), flags);
    bool direct = request_direct_io;
    if (fd < 0 && request_direct_io) {
        // Filesystems that do not support O_DIRECT get a buffered fallback.
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
        direct = false;
    }
    if (fd < 0) {
        if (err) *err = "open failed: " + std::string(std::strerror(errno));
        return Status::IoError;
    }

    struct stat st {};
    if (::fstat(fd, &st) != 0) {
        if (err) *err = "fstat failed: " + std::string(std::strerror(errno));
        ::close(fd);
        return Status::IoError;
    }
    if (!S_ISREG(st.st_mode) && st.st_size == 0) {
        if (err) *err = "not a regular file";
        ::close(fd);
        return Status::InvalidArgument;
    }

    auto f = std::unique_ptr<ModelFile>(new ModelFile());
    f->fd_ = fd;
    f->size_ = static_cast<std::uint64_t>(st.st_size);
    f->direct_io_ = direct;
    f->path_ = path;
    out = std::move(f);
    return Status::Ok;
}

Status ModelFile::pread_exact(std::uint64_t offset, void* dst, std::size_t len,
                              std::string* err) const {
    if (len == 0) return Status::Ok;
    if (!dst) {
        if (err) *err = "null destination";
        return Status::InvalidArgument;
    }
    if (offset > size_ || len > size_ - offset) {
        if (err) *err = "read past end of file";
        return Status::Eof;
    }
    auto* p = static_cast<char*>(dst);
    std::size_t done = 0;
    while (done < len) {
        if (cancelled()) {
            if (err) *err = "read cancelled";
            return Status::Cancelled;
        }
        ssize_t n = ::pread(fd_, p + done, len - done,
                            static_cast<off_t>(offset + done));
        if (n < 0) {
            if (errno == EINTR) continue;
            if (err) *err = "pread failed: " + std::string(std::strerror(errno));
            return Status::IoError;
        }
        if (n == 0) {
            if (err) *err = "unexpected EOF";
            return Status::Eof;
        }
        done += static_cast<std::size_t>(n);
    }
    return Status::Ok;
}

Status ModelFile::read_aligned(std::uint64_t offset, void* dst, std::size_t len,
                               AlignedBuffer& scratch, std::string* err) const {
    if (len == 0) return Status::Ok;
    if (!direct_io_) return pread_exact(offset, dst, len, err);

    constexpr std::uint64_t kPage = 4096;
    std::uint64_t delta = offset % kPage;
    std::uint64_t start = offset - delta;
    std::uint64_t span = delta + len;
    std::uint64_t rounded = ((span + kPage - 1) / kPage) * kPage;
    if (start > size_ || rounded > size_ - start) {
        // Tail of file: fall back to a buffered read for the final partial page.
        return pread_exact(offset, dst, len, err);
    }
    if (!scratch.allocate(static_cast<std::size_t>(rounded))) {
        if (err) *err = "aligned scratch allocation failed";
        return Status::InvalidArgument;
    }
    Status st = pread_exact(start, scratch.data(), static_cast<std::size_t>(rounded), err);
    if (st != Status::Ok) return st;
    std::memcpy(dst, static_cast<char*>(scratch.data()) + delta, len);
    return Status::Ok;
}

}  // namespace msr::storage
