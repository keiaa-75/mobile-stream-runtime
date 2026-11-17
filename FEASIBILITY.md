# FEASIBILITY

**Phase 0 gate document.** Answers one question: *is there a technically
credible way to execute at least one model with bounded weight buffers?*

Date: 2026-11-16 · Device: `24069RA21C` (`peridot`), Android 15, 11.5 GB RAM
· Upstream under review: `ggml-org/llama.cpp@ef6bc6f`

---

## 1. Bottom line

**Verdict: conditional GO — with a scope correction.**

- Executing a layer from externally supplied buffers is **possible** by
  patching llama.cpp's loader, but **no supported upstream API does it today**
  (`ADR-001`).
- **Pure full-model streaming is not worth shipping.** A dense 4B model needs
  2.313 GiB of reads per generated token. At the measured 1.4 GiB/s that caps
  generation at **≈0.6 tok/s** before compute, and no cache helps on the first
  token of every layer.
- Therefore v0.1 should be a **hybrid residency runtime**: a planner that
  keeps hot units (token embedding, most layers) resident and streams what
  does not fit, with the budget **enforced** rather than aspirational. That
  still delivers the project's real value on 12 GB — a hard, measurable RSS
  ceiling — without pretending 2.3 GiB fits in 200 MiB.

If the project insists on "weights never fully resident", it is feasible but
the target must be stated as *single-digit tokens per minute* for this model,
not interactive chat.

---

## 2. What was inspected

| Item | Result |
| --- | --- |
| llama.cpp loader, mmap, buffer allocation, lazy read, load modes | read at `src/llama-model-loader.cpp`, `src/llama-mmap.cpp`, `include/llama.h`, `common/fit.cpp` |
| MedGemma Q4_K_M GGUF | fully parsed: 444 tensors, 34 layers, 2.313 GiB payload |
| mmproj BF16 GGUF | fully parsed: 439 tensors, 27 vision blocks, 0.793 GiB payload |
| Storage behaviour | measured sequential/random throughput, `O_DIRECT`, mmap residency |
| Device memory | measured `MemTotal` / `MemAvailable` / `VmHWM` |

Raw data: `docs/model-inventory/*.json|md`, `benchmarks/results/*.json`,
`BENCHMARKS.md`.

---

## 3. Source-level findings

1. **mmap tensors are zero-copy but unbounded.** `load_all_data()` binds
   `tensor->data` directly to `mapping->addr() + weight->offs`
   (`llama-model-loader.cpp:1798-1820`). Every page the kernels touch becomes
   resident, and the loader never releases it — the only advice calls are
   `MADV_WILLNEED` and `MADV_RANDOM` (`llama-mmap.cpp:722-753`, `1120-1135`).
   *Measured consequence: 2319 MiB RSS for a 2375 MiB file.*
2. **`load-mode none` is not better.** It reads each full tensor into a host
   buffer (`llama-model-loader.cpp:1834-1839`), i.e. the whole model in heap.
3. **`O_DIRECT` is a real option.** `LLAMA_LOAD_MODE_DIRECT_IO` opens with
   `O_DIRECT` (`llama-mmap.cpp:425`), and this device's FUSE mount accepts it
   (measured). Tracking read (`llama-mmap.cpp:359`, `592`) exists for
   alignment-safe reads.
4. **A lazy mode exists but does not fit our case.** `LLAMA_LAZY_MODE_ON`
   applies to tensors an architecture marks with `TENSOR_READ_LAZY`
   (`llama.h:220-223`, `llama-model-loader.cpp:1183-1208`); in `auto` it only
   triggers above 4 GiB. gemma3 marks nothing.
5. **No per-layer substitution API.** Tensors are created once per buffer
   type and owned for the model's lifetime. `llama_model_init_from_user()`
   (`llama.h:517-526`) takes a data callback **at load time only**.
   → buffer recycling requires a loader patch.
6. **A memory-fit planner already exists** (`common/fit.cpp`) for choosing
   what fits where. Its structure is reusable for our residency planner even
   though its targets are devices/offload.

---

## 4. The numbers that decide this

### 4.1 Model shape (parsed, not assumed)

| Property | Value |
| --- | --- |
| Payload | 2,483,352,832 B = 2.313 GiB |
| Layers | 34, dense |
| Bytes per layer | 50.7–57.8 MiB (mean 54.2) |
| All layers | 1.800 GiB |
| `token_embd` (tied LM head) | 525.1 MiB Q6_K, vocab 262,208 |
| Q4_K / Q6_K share | 61.6% / 38.4% |
| Context (trained) | 131,072 |
| KV per token (f16) | 136 KiB |
| mmproj payload | 0.793 GiB (27 blocks, 896×896 input) |

### 4.2 Storage (measured)

| Metric | Value |
| --- | ---: |
| Sequential read | ~1.42 GiB/s |
| Random 16 MiB (cold-ish) | ~1.34 GiB/s |
| `O_DIRECT` | supported |
| mmap, full touch | 2319 MiB RSS (97.7% of file) |
| mmap, 256 MiB window + DONTNEED | 16 MiB RSS, 272 MiB VmHWM |

### 4.3 Streaming ceiling

| Strategy | Read per token | Ceiling |
| --- | ---: | ---: |
| All layers streamed | 1.800 GiB | ≈0.79 tok/s |
| All layers + token_embd streamed | 2.313 GiB | ≈0.60 tok/s |
| 8 layers cached, 26 streamed | 1.377 GiB | ≈1.03 tok/s |
| 16 layers cached, 18 streamed | 0.953 GiB | ≈1.49 tok/s |
| All resident | 0 | compute-bound |

Ceilings use 1.4 GiB/s and ignore compute, KV traffic, and thermal decay —
they are upper bounds, not forecasts.

---

### 4.4 Conventional baseline (measured, stock llama.cpp)

With repacking enabled (the default), peak RSS is **4,984 MiB**; with
`--no-repack` it is **2,624 MiB**. The 2.36 GiB difference is a second copy of
the weights in the CPU repack buffer. Median generation is ~12 tok/s at
2K context with `--no-repack`.

This changes two things:

- The baseline to beat is **2.62 GiB**, not the model's 2.49 GB.
- `--no-repack` is a hard requirement for any memory-budgeted configuration;
  it should be the default in our build, not a user flag.

Repeated runs reached **74 °C** and throughput fell by half; all future A/B
benchmarks must be interleaved and temperature-tagged. Full table and caveats
in `BENCHMARKS.md` §5.

## 5. Memory budget model

Assumptions: `MemAvailable` 4.57 GiB measured at idle on the reference
device; 512 MiB safety margin; activations + graph ≈ 300 MiB; KV f16 unless
stated.

| Scenario | Weights | KV | Projector | Activations | Total | Fits? |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| A: 12 GB, ctx 4K, projector resident | 2.31 | 0.53 | 0.79 | 0.30 | **3.93 GiB** | yes (tight) |
| B: 12 GB, ctx 8K, projector resident | 2.31 | 1.04 | 0.79 | 0.30 | **4.44 GiB** | marginal |
| C: 12 GB, ctx 8K, KV q8_0, projector per-image | 2.31 | 0.52 | 0.00 | 0.30 | **3.13 GiB** | yes |
| D: 6 GB class, ctx 2K, KV q8_0, 4-layer cache | 0.53 + 0.22 | 0.13 | 0.00 | 0.25 | **1.13 GiB** | yes, ≈1 tok/s |
| E: 6 GB class, pure streaming | 0.53 + arena | 0.13 | 0.00 | 0.25 | **~1.0 GiB** | yes, ≈0.6 tok/s |

Two conclusions:

1. On **12 GB**, MedGemma Q4_K_M + projector **already fits** without
   streaming, provided context is capped and the KV cache is quantized. The
   streaming runtime's value here is the **enforced ceiling and headroom for
   other apps**, not fitting a model that would otherwise fail.
2. On **6 GB**, streaming is the difference between "runs slowly" and "does
   not run". That is where the design earns its keep.

**Recommendation:** build the runtime around a budget that can be set from
`MemAvailable`, and validate it on the 12 GB device by *lowering* the budget
below the model size and proving the cap holds.

---

## 6. Strategy comparison

| Strategy | Bounded RSS | Effort | Speed | Verdict |
| --- | --- | --- | --- | --- |
| Stock `--load-mode mmap` | no (measured 2.3 GiB) | none | best | baseline only |
| Stock `--load-mode none` | no | none | similar | rejected |
| mmap + windowed DONTNEED, custom | yes | medium | ≈0.6–1.5 tok/s | **core mechanism** |
| `O_DIRECT` + pooled buffers, custom | yes, deterministic | medium-high | ≈0.6–1.5 tok/s | **core mechanism** |
| Loader patch for external buffers | yes | high | ≈0.6–1.5 tok/s | needed for clean design |
| Hybrid residency planner | yes, configurable | medium | 1–8+ tok/s | **v0.1 target** |

---

## 7. Go / no-go

**GO**, conditional on all of:

1. The loader patch is proven on one layer before any UI work. If a layer
   cannot execute from an external buffer, fall back to a *windowed-mmap +
   DONTNEED* design (already measured as viable) and accept coarser control.
2. The project redefines success as **enforced peak RSS with a configured
   residency budget**, not "runs a 2.3 GiB model in 200 MiB".
3. Interactive-speed claims are scoped to *hybrid* mode. Full-streaming mode
   is documented as ~0.6 tok/s and offered only for low-RAM devices.
4. KV quantization (`q8_0`) and a context cap are treated as first-class
   features, because KV is 136 KiB/token and dwarfs every other tuning knob.
5. `--no-repack` (or an equivalent build flag) is the default in every
   configuration; a repack copy costs 2.36 GiB.

If (1) fails **and** the windowed-mmap path cannot be made deterministic under
LMK, the verdict flips to **NO-GO for v0.1** and the recommendation is to ship
a conventional loader with a hard budget check instead of a streaming engine.

---

## 8. Open questions

1. Does an external-buffer patch survive ggml's CPU kernel path without
   copying? (Test: one layer, compare output to stock.)
2. `O_DIRECT` read granularity vs 32 B GGUF alignment — confirm over-read
   cost is negligible at 4 KiB rounding.
3. What does Android's LMK do to a 2.3 GiB-RSS process under a 300 MiB
   foreground app? (Test under load, not idle.)
4. Thermal decay curve during a 30-minute generation.
5. Actual compute time per layer on 8 cores — the ceiling table ignores it.
6. Whether the projector can be loaded per-image without re-initialising the
   context (mmproj lifetime vs graph).
7. `token_embd` logits cost: can partial-vocab sampling avoid reading the
   full 525 MiB tied matrix? (Probably not without changing the sampler.)

---

## 9. Blockers / not yet done

| Item | Status |
| --- | --- |
| `cmake` + `ninja` | installed; llama.cpp built (`llama-completion`, `llama-mtmd-cli`, `llama-bench`, `llama-gguf`) |
| Conventional baseline (tok/s, VmHWM, PSS) | **done** — `BENCHMARKS.md` §5 |
| Loader patch prototype | next (Phase 2) |
| Isolated TTFT / page faults during generation | open |
| Interleaved, temperature-controlled repack A/B | open — naive A/B confounded by 74 °C thermal drift |
| Battery mAh/1K tokens, sustained decay curve | open |
| Vision projector memory in a real image run | open (Phase 6) |
| Upstream checksum cross-check of both GGUFs | open (self-computed hashes recorded) |

The `llama-app` target fails to link in this CMake configuration because
`llama-server-impl` / `llama-cli-impl` are not built when
`LLAMA_BUILD_SERVER=OFF`. This does not affect the baseline: `llama-completion`
and `llama-mtmd-cli` link and run correctly.

---

## 10. Recommended next actions

1. ~~Install `cmake` + `ninja`, build llama.cpp.~~ **Done.**
2. ~~Conventional baseline.~~ **Done** (`BENCHMARKS.md` §5); isolated TTFT and
   page faults remain open.
3. Re-run the repack A/B with interleaved order and a thermal cooldown between
   runs before deciding whether repack is worth 2.36 GiB.
4. Prototype the `create_tensor` external-buffer patch behind a CMake option;
   prove one layer.
5. Re-run `mmap_probe.py --windowed` against the patched build to confirm the
   arena, not the mapping, sets the high-water mark.
6. Then start Phase 1 code (the GGUF reader is already prototyped in
   `scripts/gguf_inspect.py` and can be ported to C++).

---

## Appendix — reproduction

```bash
# inventory
python3 scripts/gguf_inspect.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf \
  --json docs/model-inventory/medgemma-q4km.json
python3 scripts/layer_budget.py docs/model-inventory/medgemma-q4km.json

# storage
python3 scripts/io_bench.py   /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf
python3 scripts/mmap_probe.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf
python3 scripts/mmap_probe.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf \
  --windowed --window-mib 256
```
