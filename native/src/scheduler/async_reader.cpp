#include <msr/scheduler/async_reader.hpp>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <msr/storage/model_file.hpp>

namespace msr::scheduler {
namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t ns_between(Clock::time_point a, Clock::time_point b) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
}

}  // namespace

struct AsyncReader::Impl {
    const storage::ModelFile* file = nullptr;
    std::size_t depth = 1;

    std::vector<std::thread> workers;
    mutable std::mutex mu;
    std::condition_variable cv_space;
    std::condition_variable cv_done;

    std::deque<ReadRequest> queue;
    std::unordered_map<std::uint64_t, Status> results;
    std::unordered_set<std::uint64_t> known;  // queued or in flight
    std::size_t in_flight = 0;

    bool stop = false;
    bool fatal = false;
    std::string fatal_err;

    ReadStats stats;

    void run() {
        for (;;) {
            ReadRequest req;
            {
                std::unique_lock<std::mutex> lock(mu);
                cv_space.wait(lock, [&] { return stop || !queue.empty(); });
                if (queue.empty()) {
                    // stop with nothing left to do
                    if (stop) return;
                    continue;
                }
                req = queue.front();
                queue.pop_front();
                ++in_flight;
                const std::uint64_t depth_now =
                    static_cast<std::uint64_t>(queue.size() + in_flight);
                if (depth_now > stats.peak_queue_depth) {
                    stats.peak_queue_depth = depth_now;
                }
            }

            const auto t0 = Clock::now();
            std::string err;
            const Status status = file->pread_exact(req.offset, req.dst, req.bytes, &err);
            const auto t1 = Clock::now();

            {
                std::lock_guard<std::mutex> lock(mu);
                --in_flight;
                stats.io_ns += ns_between(t0, t1);
                if (status == Status::Ok) {
                    ++stats.completed;
                    stats.bytes_read += req.bytes;
                } else {
                    ++stats.failed;
                    if (!fatal) {
                        fatal = true;
                        fatal_err = err.empty() ? "async read failed" : err;
                    }
                }
                results[req.tag] = status;
                known.erase(req.tag);
            }
            cv_done.notify_all();
            cv_space.notify_all();
        }
    }
};

AsyncReader::AsyncReader(const storage::ModelFile* file, std::size_t depth,
                         std::size_t workers)
    : impl_(new Impl()), depth_(depth == 0 ? 1 : depth),
      workers_(workers == 0 ? 1 : workers) {
    impl_->file = file;
    impl_->depth = depth_;
}

Status AsyncReader::create(const storage::ModelFile* file, std::size_t queue_depth,
                           std::unique_ptr<AsyncReader>& out, std::string* err,
                           std::size_t workers) {
    if (file == nullptr) {
        if (err) *err = "async reader needs an open model file";
        return Status::InvalidArgument;
    }
    std::unique_ptr<AsyncReader> reader(new AsyncReader(file, queue_depth, workers));
    Impl* impl = reader->impl_.get();
    for (std::size_t i = 0; i < reader->workers_; ++i) {
        impl->workers.emplace_back([impl] { impl->run(); });
    }
    out = std::move(reader);
    return Status::Ok;
}

AsyncReader::~AsyncReader() {
    if (!impl_) return;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->stop = true;
    }
    impl_->cv_space.notify_all();
    for (auto& worker : impl_->workers) {
        if (worker.joinable()) worker.join();
    }
}

Status AsyncReader::submit(const ReadRequest& req, std::string* err) {
    if (!impl_->file) {
        if (err) *err = "async reader is not attached to a file";
        return Status::InvalidArgument;
    }
    if (req.dst == nullptr || req.bytes == 0) {
        if (err) *err = "async read needs a destination and a positive size";
        return Status::InvalidArgument;
    }

    std::unique_lock<std::mutex> lock(impl_->mu);
    if (impl_->stop) {
        if (err) *err = "async reader is stopping";
        return Status::Cancelled;
    }
    if (impl_->fatal) {
        if (err) *err = impl_->fatal_err;
        return Status::IoError;
    }
    if (impl_->known.count(req.tag) != 0) {
        if (err) *err = "duplicate async read tag";
        return Status::InvalidArgument;
    }

    const auto t0 = Clock::now();
    while (impl_->queue.size() + impl_->in_flight >= impl_->depth) {
        impl_->cv_space.wait(lock);
        if (impl_->stop) {
            if (err) *err = "async reader is stopping";
            return Status::Cancelled;
        }
    }
    impl_->stats.submit_block_ns += ns_between(t0, Clock::now());

    impl_->queue.push_back(req);
    impl_->known.insert(req.tag);
    ++impl_->stats.submitted;
    lock.unlock();
    impl_->cv_space.notify_all();
    return Status::Ok;
}

Status AsyncReader::wait(std::uint64_t tag, std::string* err) {
    std::unique_lock<std::mutex> lock(impl_->mu);
    const auto t0 = Clock::now();
    if (impl_->known.count(tag) == 0 && impl_->results.count(tag) == 0) {
        if (err) *err = "async read tag is not pending or complete";
        return Status::InvalidArgument;
    }
    impl_->cv_done.wait(lock, [&] { return impl_->results.count(tag) != 0; });
    impl_->stats.wait_ns += ns_between(t0, Clock::now());
    const Status status = impl_->results[tag];
    impl_->results.erase(tag);
    return status;
}

Status AsyncReader::drain(std::string* err) {
    (void) err;
    std::unique_lock<std::mutex> lock(impl_->mu);
    impl_->cv_done.wait(lock, [&] {
        return impl_->queue.empty() && impl_->in_flight == 0;
    });
    return impl_->fatal ? Status::IoError : Status::Ok;
}

void AsyncReader::cancel_pending() {
    std::lock_guard<std::mutex> lock(impl_->mu);
    for (const auto& req : impl_->queue) {
        impl_->results[req.tag] = Status::Cancelled;
        impl_->known.erase(req.tag);
        ++impl_->stats.dropped;
    }
    impl_->queue.clear();
    // wake waiters inside the lock scope; the dropped results are already
    // visible so a racing waiter returns Cancelled instead of blocking.
    impl_->cv_done.notify_all();
    impl_->cv_space.notify_all();
}

ReadStats AsyncReader::stats() const {
    std::lock_guard<std::mutex> lock(impl_->mu);
    return impl_->stats;
}

// ---------------------------------------------------------------------------
// LayerPrefetcher
// ---------------------------------------------------------------------------

struct LayerPrefetcher::Impl {
    AsyncReader* reader = nullptr;
    std::size_t slots = 1;
    std::mutex mu;
    std::map<int, std::vector<std::uint64_t>> tags;  // layer -> pending tags
    std::set<int> pending;                           // layers with open tags
    std::uint64_t next_tag = 1;
};

LayerPrefetcher::LayerPrefetcher(AsyncReader* reader, std::size_t slots)
    : impl_(new Impl()) {
    impl_->reader = reader;
    impl_->slots = slots == 0 ? 1 : slots;
}

LayerPrefetcher::~LayerPrefetcher() {
    if (impl_ && impl_->reader) {
        impl_->reader->drain(nullptr);
    }
}

Status LayerPrefetcher::schedule(int layer, std::uint64_t offset, void* dst,
                                 std::size_t bytes, bool force_sync, std::string* err) {
    if (!impl_->reader || dst == nullptr || bytes == 0) {
        if (err) *err = "prefetch needs a reader and a destination";
        return Status::InvalidArgument;
    }

    ReadRequest req;
    req.offset = offset;
    req.dst = dst;
    req.bytes = bytes;

    std::uint64_t tag;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        tag = impl_->next_tag++;
    }
    req.tag = tag;

    if (force_sync) {
        Status status = impl_->reader->submit(req, err);
        if (status == Status::Ok) status = impl_->reader->wait(tag, err);
        return status;
    }

    const Status status = impl_->reader->submit(req, err);
    if (status == Status::Ok) {
        std::lock_guard<std::mutex> lock(impl_->mu);
        impl_->tags[layer].push_back(tag);
        impl_->pending.insert(layer);
    }
    return status;
}

Status LayerPrefetcher::wait_layer(int layer, std::string* err) {
    std::vector<std::uint64_t> tags;
    {
        std::lock_guard<std::mutex> lock(impl_->mu);
        auto it = impl_->tags.find(layer);
        if (it == impl_->tags.end()) {
            if (err) *err = "layer was not prefetched";
            return Status::InvalidArgument;
        }
        tags = std::move(it->second);
        impl_->tags.erase(it);
        impl_->pending.erase(layer);
    }
    Status worst = Status::Ok;
    for (const std::uint64_t tag : tags) {
        const Status s = impl_->reader->wait(tag, err);
        if (s != Status::Ok && worst == Status::Ok) worst = s;
    }
    return worst;
}

bool LayerPrefetcher::scheduled(int layer) const {
    std::lock_guard<std::mutex> lock(impl_->mu);
    return impl_->pending.count(layer) != 0;
}

void LayerPrefetcher::cancel_pending() {
    if (impl_->reader) impl_->reader->cancel_pending();
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->pending.clear();
    impl_->tags.clear();
}

void LayerPrefetcher::reset() {
    if (impl_->reader) impl_->reader->drain(nullptr);
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->pending.clear();
    impl_->tags.clear();
}

}  // namespace msr::scheduler
