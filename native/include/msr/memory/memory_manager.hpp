#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

#include <msr/status.hpp>

namespace msr::memory {

using BufferId = std::uint64_t;
constexpr BufferId kInvalidBuffer = 0;

struct Buffer {
    BufferId id = kInvalidBuffer;
    void* data = nullptr;
    std::uint64_t bytes = 0;
    bool pinned = false;
};

struct Stats {
    std::uint64_t budget_bytes = 0;
    std::uint64_t capacity_bytes = 0;
    std::uint64_t current_bytes = 0;
    std::uint64_t peak_bytes = 0;
    std::uint64_t pinned_bytes = 0;
    std::uint64_t acquire_calls = 0;
    std::uint64_t release_calls = 0;
    std::uint64_t fail_count = 0;
};

/// One aligned arena with first-fit reuse, explicit pinning and hard budget.
/// There is no fallback allocation outside the arena.
class MemoryManager {
public:
    explicit MemoryManager(std::uint64_t budget_bytes, std::uint64_t alignment = 4096);
    ~MemoryManager();

    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    Status acquire(std::uint64_t bytes, Buffer& out, std::string* err);
    Status release(BufferId id, std::string* err);
    Status pin(BufferId id, std::string* err);
    Status unpin(BufferId id, std::string* err);

    const Stats& stats() const { return stats_; }
    std::uint64_t budget() const { return stats_.budget_bytes; }
    std::uint64_t capacity() const { return stats_.capacity_bytes; }
    void* base() const { return arena_; }

private:
    struct Live {
        std::uint64_t offset = 0;
        std::uint64_t bytes = 0;
        bool pinned = false;
    };

    void* arena_ = nullptr;
    std::uint64_t alignment_ = 4096;
    std::map<std::uint64_t, std::uint64_t> free_;  // offset -> length
    std::unordered_map<BufferId, Live> live_;
    BufferId next_id_ = 1;
    Stats stats_;
};

}  // namespace msr::memory
