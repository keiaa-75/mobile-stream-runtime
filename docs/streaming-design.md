# Streaming design (draft)

Status: draft for the Phase 0 gate. Subject to change once the first patched
build exists.

## 1. Unit model

The runtime reasons about **units**, not files:

| Unit | Bytes | Reuse |
| --- | ---: | --- |
| `token_embd` (Q6_K, tied output) | 525.1 MiB | every token (input row + full-vocab logits) |
| `blk.N` (attention + FFN, N = 0..33) | 50.7–57.8 MiB each, 1.800 GiB total | once per token, in order |
| `output_norm` | 10 KiB | every token |
| mmproj `mm.*` + `v.*` | 811.8 MiB total | once per image, not per token |

Two facts drive the design:

1. **Every layer is touched every token.** Dense model, no expert routing.
   Full streaming therefore reads 2.313 GiB/token → ≈0.6 tok/s at the measured
   1.4 GiB/s. See `BENCHMARKS.md` §4.
2. **`token_embd` is both input and output.** Input needs one 2560-wide row,
   output needs the whole 525 MiB matrix for full-vocab logits. Keeping it
   resident is the cheap option; streaming it per token costs ~0.36 s/token
   by itself.

## 2. Residency planner

Given `budget_bytes` (derived from measured `MemAvailable` minus KV minus
activations minus margin), assign each unit one of:

- `RESIDENT` — loaded once, never evicted.
- `STREAMED` — read into a pooled buffer when its turn comes.
- `PINNED` — resident and page-locked (`mlock`), only for small hot units.

Default policy:

```
token_embd      RESIDENT (525 MiB)     # logits cost dominates otherwise
output_norm     RESIDENT (10 KiB)
blk.0 .. blk.33 STREAMED if budget < layers_total else RESIDENT
                with an LRU cache of the last K layers
mmproj          LOAD-PER-IMAGE, freed after encoding
```

`K` (layer cache depth) is the main tuning knob: `K = 0` is pure streaming,
`K = 34` is conventional loading.

Budget behaviour must be **enforced**: if the planner cannot fit the design,
it fails loudly rather than silently loading everything (the single most
important non-functional requirement in the project).

## 3. Read path

```
plan(layer N)
  -> buffer = pool.acquire(layer_bytes)
  -> reader.pread_exact(file, offset, len, buffer)   [O_DIRECT]
  -> backend.bind(tensor, buffer)
  -> execute
  -> pool.release / cache.put
```

Rules:

- `O_DIRECT` primary, buffered fallback if `open(O_DIRECT)` fails.
- `O_DIRECT` requires 4 KiB-aligned offset, length and buffer. GGUF tensor
  offsets are 32 B-aligned only → round the read down/up to 4 KiB and expect
  a small over-read (≤ 4 KiB per tensor; negligible at 50 MiB layers).
- Buffers come from one arena sized `budget_bytes`; no per-layer `malloc`.
- Pinned while a tensor is bound to the backend; unpinned only after the
  graph for that layer has completed.
- Cancellation is checked between layers and inside the reader.

## 4. Prefetch

- Single deterministic lookahead: start `read(N+1)` as soon as `N` is bound.
- Depth 1 is the default; depth 2 only if the buffer pool can hold it
  (2 × 58 MiB = 116 MiB per stream).
- No adaptive policy until the baseline numbers exist.
- Prefetch must never extend the peak buffer set beyond `budget_bytes`.

## 5. Eviction

- `MADV_DONTNEED` on any mmap window that is retired (measured: returns RSS).
- For `O_DIRECT` there is no page cache to evict — that is the point.
- LRU on the layer cache, never evicting pinned units.

## 6. Memory accounting

Reported separately, every run:

| Bucket | Source |
| --- | --- |
| resident weights | planner |
| streamed buffer arena | pool high-water |
| KV cache | context config × tokens |
| activations / graph | backend |
| projector | load-per-image or resident |
| process RSS/PSS peak | `/proc/self/statm`, `VmHWM` |
| system `MemAvailable` | `/proc/meminfo` before/after |

A claim of "bounded memory" is only valid against the **process peak**, not
against the arena alone.

## 7. Host mapping

- Runtime lives in a long-lived host process; the model file is provided by the
  host (absolute path) and never copied into the repository.
- Planned framework API: `msr_engine_create`, `load`, `generate`, `cancel`,
  `stats`, `destroy` (C ABI in `native/src/engine/`). No backend types cross it.
- `stats()` returns the table in §6 as a struct, so benchmarks and hosts read
  the same numbers.
- On a low-memory signal from the host OS, drop streamed slots before anything
  else; never evict a pinned buffer.

## 8. Open design questions

1. Patch granularity: whole-layer buffers (58 MiB) vs per-tensor buffers
   (2–20 MiB). Whole-layer is simpler; per-tensor allows a smaller budget and
   finer prefetch overlap.
2. Whether to patch `create_tensor()` to accept external buffers, or to
   replace `llama_model_loader::load_all_data()` with a streaming variant.
   The former is a smaller diff; the latter is more explicit.
3. Whether the projector should ever be resident on 8 GB devices (812 MiB is
   7% of total RAM for a cost paid once per image).
4. How to validate correctness cheaply: greedy decode with a fixed prompt,
   compared token-by-token against stock llama.cpp.
