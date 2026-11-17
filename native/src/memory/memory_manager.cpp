#include <msr/memory/memory_manager.hpp>

#include <algorithm>
#include <cstdlib>

namespace msr::memory {
namespace {

std::uint64_t align_up(std::uint64_t v, std::uint64_t a) {
    return ((v + a - 1) / a) * a;
}

}  // namespace

MemoryManager::MemoryManager(std::uint64_t budget_bytes, std::uint64_t alignment) {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) alignment = 4096;
    alignment_ = alignment;
    std::uint64_t capacity = budget_bytes - (budget_bytes % alignment_);
    stats_.budget_bytes = budget_bytes;
    stats_.capacity_bytes = capacity;
    if (capacity == 0) return;
    if (posix_memalign(&arena_, static_cast<std::size_t>(alignment_),
                       static_cast<std::size_t>(capacity)) != 0) {
        arena_ = nullptr;
        stats_.capacity_bytes = 0;
        return;
    }
    free_[0] = capacity;
}

MemoryManager::~MemoryManager() {
    if (arena_) std::free(arena_);
}

Status MemoryManager::acquire(std::uint64_t bytes, Buffer& out, std::string* err) {
    ++stats_.acquire_calls;
    if (bytes == 0 || !arena_) {
        ++stats_.fail_count;
        if (err) *err = "invalid allocation or empty arena";
        return Status::InvalidArgument;
    }
    std::uint64_t need = align_up(bytes, alignment_);
    auto it = free_.lower_bound(0);
    for (; it != free_.end(); ++it) {
        if (it->second >= need) break;
    }
    if (it == free_.end()) {
        ++stats_.fail_count;
        if (err) *err = "weight-buffer budget exhausted (no fallback allocation)";
        return Status::InvalidArgument;
    }
    std::uint64_t offset = it->first;
    std::uint64_t remaining = it->second - need;
    free_.erase(it);
    if (remaining > 0) free_[offset + need] = remaining;

    BufferId id = next_id_++;
    live_[id] = Live{offset, need, false};
    stats_.current_bytes += need;
    stats_.peak_bytes = std::max(stats_.peak_bytes, stats_.current_bytes);

    out.id = id;
    out.data = static_cast<char*>(arena_) + offset;
    out.bytes = need;
    out.pinned = false;
    return Status::Ok;
}

Status MemoryManager::release(BufferId id, std::string* err) {
    auto it = live_.find(id);
    if (it == live_.end()) {
        if (err) *err = "unknown buffer id";
        return Status::InvalidArgument;
    }
    if (it->second.pinned) {
        if (err) *err = "cannot release a pinned buffer";
        return Status::InvalidArgument;
    }
    std::uint64_t offset = it->second.offset;
    std::uint64_t bytes = it->second.bytes;
    live_.erase(it);
    stats_.current_bytes -= bytes;
    ++stats_.release_calls;

    auto next = free_.lower_bound(offset);
    if (next != free_.end() && next->first == offset + bytes) {
        bytes += next->second;
        free_.erase(next);
    }
    auto prev = free_.upper_bound(offset);
    if (prev != free_.begin()) {
        --prev;
        if (prev->first + prev->second == offset) {
            prev->second += bytes;
            return Status::Ok;
        }
    }
    free_[offset] = bytes;
    return Status::Ok;
}

Status MemoryManager::pin(BufferId id, std::string* err) {
    auto it = live_.find(id);
    if (it == live_.end()) {
        if (err) *err = "unknown buffer id";
        return Status::InvalidArgument;
    }
    if (!it->second.pinned) {
        it->second.pinned = true;
        stats_.pinned_bytes += it->second.bytes;
    }
    return Status::Ok;
}

Status MemoryManager::unpin(BufferId id, std::string* err) {
    auto it = live_.find(id);
    if (it == live_.end()) {
        if (err) *err = "unknown buffer id";
        return Status::InvalidArgument;
    }
    if (it->second.pinned) {
        it->second.pinned = false;
        stats_.pinned_bytes -= it->second.bytes;
    }
    return Status::Ok;
}

}  // namespace msr::memory
