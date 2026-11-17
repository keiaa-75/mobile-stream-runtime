# Framework guide

The deliverable is a native SDK, not an end-user program. The public surface is
a small C API in `native/include/msr/engine/engine.h`; llama.cpp, ggml, the
provider and the scheduler are private implementation details.

## C API

```c
msr_result msr_engine_create(msr_engine** out);
void       msr_engine_destroy(msr_engine*);
msr_result msr_engine_load(msr_engine*, const msr_engine_config*);
msr_result msr_engine_generate(msr_engine*, const char* prompt,
                               uint32_t max_tokens, float temperature, int32_t seed,
                               msr_token_fn on_token, void* user_data);
msr_result msr_engine_cancel(msr_engine*);
msr_result msr_engine_get_stats(const msr_engine*, msr_engine_stats* out);
const char* msr_engine_last_error(const msr_engine*);
const char* msr_engine_version(void);
```

- `msr_engine_config` carries `model_path`, `budget_bytes`, `context_tokens`,
  `threads`, `read_threads` and `prefetch`. Zero fields take the reference
  defaults (2 GiB, 2048, 4, 1, 0).
- `on_token(piece, len, user_data)` is called once per generated token; returning
  non-zero stops generation and `generate()` returns `MSR_ABORTED`.
- `cancel()` is thread-safe; abort takes effect at the next token/prompt-chunk
  boundary and via llama.cpp's CPU abort callback.
- `msr_engine_stats` includes prompt/generated tokens, TTFT, prompt/generation
  time, weight-budget/resident/peak bytes, read bytes, streamed-layer count,
  slot and reader counts, plus whole-process `VmHWM`/`VmRSS` and thermal reading.
  The arena counters are **not** a memory-limit claim on their own.
- One model/context per engine and **one engine per process** (provider is a
  singleton). See `LIMITATION.md` B2/B3.

No backend types cross the boundary. The prompt is formatted with the model's
chat template (Gemma-3 turns) before tokenization.

## Build the SDK

```sh
cd ~/mobile-stream-runtime

# 1. llama.cpp as a PIC static library (the SDK links it into one shared object)
cmake -S third_party/llama.cpp -B build-static -G Ninja \
  -DBUILD_SHARED_LIBS=OFF -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF \
  -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF \
  -DGGML_OPENMP=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target llama -j4

# 2. SDK: libmsr_engine.so, msr-cli, installable CMake package
cmake -S sdk -B build-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$PWD/build-sdk/prefix
cmake --build build-sdk -j4
ctest --test-dir build-sdk --output-on-failure   # model-dependent, skipped if asset absent
cmake --install build-sdk
```

Installed layout: `include/msr/engine/engine.h` (plus internal headers),
`lib/libmsr_engine.so`, `lib/cmake/msr/msrConfig.cmake`, `bin/msr-cli`.

## Consume from another project

```cmake
find_package(msr REQUIRED CONFIG)
add_executable(my_host main.cpp)
target_link_libraries(my_host PRIVATE msr::engine)
```

See `sdk/examples/` for a complete host program. Run it with
`LD_LIBRARY_PATH=<prefix>/lib` on systems that do not search that path.

## Reference CLI

```sh
LD_LIBRARY_PATH=$PWD/build-sdk/lib build-sdk/bin/msr-cli \
  /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf \
  "The capital of France is" 8 1024 4 4 1
# args after prompt: max_tokens budget_mb threads read_threads prefetch
# optional: --cancel-after-tokens N | --cancel-after-ms M
```

## Verified on the reference device

| Check | Result |
| --- | --- |
| Output vs stock | `thought\n1.  **Identify` — byte-identical |
| Budget | 1 GiB; peak weight bytes 1,020,289,024; 28/34 layers streamed; 2 slots |
| Whole-process peak | VmHWM ~1,302 MiB (native CLI reference: 1,255.8 MiB) |
| Cancel via callback | `MSR_ABORTED`, 3 tokens generated |
| Cancel from another thread mid-decode | `MSR_ABORTED`, 1 token generated |
| Installed package | `find_package(msr)` + `msr::engine` example built and ran |

A real bug was found and fixed while building the SDK: llama.cpp's default
`load_mode = AUTO` maps the entire GGUF file, adding ~2 GiB to `VmHWM`. The
engine now sets `load_mode = NONE` and `lazy_mode = OFF`, matching the
reference CLI's `--load-mode none`. Before that fix the same run measured
~3.2 GiB VmHWM despite the identical arena cap.

Evidence: `BENCHMARKS.md` §12; caveats: `LIMITATION.md`.
