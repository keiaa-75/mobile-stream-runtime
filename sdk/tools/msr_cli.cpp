// Reference host program for the C engine API. Intentionally small: no
// framework-specific dependencies beyond the installed SDK.
#include <msr/engine/engine.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

struct SinkState {
    std::atomic<int> seen{0};
    int stop_after = 0;
};

int on_token(const char* piece, size_t len, void* user) {
    auto* state = static_cast<SinkState*>(user);
    std::fwrite(piece, 1, len, stdout);
    std::fflush(stdout);
    const int n = ++state->seen;
    return state->stop_after > 0 && n >= state->stop_after;
}

int int_flag(int argc, char** argv, const char* name, int fallback) {
    for (int i = 0; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], name) == 0) return std::atoi(argv[i + 1]);
    }
    return fallback;
}

const char* result_name(msr_result r) {
    switch (r) {
        case MSR_OK: return "OK";
        case MSR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case MSR_NOT_LOADED: return "NOT_LOADED";
        case MSR_ALREADY_LOADED: return "ALREADY_LOADED";
        case MSR_IO_ERROR: return "IO_ERROR";
        case MSR_MODEL_ERROR: return "MODEL_ERROR";
        case MSR_CONTEXT_ERROR: return "CONTEXT_ERROR";
        case MSR_ABORTED: return "ABORTED";
        case MSR_INTERNAL_ERROR: return "INTERNAL_ERROR";
    }
    return "UNKNOWN";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr,
            "usage: msr-cli <model.gguf> <prompt> [max_tokens] [budget_mb] "
            "[threads] [read_threads] [prefetch] "
            "[--cancel-after-tokens N] [--cancel-after-ms M]\n");
        return 2;
    }

    const char* model = argv[1];
    const char* prompt = argv[2];
    const uint32_t max_tokens = argc > 3 ? static_cast<uint32_t>(std::atoi(argv[3])) : 8;
    const uint64_t budget_mb = argc > 4 ? static_cast<uint64_t>(std::atoll(argv[4])) : 1024;
    const uint32_t threads = argc > 5 ? static_cast<uint32_t>(std::atoi(argv[5])) : 4;
    const uint32_t readers = argc > 6 ? static_cast<uint32_t>(std::atoi(argv[6])) : 4;
    const int prefetch = argc > 7 ? std::atoi(argv[7]) : 1;

    msr_engine* engine = nullptr;
    msr_result r = msr_engine_create(&engine);
    if (r != MSR_OK) {
        std::fprintf(stderr, "create failed: %s\n", result_name(r));
        return 1;
    }

    msr_engine_config cfg{};
    cfg.model_path = model;
    cfg.budget_bytes = budget_mb << 20;
    cfg.context_tokens = 2048;
    cfg.threads = threads;
    cfg.read_threads = readers;
    cfg.prefetch = prefetch;

    const auto t0 = std::chrono::steady_clock::now();
    r = msr_engine_load(engine, &cfg);
    if (r != MSR_OK) {
        std::fprintf(stderr, "load failed: %s (%s)\n", result_name(r),
                     msr_engine_last_error(engine));
        msr_engine_destroy(engine);
        return 1;
    }

    SinkState sink;
    sink.stop_after = int_flag(argc, argv, "--cancel-after-tokens", 0);
    const int cancel_ms = int_flag(argc, argv, "--cancel-after-ms", 0);
    std::thread canceller;
    if (cancel_ms > 0) {
        canceller = std::thread([engine, cancel_ms] {
            std::this_thread::sleep_for(std::chrono::milliseconds(cancel_ms));
            std::fprintf(stderr, "cancelling after %d ms\n", cancel_ms);
            msr_engine_cancel(engine);
        });
    }

    r = msr_engine_generate(engine, prompt, max_tokens, 0.0f, 42, on_token, &sink);
    if (canceller.joinable()) canceller.join();
    std::fputc('\n', stdout);

    msr_engine_stats st{};
    msr_engine_get_stats(engine, &st);
    const double gen_tps = st.generate_ms ? st.generated_tokens * 1000.0 / st.generate_ms : 0.0;

    std::fprintf(stderr,
        "result=%s version=%s\n"
        "prompt=%llu gen=%llu ttft=%llums prompt_ms=%llu gen_ms=%llu (%.2f tok/s)\n"
        "weight_budget=%llu resident=%llu peak_weight=%llu streamed=%u slots=%u "
        "prefetch=%u readers=%u read_bytes=%llu\n"
        "VmHWM=%llu KiB VmRSS=%llu KiB thermal=%d C wall_ms=%lld\n",
        result_name(r), msr_engine_version(),
        (unsigned long long) st.prompt_tokens, (unsigned long long) st.generated_tokens,
        (unsigned long long) st.ttft_ms, (unsigned long long) st.prompt_ms,
        (unsigned long long) st.generate_ms, gen_tps,
        (unsigned long long) st.budget_bytes, (unsigned long long) st.resident_weight_bytes,
        (unsigned long long) st.peak_weight_bytes, st.streamed_layers, st.stream_slots,
        st.prefetch, st.read_threads, (unsigned long long) st.read_bytes,
        (unsigned long long) st.peak_vm_hwm_kb, (unsigned long long) st.peak_vm_rss_kb,
        st.thermal_c,
        (long long) std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count());

    msr_engine_destroy(engine);
    return r == MSR_ABORTED ? 3 : (r == MSR_OK ? 0 : 1);
}
