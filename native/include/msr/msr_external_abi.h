#pragma once

// Stable C ABI between the streaming provider and embedders (the JNI bridge
// here; anything else that links the provider statically).
//
// Rules:
//   - configure() must run before the first model load; it never changes a
//     session that is already initialised.
//   - Environment variables remain the fallback (MSR_*), so the LD_PRELOAD
//     path and the embedded path behave the same.
//   - No llama/ggml types cross this boundary.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msr_external_stats {
    unsigned long long budget_bytes;
    unsigned long long resident_weight_bytes;
    unsigned long long stream_buffer_bytes;
    unsigned long long peak_weight_bytes;
    unsigned long long read_bytes;
    unsigned long long read_io_ns;
    unsigned long long read_wait_ns;
    unsigned long long reads_submitted;
    unsigned long long reads_completed;
    unsigned long long reads_failed;
    unsigned long long reads_dropped;
    int streamed_layers;
    int stream_slots;
    int prefetch;
    int read_threads;
} msr_external_stats;

/// Configures the provider. `budget_bytes == 0`, `prefetch < 0` and
/// `read_threads <= 0` mean "leave to the environment/default".
void msr_external_configure(const char* model_path, unsigned long long budget_bytes,
                            int prefetch, int read_threads);

/// Fills `out`. Safe to call before initialisation (zeros) and after a load.
void msr_external_get_stats(msr_external_stats* out);

#ifdef __cplusplus
}
#endif
