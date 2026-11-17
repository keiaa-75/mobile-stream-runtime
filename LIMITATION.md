# LIMITATION.md — known limits, bugs and hand-off notes

**Audience:** collaborators picking up the framework. **Project scope:** a
*framework/SDK* for storage-streamed inference, not an application. Android app
development was cancelled; the `android/` shell and APK gate were removed. The
JNI library remains only as an optional JVM binding to the same C++ engine.

Read this before claiming support for anything. The rule for this project is
unchanged: **no "supported" without an inference run and byte-identical greedy
output; no "bounded RAM" from arena counters alone.**

---

## 1. Hard scope boundaries (measured, not aspirational)

| Boundary | Reality |
| --- | --- |
| Model | `medgemma-1.5-4b-it.Q4_K_M.gguf` only (2,489,894,624 B; 34 layers; 525 MiB tied `token_embd`). Other models have never been streamed through this path. |
| Modality | Text only. `mmproj` / vision (Phase 6) not attempted; no image claim. |
| Device | One device: Redmi Turbo 3 (`24069RA21C`, `peridot`), Snapdragon 8s Gen 3, Android 15, 11.0 GiB visible RAM, ~1.4 GiB/s FUSE storage. No 6–8 GiB device, no low-RAM device, no x86_64, no iOS. |
| Cache state | Benchmarks are **warm page cache**. Cold storage could not be forced (`posix_fadvise(DONTNEED)` is a no-op on FUSE; no root). |
| Decoding | Greedy (`temp=0`, seed 42) verified. Sampled generation (temp > 0) is implemented but not output-verified. |
| Concurrency | One engine per process; one `generate()` at a time. Concurrent sessions/engines are not supported (see B6). |
| Context | 2K tokens measured. 4K/8K (Phase 5) not measured; KV cache is outside the budget. |
| Memory claim | Whole-process peak measured externally (`VmHWM`/PSS). Best native result: **1,255.8 MiB** at a 1 GiB weights budget, vs 2,645.6 MiB stock. JVM peaks (~3.3 GiB) are not comparable. |

## 2. The performance bottleneck (what collaborators should optimise)

A decoded token re-reads **~1.7 GB** of streamed weights (28 layers × 60.6 MB).
Read-side counters for 7 generated tokens at 1 GiB:

| Config | Bytes read | Compute-thread wait | Result |
| --- | ---: | ---: | ---: |
| Prefetch off | 14.71 GB | 3.82 s | 2.06 tok/s |
| Prefetch + 1 reader | 15.84 GB | 2.69 s | 2.27 tok/s |
| Prefetch + 4 readers | 15.84 GB | 1.42 s | 3.00 tok/s |

So: **I/O dominates**, and prefetch helps only by hiding it. Anything that does
not reduce bytes read or increase overlap will not move throughput much.
Measured trade-off: stock (everything resident) = 9.91 tok/s at 2,645.6 MiB;
the engine at 1 GiB = 3.00 tok/s at 1,255.8 MiB. There is no configuration
known that gives both stock speed and half the RAM.

## 3. Known bugs and sharp edges (with proposed fixes)

### B1 — Cancellation has no typed status from the provider (high)

- **Symptom:** cancelling mid-`llama_decode` makes llama return error 1; the JNI
  bridge originally surfaced a Java `IllegalStateException`. A local fix now
  checks the abort flag and treats that error as graceful termination
  (`native/jni/msr_jni.cpp`, two decode sites).
- **Root cause:** `msr_streaming_cancel()` drops queued reads and sets the
  current layer stale, but there is no "aborted" status propagated back through
  the ggml/llama hooks. The abort callback is a bool.
- **Fix:** add an explicit abort state to the provider and a typed return from
  the refill path; have the vendored hook fail with a distinguishable abort
  reason; keep the JNI workaround only as a fallback.

### B2 — `weights_no_alloc` is driven by a process-wide environment variable (high)

- **Cause:** the vendored patch uses `std::getenv("MSR_NO_ALLOC")` inside
  `llama_model_loader::load_tensors`, and the JNI binding calls `setenv` before
  loading. It is global, not per-engine, and must be set before first use.
- **Risk:** any embedding that loads a second model, or sets the env late, gets
  inconsistent behaviour. Thread-safe reads of `getenv` are not guaranteed.
- **Fix:** add a real parameter/flag to `llama_model_params` in the vendored
  patch (e.g. `external_weights` plus an opaque provider handle) and remove the
  env dependency from the loader. Keep `MSR_NO_ALLOC` only as a CLI fallback.

### B3 — Provider is a global singleton: `configure` is first-wins (high)

- `msr_external_configure()` returns immediately once `g_initialized` is set;
  all engines in a process share one arena, one streamed-layer map and one
  `g_mutex`. Two engines would alias the same tensor pointers.
- **Fix:** make the provider instance-based (context handle passed via the
  hooks/backing struct), or explicitly document and enforce one engine per
  process in the framework API.

### B4 — Per-layer refill depends on node-name suffixes (medium)

- Refills trigger on the `-<layer>` suffix of ggml node names (`norm-33`,
  `Qcur-7`). Fused ops absorb the tensor pointer registered by
  `msr_streaming_layer_first_node`, which is why pointer identity was abandoned.
- **Risk:** a llama.cpp/ggml update that renames or further fuses nodes silently
  disables refills — the model then computes from stale or uninitialised slot
  data instead of crashing.
- **Fix:** pin the vendored commit (`ef6bc6f`, already done), add a CI check that
  asserts ≥26 distinct refills for the reference run, and eventually register
  a stable layer id on the graph nodes in the patch itself.

### B5 — Dangling pointer in provider layout (low, latent UB)

- `UnitLayout::unit = &u` points into the local `units` vector inside
  `initialize()`; that vector dies at function exit. The field is currently
  never read, so there is no observed effect.
- **Fix:** delete the `unit` field (and the assignment), or store units by value
  in a member vector with stable ownership.

### B6 — Refill/prefetch slot safety assumes sequential layer execution (medium)

- Layer `k+1` is prefetched into the slot last used by layer `k-1`. This is safe
  only because ggml-cpu currently finishes layer `k-1` before layer `k` starts.
- **Risk:** any future graph-level parallelism across layers introduces a
  write/compute race (no pin/unpin is actually used; `MemoryManager` supports
  pinning but the provider does not call it).
- **Fix:** pin the target buffer while its layer is active; assert exclusive
  ownership before a refill; document the sequential-execution precondition.

### B7 — Read counters are misleading for parallel readers (low)

- `AsyncReader::stats().io_ns` **sums** worker durations. With 4 readers it
  reads 7.36 s where the wall-clock read phase was ~2 s. `wait_ns` (main-thread
  blocking) is the meaningful metric.
- **Fix:** record wall-clock spans per request and report both aggregate and
  max/mean; rename the field or document it in the stats header.

### B8 — Prefetch's extra slot forces 1–2 more layers to stream (medium)

- Second 60.6 MB slot → planner streams 28 layers instead of 26 at 1 GiB
  (+7.7 % bytes read). At 1 GiB this is still a net win; at some budgets it may
  not be. Prompt throughput with 4 readers (15.15 tok/s) is slightly below the
  1-reader run (16.62 tok/s) because prompt compute also uses 4 threads.
- **Fix:** A/B slot count (1/2/3) and reader count (2/4/8) per device; consider
  partial (tensor-granular) double buffering if the extra 60 MB is too costly.

### B9 — Two-slot layout assumes equal layer sizes (low)

- All slots are sized to the max layer bytes; if a model has uneven layers,
  memory is wasted and the residency plan is pessimistic.
- **Fix:** per-slot sizes or a sub-arena of independently sized buffers.

### B10 — No cold-storage measurement (medium)

- Benchmarks are warm-cache only; the larger 1.5 GiB arena appears to squeeze
  page cache (9.70 GB read at 2.97 s ≈ 3.3 GB/s vs 4.3 GB/s at 1 GiB), which is
  why 1.5 GiB without prefetch is barely faster than 1 GiB.
- **Fix:** root/namespace tools or a FUSE backend that supports cache control;
  add a cold/warm column to future A/B tables. Do not extrapolate to cold.

### B11 — Thermal drift confounds naive A/B (medium)

- Device reaches 74 °C; non-interleaved runs drift up to ~30 %. The A/B driver
  interleaves configs and 4 s cooldown, but spreads remain large for prompt
  tokens (3–6 tok/s).
- **Fix:** thermal-controlled bench (Phase 5): enforce a start temperature,
  longer cooldown, log zone temperatures per run, discard out-of-band runs.

### B12 — Provider logs unconditionally to stderr (low)

- `msr_external_summary()` is registered with `atexit` and prints init/refill
  statistics on every run. Harmless in the CLI, noisy for an embedded library.
- **Fix:** verbosity callback or `MSR_VERBOSE=0`; route logs through the host.

### B13 — NDK/OpenSSL dependency (low after app cancellation)

- `native/src/model/integrity.cpp` uses OpenSSL for SHA-256. The Android NDK has
  no OpenSSL. This only matters if the framework is consumed from an NDK build;
  the Termux/JVM path links OpenSSL fine.
- **Fix:** pluggable hash backend (BoringSSL or a small self-contained SHA-256).

### B14 — 16 KB page size and non-reference ABIs untested (medium)

- Arena alignment is 4096 B and ggml tensors 64 B; both are compatible with
  16 KB pages, but ELF load segments need NDK r27+ and
  `-Wl,-z,max-page-size=16384`. No measurement outside arm64 Android 15.

### B15 — Checksums are recorded but not enforced at runtime (low)

- GGUF SHA-256 values were computed on-device and are in `DEVELOPMENT_PHASES.md`
  / `docs/model-inventory/`; cross-check against upstream `SHA256SUMS` is still
  open, and the streamed loader does not verify the file before use.

## 4. Missing work, in suggested order

1. **Framework extraction:** move the engine lifecycle out of the JNI file into
   `native/src/engine/` behind a C ABI (`msr_engine_*`), with JNI/CLI as thin
   clients. Add an installable CMake package and a minimal C example.
2. Fix B1–B3 (cancellation status, per-load flag, instance-based provider).
3. A/B reader count (2/4/8) and slots (2/3) with the interleaved driver;
   publish medians + spread. No claims before that.
4. Streaming `token_embd` rows (525 MiB) if lower RAM is the priority; this is
   the largest remaining memory lever, but changes Gemma-3 embedding/output
   plumbing in the vendored patch.
5. Packed per-layer stream file (one aligned read instead of 13) if speed at
   the same RAM is the priority.
6. Phase 5 text evaluation at 2K/4K/8K, sampled generation, corruption/thermal
   failure tests, and honest limitations update.
7. Multimodal (Phase 6) only after text is closed.

## 5. Reproduction / verification

```sh
cd ~/mobile-stream-runtime
ctest --test-dir native/build --output-on-failure        # 3 suites
scripts/jni_smoke.sh                                   # CLI + both cancel paths
python3 scripts/ab_prefetch.py --model ... --runs 5 \
  --out-dir benchmarks/results/<new-sweep>
```

Regenerate the vendored patch after touching `third_party/llama.cpp`:

```sh
git -C third_party/llama.cpp diff > patches/0001-msr-external-buffer-hook.patch
git apply --check --reverse --directory=third_party/llama.cpp \
  patches/0001-msr-external-buffer-hook.patch
```

Reference evidence: `BENCHMARKS.md` §§10–12,
`benchmarks/REDMI_TURBO_3_BENCHMARK.md`, `updates.md` §§8–10,
`docs/PORTABILITY.md`, `docs/native-api.md`.
