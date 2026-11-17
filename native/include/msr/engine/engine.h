#ifndef MSR_ENGINE_H
#define MSR_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(MSR_BUILDING_LIBRARY)
#    define MSR_API __declspec(dllexport)
#  else
#    define MSR_API __declspec(dllimport)
#  endif
#else
#  define MSR_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque engine: one model + one context per instance.
 *  The provider is a process-wide singleton, so only one instance per process
 *  is supported today. See LIMITATION.md (B3). */
typedef struct msr_engine msr_engine;

typedef enum msr_result {
    MSR_OK = 0,
    MSR_INVALID_ARGUMENT = 1,
    MSR_NOT_LOADED = 2,
    MSR_ALREADY_LOADED = 3,
    MSR_IO_ERROR = 4,
    MSR_MODEL_ERROR = 5,
    MSR_CONTEXT_ERROR = 6,
    MSR_ABORTED = 7,
    MSR_INTERNAL_ERROR = 8
} msr_result;

typedef struct msr_engine_config {
    const char* model_path;      /* borrowed for the duration of load() */
    uint64_t    budget_bytes;    /* weights arena cap, 0 = default 2 GiB */
    uint32_t    context_tokens;  /* 0 = 2048 */
    uint32_t    threads;         /* compute threads, 0 = 4 */
    uint32_t    read_threads;    /* storage prefetch workers, 0 = 1 */
    int         prefetch;        /* 0 = serial refill, 1 = two-slot prefetch */
} msr_engine_config;

/** Called once per generated token. Return non-zero to stop generation. */
typedef int (*msr_token_fn)(const char* piece, size_t len, void* user_data);

typedef struct msr_engine_stats {
    uint64_t prompt_tokens;
    uint64_t generated_tokens;
    uint64_t ttft_ms;
    uint64_t prompt_ms;
    uint64_t generate_ms;

    uint64_t budget_bytes;
    uint64_t resident_weight_bytes;
    uint64_t peak_weight_bytes;
    uint64_t read_bytes;
    uint32_t streamed_layers;
    uint32_t stream_slots;
    uint32_t prefetch;
    uint32_t read_threads;

    /* Whole-process values from /proc/self/status (-1 if unavailable). These
     * are measurements, never a substitute for the arena cap. */
    uint64_t peak_vm_hwm_kb;
    uint64_t peak_vm_rss_kb;
    int32_t  thermal_c;
} msr_engine_stats;

MSR_API msr_result msr_engine_create(msr_engine** out);
MSR_API void       msr_engine_destroy(msr_engine* engine);

/** Loads the model and binds its tensors directly to the arena. Single-shot. */
MSR_API msr_result msr_engine_load(msr_engine* engine, const msr_engine_config* config);

/** Greedy (temperature <= 0) or sampled generation with per-token callback.
 *  Returns MSR_OK on normal completion (including end-of-generation) and
 *  MSR_ABORTED when cancelled. Not thread-safe against destroy(). */
MSR_API msr_result msr_engine_generate(msr_engine* engine,
                                       const char* prompt,
                                       uint32_t max_tokens,
                                       float temperature,
                                       int32_t seed,
                                       msr_token_fn on_token,
                                       void* user_data);

/** Thread-safe; abort takes effect at the next token/prompt-chunk boundary. */
MSR_API msr_result msr_engine_cancel(msr_engine* engine);

MSR_API msr_result msr_engine_get_stats(const msr_engine* engine, msr_engine_stats* out);
MSR_API const char* msr_engine_last_error(const msr_engine* engine);
MSR_API const char* msr_engine_version(void);

#ifdef __cplusplus
}
#endif
#endif  /* MSR_ENGINE_H */
