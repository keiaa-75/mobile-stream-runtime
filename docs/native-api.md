# Native API

Two layers, deliberately separated:

1. **Provider C ABI** (`native/include/msr/msr_external_abi.h`) — configuration
   and telemetry for the streaming engine.
2. **JNI surface** (`native/jni/msr_jni.cpp`) — an optional JVM binding. It is
   not the framework's primary API (a C engine API is the next step). No
   llama.cpp, ggml or provider type crosses it.

## Provider C ABI

```c
void msr_external_configure(const char *model_path,
                            unsigned long long budget_bytes,
                            int prefetch, int read_threads);
void msr_external_get_stats(msr_external_stats *out);
```

- `configure` must be called **before the first model load**. Zero budget,
  negative `prefetch` and non-positive `read_threads` mean "keep the
  environment/default value". The `LD_PRELOAD` path still uses `MSR_*` env vars.
- `get_stats` returns budget/resident/peak weight bytes, streamed-layer count,
  slot count, prefetch/read-thread flags and async-read counters. It is safe
  before initialisation (zeros) and holds the provider mutex.
- Hooks implemented by the provider and resolved by the vendored llama.cpp:
  `msr_streaming_arena`, `msr_streaming_bind_tensor` (plus the legacy
  `msr_streaming_supply_tensor`), `msr_streaming_prepare_layer`,
  `msr_streaming_layer_first_node`, `msr_streaming_layer_boundary`,
  `msr_streaming_node_start`, `msr_streaming_node_done`.
- `msr_streaming_cancel()` drops queued prefetches and marks the streamed-layer
  state stale. In-flight reads finish. It never frees a pinned buffer.

## JNI surface

Class `dev.msr.engine.MsrEngine` (Java mirror in
`native/jni/java/dev/msr/engine/MsrEngine.java`).

| Method | Meaning |
| --- | --- |
| `MsrEngine()` | Allocates a session handle. Loads `libmsr_jni` in a static initialiser. |
| `load(path, budgetBytes, ctxTokens, threads, prefetch, readThreads)` | One-time model load. Binds all weights to the arena; `--no-repack` equivalent is set internally. Throws `IllegalStateException` on failure. |
| `generate(prompt, maxTokens, temperature, seed, sink)` | Applies the model chat template, tokenizes, prompts, then samples. Sink's `onToken(String)Z` is called per token; returning `false` stops generation. Blocks until done/cancelled. |
| `cancel()` | Thread-safe. Sets the abort flag consumed by llama.cpp's CPU abort callback and by the token loop; also calls `msr_streaming_cancel()`. |
| `stats()` | Snapshot of tokens, TTFT/prompt/generation milliseconds, weight bytes, streamed layers, read bytes, whole-process `VmHWM`/`VmRSS` and thermal reading (`-1` if unavailable). |
| `lastError()` | Last native error string. |
| `close()` / `AutoCloseable` | Frees the session. Call once; do not call while `generate` is running. |

Threading and lifecycle rules:

- One engine instance = one model + one context. `load` is single-shot. `close`
  destroys the model and KV cache.
- `generate` serialises per engine (internal mutex). The caller should run it on
  a background thread; the JNI binding is safe only for one engine per process
  (see `LIMITATION.md`, B3/B6).
- Cancellation granularity is one token (or one 512-token prompt chunk). The
  abort callback also interrupts a CPU `llama_decode` in progress.
- Errors are Java `IllegalStateException`; native code never throws across JNI.
- `stats()` reads `/proc/self/status` for whole-process peaks — the arena's own
  counters are reported separately, never as a memory-limit claim.

## Build

```sh
# 1. static llama.cpp (fPIC is required: the JNI .so links it as one unit)
cd ~/mobile-stream-runtime
cmake -S third_party/llama.cpp -B build-static -G Ninja \
  -DBUILD_SHARED_LIBS=OFF -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF \
  -DGGML_OPENMP=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target llama -j4

# 2. JNI library (provider + core compiled in; no LD_PRELOAD)
cmake -S native/jni -B native/build/jni-cmake -G Ninja
cmake --build native/build/jni-cmake -j4
# -> native/build/jni/libmsr_jni.so

# 3. Java smoke CLI (headless check of the binding)
mkdir -p native/jni/classes
javac -d native/jni/classes native/jni/java/dev/msr/engine/*.java
java -Djava.library.path=native/build/jni -cp native/jni/classes \
  dev.msr.engine.MsrCli <model.gguf> "The capital of France is" 8 1024 4 4 1
```

Flags after the prompt: `maxTokens budgetMb threads readThreads prefetch`, then
optional `--cancel-after-tokens N` or `--cancel-after-ms M` to exercise both
cancellation paths.

## Verification status

- The Java CLI path is run end-to-end on the reference device (see
  `BENCHMARKS.md` §12): load, greedy generation, per-token sink, stats, and both
  cancellation paths.
- Android application development was **cancelled**; no APK or UI is part of
  this project. The JNI library is kept as an optional framework binding.
  Known limitations and collaborator fixes are in `LIMITATION.md`.
