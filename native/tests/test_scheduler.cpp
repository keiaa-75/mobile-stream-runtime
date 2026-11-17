// Tests for the bounded async reader and the layer prefetcher.
// No model file is needed: a deterministic scratch file is generated.
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <msr/scheduler/async_reader.hpp>
#include <msr/storage/model_file.hpp>

namespace {

constexpr std::size_t kMiB = 1024 * 1024;

int g_failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::cerr << "FAIL: " << what << "\n";
        ++g_failures;
    }
}

std::string make_payload(std::size_t bytes) {
    std::string s(bytes, '\0');
    for (std::size_t i = 0; i < bytes; ++i) {
        s[i] = static_cast<char>((i * 31 + 7) % 251);
    }
    return s;
}

}  // namespace

int main() {
    const char* tmpdir = std::getenv("TMPDIR");
    const std::string path =
        std::string(tmpdir && *tmpdir ? tmpdir : "/data/data/com.termux/files/usr/tmp") +
        "/msr_scheduler_test.bin";
    const std::size_t file_bytes = 4 * kMiB;
    const std::string payload = make_payload(file_bytes);
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        if (!out) {
            std::cerr << "cannot write scratch file\n";
            return 2;
        }
    }

    std::string err;
    std::unique_ptr<msr::storage::ModelFile> file;
    check(msr::storage::ModelFile::open(path, false, file, &err) == msr::Status::Ok,
          "open scratch file");

    // --- ordered reads, bounded queue --------------------------------------
    {
        std::unique_ptr<msr::scheduler::AsyncReader> reader;
        check(msr::scheduler::AsyncReader::create(file.get(), 2, reader, &err) ==
                  msr::Status::Ok, "create reader");

        std::vector<char> a(kMiB), b(kMiB), c(kMiB);
        msr::scheduler::ReadRequest r0{0, a.data(), a.size(), 10};
        msr::scheduler::ReadRequest r1{kMiB, b.data(), b.size(), 11};
        msr::scheduler::ReadRequest r2{2 * kMiB, c.data(), c.size(), 12};

        check(reader->submit(r0, &err) == msr::Status::Ok, "submit r0");
        check(reader->submit(r1, &err) == msr::Status::Ok, "submit r1");
        // depth is 2, so this blocks until a slot frees up
        check(reader->submit(r2, &err) == msr::Status::Ok, "submit r2 (bounded)");

        check(reader->wait(10, &err) == msr::Status::Ok, "wait r0");
        check(reader->wait(11, &err) == msr::Status::Ok, "wait r1");
        check(reader->wait(12, &err) == msr::Status::Ok, "wait r2");

        check(std::memcmp(a.data(), payload.data(), kMiB) == 0, "r0 bytes");
        check(std::memcmp(b.data(), payload.data() + kMiB, kMiB) == 0, "r1 bytes");
        check(std::memcmp(c.data(), payload.data() + 2 * kMiB, kMiB) == 0, "r2 bytes");

        const auto st = reader->stats();
        check(st.submitted == 3, "stats submitted == 3");
        check(st.completed == 3, "stats completed == 3");
        check(st.bytes_read == 3 * kMiB, "stats bytes_read == 3 MiB");
        check(st.peak_queue_depth <= 2, "queue depth stayed bounded");
        check(st.failed == 0, "no failed reads");

        check(reader->wait(99, &err) == msr::Status::InvalidArgument,
              "wait on unknown tag is rejected");
        check(reader->drain(&err) == msr::Status::Ok, "drain");
    }

    // --- cancellation drops queued work without hanging --------------------
    {
        std::unique_ptr<msr::scheduler::AsyncReader> reader;
        check(msr::scheduler::AsyncReader::create(file.get(), 4, reader, &err) ==
                  msr::Status::Ok, "create reader for cancel");

        std::vector<char> a(kMiB), b(kMiB);
        msr::scheduler::ReadRequest r0{0, a.data(), a.size(), 20};
        msr::scheduler::ReadRequest r1{3 * kMiB, b.data(), b.size(), 21};
        check(reader->submit(r0, &err) == msr::Status::Ok, "cancel: submit r0");
        check(reader->submit(r1, &err) == msr::Status::Ok, "cancel: submit r1");

        reader->cancel_pending();
        check(reader->drain(&err) == msr::Status::Ok, "cancel: drain returns");

        const auto st = reader->stats();
        check(st.submitted == 2, "cancel: submitted == 2");
        check(st.completed + st.dropped == 2, "cancel: every request resolved");
        check(st.dropped >= 1, "cancel: at least one request dropped");

        // A dropped tag must surface Cancelled, not block.
        const msr::Status s0 = reader->wait(20, &err);
        check(s0 == msr::Status::Cancelled || s0 == msr::Status::Ok,
              "cancel: dropped tag resolves");
    }

    // --- parallel workers: eight reads complete out of order -----------------
    {
        std::unique_ptr<msr::scheduler::AsyncReader> reader;
        check(msr::scheduler::AsyncReader::create(file.get(), 8, reader, &err, 4) ==
                  msr::Status::Ok, "create 4-worker reader");

        std::vector<std::vector<char>> dsts(8, std::vector<char>(kMiB / 2));
        for (std::size_t i = 0; i < dsts.size(); ++i) {
            msr::scheduler::ReadRequest req{i * (kMiB / 2), dsts[i].data(), dsts[i].size(),
                                           100 + i};
            check(reader->submit(req, &err) == msr::Status::Ok, "parallel: submit");
        }
        for (std::size_t i = 0; i < dsts.size(); ++i) {
            check(reader->wait(100 + i, &err) == msr::Status::Ok, "parallel: wait");
            check(std::memcmp(dsts[i].data(), payload.data() + i * (kMiB / 2),
                              dsts[i].size()) == 0, "parallel: bytes");
        }
        const auto st = reader->stats();
        check(st.completed == 8, "parallel: completed == 8");
        check(st.failed == 0, "parallel: no failures");
        check(st.peak_queue_depth <= 8, "parallel: bounded depth");
    }

    // --- layer prefetcher sequence -----------------------------------------
    {
        std::unique_ptr<msr::scheduler::AsyncReader> reader;
        check(msr::scheduler::AsyncReader::create(file.get(), 2, reader, &err) ==
                  msr::Status::Ok, "create reader for prefetch");

        msr::scheduler::LayerPrefetcher pf(reader.get(), 2);
        std::vector<char> buf0(kMiB), buf1(kMiB), buf2(kMiB);

        // first layer has no earlier prefetch: synchronous
        check(pf.schedule(0, 0, buf0.data(), kMiB, true, &err) == msr::Status::Ok,
              "prefetch: first layer sync");
        check(!pf.scheduled(0), "prefetch: sync layer is not pending");
        check(std::memcmp(buf0.data(), payload.data(), kMiB) == 0, "prefetch: buf0 bytes");

        // two-slot alternation: layer 1 into slot 1, split across two ranges
        check(pf.schedule(1, kMiB, buf1.data(), kMiB / 2, false, &err) == msr::Status::Ok,
              "prefetch: schedule layer 1 range A");
        check(pf.schedule(1, kMiB + kMiB / 2, buf1.data() + kMiB / 2, kMiB / 2, false, &err) ==
                  msr::Status::Ok, "prefetch: schedule layer 1 range B");
        check(pf.scheduled(1), "prefetch: layer 1 pending");
        check(pf.wait_layer(1, &err) == msr::Status::Ok, "prefetch: wait layer 1");
        check(std::memcmp(buf1.data(), payload.data() + kMiB, kMiB) == 0,
              "prefetch: buf1 bytes");

        check(pf.schedule(2, 2 * kMiB, buf2.data(), kMiB, false, &err) == msr::Status::Ok,
              "prefetch: schedule layer 2");
        pf.cancel_pending();
        check(!pf.scheduled(2), "prefetch: cancel clears pending");
        check(pf.wait_layer(2, &err) == msr::Status::InvalidArgument,
              "prefetch: wait after cancel is rejected");

        pf.reset();
        check(reader->stats().failed == 0, "prefetch: no failed reads");
    }

    std::remove(path.c_str());

    if (g_failures == 0) {
        std::cout << "scheduler tests passed\n";
        return 0;
    }
    std::cerr << g_failures << " scheduler check(s) failed\n";
    return 1;
}
