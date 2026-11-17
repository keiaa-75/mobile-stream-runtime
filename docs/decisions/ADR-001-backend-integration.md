# ADR-001 — Backend integration approach

- **Status:** Proposed (Phase 0 gate)
- **Date:** 2026-11-16
- **Context commit under review:** `ggml-org/llama.cpp@ef6bc6f367082338006c8379bc7bcab3e1584766`
  (shallow clone at `third_party/llama.cpp`)

## Context

The project needs a tensor-compute backend that can execute a layer from
weights the runtime placed in **its own bounded buffers**, without keeping the
full model resident.

llama.cpp was inspected for three questions:

1. Where do weights live after load?
2. Can a caller supply/replace tensor storage per layer?
3. What does each load mode actually cost in RSS?

## Findings

| Finding | Evidence |
| --- | --- |
| Load modes are `auto / none / mmap / mlock / mmap+mlock / dio` | `include/llama.h:208-215` |
| `mmap` binds `tensor->data` directly into the file mapping — zero copy, but pages become resident as compute reads them | `src/llama-model-loader.cpp:1796-1833` |
| `mmap` does **not** evict: the only advice calls are `MADV_WILLNEED` / `MADV_RANDOM` (+ `MAP_POPULATE` when prefetching) | `src/llama-mmap.cpp:722-753`, `1120-1135` |
| `load-mode none` reads each full tensor into a host buffer — full model in heap | `src/llama-model-loader.cpp:1834-1839` |
| `load-mode dio` opens the file with `O_DIRECT` | `src/llama-mmap.cpp:425` |
| A "lazy read" facility exists, but only for arch-marked tensors, needs mmap, and in `auto` only applies to tensors > 4 GiB | `include/llama.h:220-223`, `src/llama-model-loader.cpp:1183-1208` |
| No architecture in this snapshot marks gemma3 tensors for lazy read | `grep TENSOR_READ_LAZY src/ -> only loader/model plumbing` |
| Tensors are created per buffer-type and owned for the model's lifetime; there is no per-layer swap API | `src/llama-model-loader.cpp:1211-1232`, `src/llama-model.cpp` load path |
| `llama_model_init_from_user()` lets a caller supply tensor data via a callback — but only at load time, not per forward pass | `include/llama.h:517-526` |
| An auto-fit memory planner already exists for choosing how much to place per device | `common/fit.cpp:60-200` |

Measured on the reference device (see `BENCHMARKS.md`):

- Naive mmap + full touch → **2319 MiB RSS** for a 2375 MiB file (97.7%).
- Windowed mmap (256 MiB) + `MADV_DONTNEED` → **16 MiB RSS**, 272 MiB VmHWM.
- `O_DIRECT` works on the shared-storage FUSE mount.
- Sustained read ≈ **1.4 GiB/s**; random 16 MiB ≈ sequential.
- Full dense streaming ⇒ **≈0.6 tok/s** ceiling before compute.

## Decision

**Integrate llama.cpp as the compute backend, vendored as a pinned fork, and
add a storage-streaming layer in front of its loader.** Do not write a tensor
engine.

Concretely:

1. **Fork, don't fork-in-place.** Vendor `llama.cpp` at a pinned commit under
   `third_party/`, keep patches in `patches/`, and track them in a series file.
   Upstream is moving fast (load modes and lazy read changed recently).
2. **Add an explicit residency planner.** Model = ordered list of *units*
   (embedding, 34 layer blocks, output projection, projector). Each unit has a
   byte cost and a reuse frequency. A planner chooses `resident` vs `streamed`
   under a configured budget.
3. **Patch the loader, not the graph.** The smallest viable patch is to make
   `llama_model_loader` accept an externally provided host buffer for a tensor
   in `create_tensor()`, then let the runtime swap contents per layer before
   execution. Everything above (graph, kernels, sampling) stays untouched.
4. **Use `O_DIRECT` for streamed units.** Page-cache pollution is the enemy of
   a bounded budget; measured `O_DIRECT` support makes this viable. Keep a
   buffered path as fallback for filesystems that reject it.
5. **Keep the KV cache separate and independently configurable.** It is
   136 KiB/token and outside the streaming design.

## Rejected alternatives

| Alternative | Why rejected |
| --- | --- |
| Pure `--load-mode mmap` | Measured 2.3 GiB RSS; streamed-buffer goal unmet. |
| Pure `--load-mode none` | Full model in heap; worse than mmap. |
| Upstream `--lazy-mode on` | Only arch-marked tensors; gemma3 marks none; still mmap-based. |
| `llama_model_init_from_user` alone | Load-time only; cannot recycle buffers between tokens. |
| Write a new tensor engine | Out of scope, unbounded effort, no correctness baseline. |
| Rely on `mmap` + kernel reclaim | No eviction control, no determinism, LMK does the "policy". |

## Consequences

- We own a patch series against a fast-moving upstream → rebase cost is real;
  keep patches minimal and semantic, never a wholesale rewrite.
- Correctness can be checked against stock llama.cpp on the same GGUF, which
  is exactly the baseline Phase 5 needs.
- The plan is now a **hybrid** runtime (some units resident, some streamed),
  not a pure streaming engine — see `FEASIBILITY.md` for why the pure version
  is not worth shipping on 12 GB hardware.

## Follow-ups

- Prototype the `create_tensor` patch behind a CMake option
  (`LLAMA_STREAMING_BUFFERS`) and prove one layer executes from an external
  buffer.
- Measure read amplification with `O_DIRECT` at 4 MiB and 16 MiB granularity.
- Decide the unit granularity: whole layer (50.7–57.8 MiB) vs sub-tensor.
