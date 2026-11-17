// Minimal host program: only the installed C API header is needed.
#include <msr/engine/engine.h>

#include <cstdio>
#include <cstdlib>

static int emit(const char* piece, size_t len, void*) {
    std::fwrite(piece, 1, len, stdout);
    std::fflush(stdout);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: minimal <model.gguf> <prompt> [tokens]\n");
        return 2;
    }
    msr_engine* engine = nullptr;
    if (msr_engine_create(&engine) != MSR_OK) return 1;

    msr_engine_config cfg{};
    cfg.model_path = argv[1];
    cfg.budget_bytes = 1ull << 30;
    cfg.context_tokens = 2048;
    cfg.threads = 4;
    cfg.read_threads = 4;
    cfg.prefetch = 1;

    msr_result r = msr_engine_load(engine, &cfg);
    if (r == MSR_OK) {
        r = msr_engine_generate(engine, argv[2], argc > 3 ? std::atoi(argv[3]) : 8,
                                0.0f, 42, emit, nullptr);
        std::fputc('\n', stdout);
    }
    msr_engine_stats st{};
    msr_engine_get_stats(engine, &st);
    std::fprintf(stderr, "result=%d peak_weight=%llu streamed=%u VmHWM=%llu KiB\n",
                 static_cast<int>(r), (unsigned long long) st.peak_weight_bytes,
                 st.streamed_layers, (unsigned long long) st.peak_vm_hwm_kb);
    if (r != MSR_OK) std::fprintf(stderr, "error: %s\n", msr_engine_last_error(engine));
    msr_engine_destroy(engine);
    return r == MSR_OK ? 0 : 1;
}
