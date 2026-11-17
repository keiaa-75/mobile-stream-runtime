# Memory management (Phase 2 foundations)

Status: implemented and tested; real inference integration is still open.

## Weight-buffer arena

`msr::memory::MemoryManager` owns one page-aligned arena of exactly the
configured budget. Allocations are first-fit, rounded to 4096 B, and tracked by
id. There is no `malloc` fallback: an allocation that does not fit fails with a
counted error. Buffers can be pinned while a backend binds them; pinned buffers
cannot be released until unpinned. Reported statistics include current bytes,
peak bytes, pinned bytes, allocation failures, and acquire/release counts.

## Residency planning

`msr::streaming::build_layer_plan` groups tensor names into units:

- `token_embd.weight` (tied embedding / output head)
- `output_norm.weight`
- `blk.0 … blk.33` (dense transformer layers)
- equivalent `v.block.*` / `mm.*` units for the projector

`choose_residency` checks page-rounded costs, not raw tensor bytes. If the
largest unit does not fit the budget it returns an error instead of silently
loading the full model. When streaming is required it reserves one stream-buffer
slot so the pinned resident set cannot starve the loader.

## Staging path

`StreamingLoader` reads each unit's tensors into a single bounded buffer with
positional `pread` (or page-rounded `O_DIRECT` reads), calls the backend with
the exact unit byte count, then releases streamed units. Resident units stay
pinned and are handed to the backend each token without re-reading. Cancellation
is checked between units and between tokens. A hard assertion rejects any run
whose peak weight bytes reach the full model payload.

## Measured proof (MedGemma Q4_K_M, 2 tokens)

```
budget_bytes=671088640          (640 MiB)
model_payload_bytes=2483352832  (2.313 GiB)
peak_weight_bytes=664342528     (~634 MiB)
total_read_bytes=4853006848
read_amplification=1.95422
```

All buffers were returned (`current_bytes == 0`). The amplification value is
expected for two tokens: streamed layers are read once per token while the
embedding and a few layers stay resident. This is a memory/read audit, **not** a
tokens/sec or inference-correctness result.

## Outstanding for the Phase 2 gate

- llama.cpp loader patch accepting externally supplied tensor buffers
  (`LLAMA_STREAMING_BUFFERS`, ADR-001).
- A small dense model for greedy-output comparison against stock llama.cpp.
- Whole-process RSS/PSS measurement of the patched run, not just weight bytes.
