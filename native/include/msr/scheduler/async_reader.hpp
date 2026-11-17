#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <msr/status.hpp>

namespace msr::storage {
class ModelFile;
}

namespace msr::scheduler {

/// One contiguous read handed to the worker thread.
struct ReadRequest {
    std::uint64_t offset = 0;
    void* dst = nullptr;
    std::size_t bytes = 0;
    /// Caller-defined identity, typically the layer this read belongs to.
    std::uint64_t tag = 0;
};

struct ReadStats {
    std::uint64_t submitted = 0;
    std::uint64_t completed = 0;
    std::uint64_t failed = 0;
    std::uint64_t dropped = 0;
    std::uint64_t bytes_read = 0;
    std::uint64_t io_ns = 0;
    std::uint64_t wait_ns = 0;
    std::uint64_t submit_block_ns = 0;
    std::uint64_t peak_queue_depth = 0;
};

/// Bounded async reader over `ModelFile::pread_exact`.
///
/// Guarantees:
///   - at most `queue_depth` requests are queued or in flight at any time;
///   - `submit()` blocks (bounded queue) rather than growing without limit;
///   - `cancel_pending()` drops queued work and marks it Cancelled so waiters
///     never hang; an in-flight read is allowed to finish;
///   - the worker never touches memory after `drain()`/destruction returns.
///
/// This is the portable fallback for platforms without io_uring/AIO: a small
/// pool of workers issuing positional reads. `workers` trades memory bandwidth
/// for per-layer latency (page-cache reads parallelise across cores). The
/// interface does not assume completion order, so a later kernel backend can
/// replace the workers.
class AsyncReader {
public:
    static Status create(const storage::ModelFile* file, std::size_t queue_depth,
                         std::unique_ptr<AsyncReader>& out, std::string* err,
                         std::size_t workers = 1);

    ~AsyncReader();

    AsyncReader(const AsyncReader&) = delete;
    AsyncReader& operator=(const AsyncReader&) = delete;

    Status submit(const ReadRequest& req, std::string* err);
    /// Waits for `tag` and consumes its result. Returns InvalidArgument if the
    /// tag is neither queued nor completed.
    Status wait(std::uint64_t tag, std::string* err);
    /// Waits until nothing is queued or in flight.
    Status drain(std::string* err);
    /// Drops queued (not yet started) reads, marking them Cancelled.
    void cancel_pending();

    ReadStats stats() const;
    std::size_t queue_depth() const { return depth_; }

private:
    AsyncReader(const storage::ModelFile* file, std::size_t depth, std::size_t workers);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::size_t depth_;
    std::size_t workers_;
};

/// Deterministic next-layer prefetch helper.
///
/// Owns no buffers: the caller supplies one destination buffer per slot and
/// must keep it alive until `wait_layer()` for that slot returns. The helper
/// only sequences reads and tracks which ranges belong to which layer.
class LayerPrefetcher {
public:
    LayerPrefetcher(AsyncReader* reader, std::size_t slots);
    ~LayerPrefetcher();

    LayerPrefetcher(const LayerPrefetcher&) = delete;
    LayerPrefetcher& operator=(const LayerPrefetcher&) = delete;

    /// Submits one range for `layer`. Call once per tensor; `wait_layer`
    /// waits for every range scheduled for that layer.
    Status schedule(int layer, std::uint64_t offset, void* dst, std::size_t bytes,
                    bool force_sync, std::string* err);

    /// Blocks until all ranges scheduled for `layer` have completed.
    Status wait_layer(int layer, std::string* err);

    /// True when `layer` has been scheduled and not yet waited for.
    bool scheduled(int layer) const;

    void cancel_pending();
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace msr::scheduler
