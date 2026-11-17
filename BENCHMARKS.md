# BENCHMARKS

All numbers below were measured on the reference device on 2026-11-16.
Reproduce with the scripts in `scripts/`. Raw output is in
`benchmarks/results/`.

## Test conditions (must be quoted with any number)

| Field | Value |
| --- | --- |
| Device | 24069RA21C (`peridot`), aarch64, 8 cores |
| Android | 15 (SDK 35), kernel 6.1.118-android14 |
| RAM | 11,542,640 kB total; MemAvailable 4,572,176 kB at test time |
| Swap | 8,388,604 kB (`zram`-backed) |
| Storage | `/storage/emulated/0` (FUSE), 227 GB, 23 GB free |
| Model | `medgemma-1.5-4b-it.Q4_K_M.gguf`, 2,489,894,624 B |
| Runtime | none yet (storage probes only) |
| Shell | Termux aarch64, Python 3.14.10 |

---

## 1. Storage read throughput

`python3 scripts/io_bench.py … --span-mib 1024` (1 GiB span, 8 MiB chunks).

### Sequential

| Pass | MiB/s |
| --- | ---: |
| pass 1 (as cold as achievable) | 1419.9 |
| pass 2 (warm page cache) | 1430.6 |

### Random aligned `pread`

| Request | Median MiB/s | Min | Max | p95 latency |
| ---: | ---: | ---: | ---: | ---: |
| 1 MiB | 3465.7 | 1294.8 | 4468.0 | 0.40 ms |
| 4 MiB | 1559.2 | 1330.5 | 1791.0 | 3.04 ms |
| 16 MiB | 1390.6 | 1277.2 | 1506.6 | 13.07 ms |
| 64 MiB | 1378.9 | 1287.3 | 1527.9 | 51.84 ms |

4 KiB random reads: median 0.00 ms, p95 0.01 ms (page-cache hits; this
figure is **not** usable as a cold-storage latency).

### Cold-ish retest (page cache deliberately displaced)

Flushed 9.28 GiB through the file, then re-measured:

| Metric | MiB/s |
| --- | ---: |
| Sequential 512 MiB span | 1411.0 |
| Random 16 MiB, median | 1338.0 |
| Random 16 MiB, min / max | 1297.3 / 1500.6 |

**Interpretation:** ~1.3–1.5 GiB/s sustained is available from this FUSE
mount even when the cache is displaced. Random 16 MiB reads cost essentially
nothing versus sequential — good news for layer-wise streaming.

**Caveat:** without root we cannot drop the page cache, so a true cold UFS
number is not established. Treat 1.4 GiB/s as an optimistic-but-plausible
upper bound.

### Direct I/O

`O_DIRECT` opens and 4 KiB reads **succeed** on this mount (verified). That
matters: it gives the runtime a page-cache-bypassing path with predictable
RSS, which is what a bounded-buffer design wants.

---

## 2. mmap residency

`python3 scripts/mmap_probe.py MODEL`

### Mode A — map whole file, touch every 4 KiB page

| Metric | Value |
| --- | --- |
| Mapped span | 2375 MiB |
| Peak RSS during touch | 2319.0 MiB |
| Peak VmHWM | 2390.2 MiB |
| Resident / span | 97.7% |
| Scan rate | 1756 MiB/s |

RSS rose monotonically with the touched offset and finished at essentially
the entire file. **Memory-mapping the model does not keep RAM bounded.**

### Mode B — 256 MiB sliding window + `MADV_DONTNEED`

| Metric | Value |
| --- | --- |
| Peak RSS | 16.4 MiB |
| Peak VmHWM | 272.1 MiB |
| Resident / span | 0.7% |
| Scan rate | 5106 MiB/s |

A bounded window holds only ~1 window of pages. The high-water mark (272 MiB
for a 256 MiB window) is the real cost to budget for.

**Phase 0 conclusion:** residency is a function of *how* the file is
accessed, not of mmap itself. Windowed access + explicit `MADV_DONTNEED`
gives a hard cap; naive mmap gives none.

---

## 3. KV cache model (computed from GGUF metadata)

gemma3: 34 layers, 4 KV heads, key_length 256, value_length 256.

| Context | KV f16 | KV q8_0 (est.) |
| ---: | ---: | ---: |
| 2,048 | 272.0 MiB | 136 MiB |
| 4,096 | 544.0 MiB | 272 MiB |
| 8,192 | 1.062 GiB | 544 MiB |
| 16,384 | 2.125 GiB | 1.09 GiB |
| 32,768 | 4.250 GiB | 2.18 GiB |

136 KiB **per token** of context. At 8K this alone is 46% of the model file
size and is unaffected by weight streaming.

---

## 4. Dense-streaming throughput ceiling

The file is 2.313 GiB of payload and every token needs every layer.

| Effective read rate | Ceiling (full stream, no cache) |
| ---: | ---: |
| 0.5 GB/s | 0.20 tok/s |
| 1.0 GB/s | 0.40 tok/s |
| 1.4 GB/s (measured) | 0.60 tok/s |
| 2.0 GB/s | 0.81 tok/s |
| 3.0 GB/s | 1.21 tok/s |

At the measured 1.4 GiB/s, **full layer streaming of MedGemma 4B is a
~0.6 tok/s engine before any compute cost.** This is the single most
important number in Phase 0.

---

## 5. Conventional llama.cpp baseline (stock loader, no streaming)

Build and invocation:

```
llama.cpp @ ef6bc6f367082338006c8379bc7bcab3e1584766
cmake -DCMAKE_BUILD_TYPE=Release -DGGML_NATIVE=ON -DGGML_OPENMP=OFF \
      -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_SERVER=OFF
CPU features: NEON, FMA, FP16_VA, MATMUL_INT8, DOTPROD, REPACK
binary: llama-completion
prompt: 24 tokens; -n 32 -c 2048 -b 256 -ub 256 -t 4 -st --no-warmup
```

Peak memory is sampled with `/proc/<pid>/status` (VmHWM) and
`/proc/<pid>/smaps_rollup` (Pss) by `scripts/run_measured.py`.

| Config | VmHWM | PSS | prompt tok/s | generation tok/s (runs) |
| --- | ---: | ---: | ---: | --- |
| `--load-mode auto` (repack on) | 4,984 MiB | 4,981 MiB | 14.9 / 31.2 / 13.6 | 11.0 / 9.5 / 7.0 |
| `--load-mode auto --no-repack` | 2,624 MiB | 2,621 MiB | 19.8 / 23.2 / 17.3 | 12.4 / 12.4 / 6.4 |
| `--load-mode none --no-repack` | 2,617 MiB | 2,614 MiB | 23.1 / 23.6 / 12.6 | 12.5 / 12.4 / 6.1 |
| `--load-mode dio --no-repack` | 2,624 MiB | 2,621 MiB | 17.7 / 21.3 / 23.4 | 10.8 / 12.5 / 12.5 |
| ctx 4096, `-ctk/-ctv q8_0`, no-repack | 2,563 MiB | 2,560 MiB | 19.3 | 5.7 (single run, hot device) |

### Findings

1. **Weight repacking nearly doubles RSS.** `VmHWM 4,984 MiB` with repack
   vs `2,624 MiB` without — a **+2.36 GiB** penalty for the CPU repack buffer
   type (`ggml-cpu/repack.cpp`, `GGML_USE_CPU_REPACK`). For a project whose
   whole premise is bounding RAM, `--no-repack` is mandatory.
2. **Baseline is already 2.62 GiB** for a 2.49 GB file: 2.31 GiB weights +
   272 MiB f16 KV at 2K + graph/activations. Any "streaming saves memory"
   claim must beat this, not an imaginary 100 MiB baseline.
3. **Load modes barely matter once repack is off.** `auto`, `none` and `dio`
   all land at 2.6 GiB / ~12 tok/s. `dio` works on this FUSE mount and gives
   a deterministic heap-resident model.
4. **Thermal drift invalidates naive A/B.** Temperatures rose 53.6 → 62.3 °C
   in a single run and reached **74 °C** after a short sequence; generation
   rate fell from ~12.4 to ~3–6 tok/s in later runs. The apparent
   "repack is slower" result is **not conclusive** — compare with
   interleaved, cooled runs before drawing that conclusion.
5. First-token latency was not isolated; "prompt tok/s" above is the
   prompt-eval rate for a 24-token prompt (~1.0–1.8 s total).

### Still not measured

- Isolated TTFT and page-fault rate during generation.
- Battery mAh/1K tokens and sustained-load decay curve with cooling control.
- CPU utilization per thread count / affinity.
- Vision projector memory during an actual image run.
- Interleaved, temperature-controlled repack A/B.

No memory or speed claim should be quoted without the thermal context above.

---

## 7. External-buffer hook — one-layer correctness proof (2026-11-17)

This closes the first GO condition in `FEASIBILITY.md` §7: *can llama.cpp
execute a layer with weights placed in an external buffer?*

- Patch: `patches/0001-msr-external-buffer-hook.patch` adds a weak symbol
  `msr_streaming_supply_tensor()` to `llama_model_loader::load_all_data()`.
  Stock builds leave it unresolved, so behaviour is unchanged unless a provider
  is linked/preloaded.
- Provider: `native/build/libmsr-external.so` reads the exact GGUF byte ranges
  via the Phase 1 reader and supplies every `blk.0.*` tensor from its own
  aligned buffers.
- Command (stock and hooked runs identical apart from `LD_PRELOAD`/env):

```bash
ASSET=/storage/emulated/0/Download/gemma
build/bin/llama-completion -m "$ASSET/medgemma-1.5-4b-it.Q4_K_M.gguf" \
  -p 'The capital of France is' -n 16 -s 42 --temp 0 -c 2048 -t 4 \
  --no-repack -lm none
```

- Result: **13 tensors** of `blk.0` (≈57 MB total) were supplied externally;
  greedy output was **byte-identical** to the stock run (`diff -q` passed).
- Raw logs: `benchmarks/results/external-proof-stock.txt`,
  `external-proof-preload.txt`, `external-proof-preload.log`.

**Scope limit:** this proves load-time external buffers survive the CPU kernel
path. It is *not* yet per-token layer swapping, and it does not demonstrate a
bounded whole-process RSS. The Phase 2 generation gate therefore remains open.

---

## 8. Per-token layer refill — correctness and RSS (2026-11-17)

Extended the patch with a weak `msr_streaming_prepare_layer(int layer)` hook in
the gemma3 graph constructor (`src/models/gemma3.cpp`), called before each
layer's nodes are added. The preload provider now accepts comma-separated
`MSR_EXTERNAL_PREFIX` values and refills the cached buffers for that layer on
every graph build.

Run: same fixed prompt/seed/settings as §7, with
`MSR_EXTERNAL_PREFIX=blk.0.,blk.1.,...,blk.7.` (8 layers, 104 tensors).

| Run | External tensors | Output vs stock | VmHWM |
| --- | ---: | --- | ---: |
| Stock, `-lm none --no-repack` | 0 | reference | 2,708,208 kB |
| Per-token refill, 8 layers | 104 | byte-identical | 3,159,392 kB |

Logs: `benchmarks/results/stream8-preload.{txt,log}`; stock reference in
`external-proof-stock.txt`. The prepare hook fired for all 34 layers and refilled
the 8 cached ones.

**Interpretation:** per-token external-buffer refresh is correct, but this
prototype **increases** peak RSS because the normal loader still keeps the
non-external weights resident and the external layer buffers are additional.
It does not yet demonstrate a bounded 2 GiB whole-process footprint. The Phase 2
gate remains open until a variant avoids the duplicate residency (e.g. normal
layers via a revoked mmap window, or all weights staged from one bounded arena).

---

## 9. Failed experiment — per-node `MADV_DONTNEED` on mmap layers (2026-11-17)

Attempted the recommended windowed-mmap approach:

- Added a weak `msr_streaming_node_done()` hook in `ggml-cpu.c`, called on thread 0
  after each graph node.
- Added `msr_streaming_layer_boundary(layer, tensor)` in the gemma3 graph builder
  to tag each layer's final node.
- The preload provider resolved the model's `/proc/self/maps` base, computed each
  `blk.N` byte range from the GGUF, and called `madvise(MADV_DONTNEED)` when the
  layer boundary node completed.
- Run configuration: `-lm mmap`, 8 external layers, 4 tokens, 120 s timeout.

**Result: failed / not viable as implemented.** The process did not finish even
4 tokens within 120 s (`rc=124`, ~109 s user CPU), with revoke messages for all
34 layers in the log. Per-node revocation on this FUSE-backed mmap either stalls
or causes pathological re-faulting; no usable RSS number was obtained.

Raw logs: `benchmarks/results/window4.{txt,log}` (empty output, timeout).
The patch is retained in `patches/0001-msr-external-buffer-hook.patch` for
reference, but this revoke path should not be used without a redesign (e.g.
revoke once per token at graph start, or use `O_DIRECT` buffers instead of mmap).

---

## 10. Bounded-arena staging — Phase 2 gate (2026-11-17)

Single-arena staging replaces both earlier prototype paths. Every weight tensor
is bound directly to one budgeted arena; the loader never allocates a second
copy. Resident units (embedding, norms, output norm, low layers) keep permanent
slices; the remaining transformer layers share one stream buffer that is
refilled immediately before each streamed layer executes.

### Implementation notes (needed to reproduce)

- `MSR_NO_ALLOC=1` now means **weights only**. llama.cpp's own `no_alloc` flag
  also disables KV-cache and compute-buffer allocation, which segfaults in
  `ggml_gallocr_alloc_graph` (`buffers[0] == NULL`; observed first on
  `cache_k_l0`). The patch therefore uses a separate `weights_no_alloc` flag and
  leaves the rest of the context on the normal allocation path.
- The per-layer refill marker is the `-<layer>` suffix on graph node names
  (`norm-33`, `Qcur-7`, …), not a tensor pointer. Fused ops absorb the tensor
  that the graph constructor hands to `msr_streaming_layer_first_node`, so
  pointer lookups never hit.
- The weak `msr_streaming_node_start(node, name)` hook runs on thread 0 before
  the node's compute, followed by a barrier; the stream buffer is safe to share
  across threads.

### Conditions

| Field | Value |
| --- | --- |
| Device | Redmi Turbo 3 (`24069RA21C`, `peridot`), Snapdragon 8s Gen 3, Android 15, 11.0 GiB visible RAM (`MemTotal` 11,542,640 kB) |
| Model | `medgemma-1.5-4b-it.Q4_K_M.gguf`, 2,489,894,624 B, payload 2,483,352,832 B |
| Runtime | vendored llama.cpp `ef6bc6f`, `-t 4`, `-lm none`, `--no-repack`, `-c 2048` |
| Prompt | `The capital of France is`, 14 prompt tokens, 7 generated, seed 42, temp 0 |
| Measurement | `scripts/run_measured.py`, 0.2 s sampling of `/proc/<pid>/status` + `smaps_rollup` |
| Runs | 3 per configuration, 10 s cooldown between runs |

### Results

| Configuration | Weight budget | Resident weights | Streamed layers | VmHWM median | Spread | Output vs stock |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| Stock loader (no streaming) | — | — | 0 | **2,645.6 MiB** | 0.6 MiB | reference |
| Arena, 2 GiB budget | 2,147,483,648 B | 2,066,915,328 B | 7 | **2,312.2 MiB** | single run | byte-identical |
| Arena, 1 GiB budget | 1,073,741,824 B | 1,012,854,784 B | 26 | **1,307.4 MiB** | 0.5 MiB | byte-identical |

`peak_weight_bytes` reported by the provider: 2,127,478,784 B (2 GiB) and
1,073,418,240 B (1 GiB) — both inside the configured budget.

Throughput at the 1 GiB budget (run 1; medians match within 3%):
prompt eval 16.89 → 11.68 tok/s, generation 9.91 → 2.34 tok/s. Load time fell
from 2.28 s to 1.01 s because deferred reads are no longer performed.

Raw data: `benchmarks/results/gate2-{stock,arena1g}-r{1,2,3}.{json,txt}`,
`phase2-arena-{2g,1g}-2k.{json,txt}`.

### Reproduce

```sh
cd ~/mobile-stream-runtime
M=/storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf
MSR_NO_ALLOC=1 MSR_PREFETCH=1 MSR_READ_THREADS=4 \
MSR_BUDGET_BYTES=1073741824 MSR_MODEL_PATH=$M \
LD_PRELOAD=$PWD/native/build/libmsr-external.so \
build/bin/llama-completion -m $M -p "The capital of France is" -n 8 \
  --seed 42 --temp 0 -t 4 -lm none --no-repack -c 2048

# full A/B sweep (5 interleaved runs per config, writes summary.json)
python3 scripts/ab_prefetch.py --model $M --runs 5 \
  --out-dir benchmarks/results/phase3-warm-1g \
  --configs off,prefetch-1t,prefetch-4t
```

### Gate verdict — **PASS**

- Inference completes with 26 of 34 layers streamed through one 60.6 MB buffer
  while the whole process stays at **1,307 MiB**, i.e. **50.6 % below** the
  stock loader and **well under** the 2,313 MiB tensor payload.
- Greedy output is byte-identical to the stock loader in all six arena runs.
- Memory is measured, not asserted (`VmHWM`/`PSS` sampled externally), and the
  arena never exceeds its budget.

Caveats carried forward:

- This is the reference device (Redmi Turbo 3, 11.0 GiB visible). No claim
  for 6–8 GiB devices yet.
- The 1 GiB budget costs ~4.2× generation throughput because ~1.5 GiB per token
  is re-read from storage. Prefetch (Phase 3) targets exactly this gap.
- Only `gemma3` text is exercised. mmproj/vision remains Phase 6.

---

## 11. Phase 3 — bounded async reads and two-slot layer prefetch

Goal: hide layer read latency behind compute instead of paying it serially at
every layer boundary, without raising the memory cap.

### Implementation

New module (`native/include/msr/scheduler/async_reader.hpp`,
`native/src/scheduler/async_reader.cpp`):

| Component | Behaviour |
| --- | --- |
| `AsyncReader` | Bounded queue of positional reads (`queue_depth`, default 32) served by 1–16 worker threads. `submit()` blocks when the queue is full, so nothing grows without limit. `wait(tag)`, `drain()`, `cancel_pending()` (queued → `Cancelled`, in-flight finishes), plus counters for bytes, I/O time, compute-thread wait time, submit blocking and peak queue depth. |
| `LayerPrefetcher` | Groups ranges per layer (a layer is 13 separate GGUF tensors, not one extent), supports a synchronous first layer, `wait_layer`, `cancel_pending`, `reset`. Owns no buffers. |

Android's FUSE storage exposes no usable `io_uring`/AIO here, so the portable
fallback is a thread pool issuing `pread`; the interface does not assume
completion order, so a kernel backend can replace the workers later.

Provider wiring (`native/tools/msr_external_preload.cpp`):

- `MSR_PREFETCH=1` allocates **two** stream slots. Streamed layer `k` is bound to
  slot `k % 2`, so a layer can be read while the previous one is computing.
- At the first compute node of layer `k`: wait for the already-scheduled read of
  `k` (or read it synchronously if none is pending), then submit every range of
  layer `k+1` into the other slot. The slot being read is always the one used by
  layer `k-1`, whose compute has finished.
- `msr_streaming_prepare_layer(0)` also schedules the first streamed layer during
  graph construction, overlapping it with graph build and the resident layers.
- `MSR_READ_THREADS=N` (default 1) sets the worker count; page-cache reads are
  `memcpy`-bound and parallelise across cores.
- `msr_streaming_cancel()` is exported for the Phase 4 JNI layer. No in-runtime
  caller yet.
- Cost: one extra 60.6 MB slot. The planner streams 1–2 more layers so peak
  weight memory stays under budget (at 1 GiB: 1,020,289,024 B of 1,073,741,824 B).

### Correctness

Greedy output (`-n 24`, seed 42, temp 0) is byte-identical to the stock loader
with prefetch off, with one reader thread, and with four reader threads. The
`scheduler_async` CTest covers bounded depth, parallel completion, unknown tags,
cancellation without hangs, multi-range layer groups and the synchronous first
layer.

### Conditions

| Field | Value |
| --- | --- |
| Device | Redmi Turbo 3 (`24069RA21C`, `peridot`), Snapdragon 8s Gen 3, 8 cores, Android 15, 11.0 GiB visible RAM |
| Model | `medgemma-1.5-4b-it.Q4_K_M.gguf`, payload 2,483,352,832 B |
| Runtime | vendored llama.cpp `ef6bc6f`, `-t 4`, `-lm none`, `--no-repack`, `-c 2048` |
| Prompt | `The capital of France is`, 14 prompt tokens, 7 generated, seed 42, temp 0 |
| Runs | 5 per configuration, **interleaved round-robin**, 4 s cooldown |
| Measurement | `scripts/run_measured.py` (0.2 s `VmHWM`/PSS sampling), driver `scripts/ab_prefetch.py` |

Interleaving matters: non-interleaved runs drift by up to ~30 % on this device
(thermal and page-cache state), so medians are only comparable within one sweep.

### Results — 1 GiB budget (hard cap respected everywhere)

| Configuration | Resident weights | Slots | Streamed | Peak weight bytes | VmHWM median | eval tok/s | prompt tok/s | wall s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Prefetch off | 1,012,854,784 | 1 | 26 | 1,073,418,240 | 1,306.9 (0.5) | 2.06 (0.29) | 12.11 (3.50) | 7.80 (0.96) |
| Prefetch, 1 reader | 899,162,112 | 2 | 28 | 1,020,289,024 | 1,256.2 (0.6) | 2.27 (0.24) | 16.62 (4.69) | 6.94 (0.83) |
| Prefetch, 4 readers | 899,162,112 | 2 | 28 | 1,020,289,024 | **1,255.8 (0.6)** | **3.00 (0.44)** | 15.15 (3.32) | **6.11 (0.62)** |

(VmHWM in MiB, wall in seconds; spread = max − min of 5 runs.)

Read-side counters (medians, same runs):

| Configuration | Bytes read | Worker I/O time | Compute-thread wait | Peak queue depth |
| --- | ---: | ---: | ---: | ---: |
| Prefetch off | 14.71 GB | 3.45 s | 3.82 s | 1 |
| Prefetch, 1 reader | 15.84 GB | 3.95 s | 2.69 s | 13 |
| Prefetch, 4 readers | 15.84 GB | 7.36 s (sum of 4 workers) | **1.42 s** | 13 |

Prefetch with one reader hides ~30 % of the wait; four readers hide ~63 %
despite reading 7.7 % more bytes (28 vs 26 streamed layers, because the second
slot consumes budget). Generation rises **+45.6 %** (2.06 → 3.00 tok/s),
whole-run wall time falls **−21.7 %**, and peak RSS is **3.9 % lower** than
prefetch-off.

### Results — 1.5 GiB budget

| Configuration | Resident weights | Slots | Streamed | Peak weight bytes | VmHWM median | eval tok/s | prompt tok/s | wall s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Prefetch off | 1,513,320,448 | 1 | 17 | 1,573,883,904 | 1,784.1 (0.6) | 2.10 (1.02) | 9.39 (5.97) | 8.58 (1.32) |
| Prefetch, 4 readers | 1,460,191,232 | 2 | 18 | 1,581,318,144 | 1,790.8 (1.2) | **3.74 (0.40)** | **14.16 (5.10)** | **6.38 (1.39)** |

Generation **+78.1 %** for +0.4 % peak memory. The prefetch-off row is noisy
(range 1.95–2.92 tok/s) and barely faster than the 1 GiB off row despite 35 %
fewer streamed bytes: the larger resident arena leaves less page cache for the
model file, so more re-reads reach storage. That effect is visible in the
read counters too (9.70 GB at 2.97 s ≈ 3.3 GB/s vs 4.3 GB/s at 1 GiB).

The practical consequence: **1 GiB + prefetch is strictly better than 1.5 GiB
without prefetch** — 1,255.8 MiB and 3.00 tok/s versus 1,784.1 MiB and
2.10 tok/s (−29.6 % memory, +42.9 % speed). Against the stock loader the best
Phase 3 configuration keeps **1,255.8 MiB** (−52.5 % peak) and reaches 30 % of
stock generation throughput (9.91 tok/s).

### Gate verdict — **PASS**

- Prefetch correctness proven: byte-identical greedy output for 1 and 4 readers.
- Latency improvement quantified on hardware: +45.6 % at 1 GiB, +78.1 % at
  1.5 GiB, with medians and spreads from 5 interleaved runs.
- Memory cap still respected: peak weight bytes 1,020,289,024 ≤ 1,073,741,824;
  whole process 1,255.8 MiB, the lowest of any configuration measured so far.

Caveats carried forward:

- These runs are **warm page cache** (the realistic steady state for repeated
  decoding). Cold-storage behaviour could not be forced: `posix_fadvise`
  `DONTNEED` on the model file had no measurable effect on FUSE storage and
  there is no root to drop caches. The 1.5 GiB off/on gap suggests cache
  pressure, not raw storage latency.
- The extra slot forces 1–2 more streamed layers, which is why 1 reader per
  worker helps less than expected and why prompt throughput with 4 readers is
  slightly below the 1-reader run (prompt compute also uses 4 threads, so the
  readers oversubscribe briefly). The 4-reader setting is tuned for this 8-core
  device.
- `token_embd` (525 MiB) must stay resident, so the practical floor is ~600 MiB
  with one slot, ~720 MiB with two. 512 MiB is infeasible for this model; no
  lower-RAM device has been tested.
- Cancellation is implemented and unit-tested but not yet called from the
  runtime; the JNI layer (Phase 4) is the intended caller.
- Only `gemma3` text is exercised. mmproj/vision remains Phase 6.

Raw data: `benchmarks/results/phase3-warm-1g/`,
`benchmarks/results/phase3-warm-15g/` (`*.json` per run, `summary.json`).
Consolidated device summary: `benchmarks/REDMI_TURBO_3_BENCHMARK.md`.
