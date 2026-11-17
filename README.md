# mobile-stream-runtime

Standalone on-device inference-engine R&D: run supported local LLMs while
**bounding peak RAM** by streaming quantized weights from storage into
reusable buffers.

Repository: <https://github.com/RavinEduard/Mobility_Engine> (private).

This repository is a systems/inference project, not a medical product.

- Canonical plan: `/storage/emulated/0/Download/gemma/DEVELOPMENT_PHASES.md`
- Project context: `/storage/emulated/0/Download/gemma/mobile_storage_streamed_llm_runtime_context.md`
- Model assets (12 GB target): `/storage/emulated/0/Download/gemma/`

## Current status — framework/SDK (Phases 1–4 complete)

| Deliverable | State |
| --- | --- |
| `FEASIBILITY.md` | written, gate verdict **conditional GO** with scoped-down goal |
| `docs/decisions/ADR-001-backend-integration.md` | written (llama.cpp fork + loader patch) |
| `docs/streaming-design.md` | draft (hybrid residency planner) |
| `docs/model-inventory/` | full GGUF parse for both files |
| `MODEL_COMPATIBILITY.md` | written, Phase 1 read evidence included |
| `BENCHMARKS.md` | storage, mmap, KV model, stock baseline, arena streaming (§10), async prefetch (§11), C API SDK (§12) |
| Conventional build baseline | done: ~2.62 GiB peak RSS, ~12 tok/s at 2K; see `BENCHMARKS.md` §5 |
| Native Phase 1 modules + tests | done: `native/`, 3 CTest targets passing |
| Native Phase 2 foundations + tests | done: bounded arena, layer plan, staging loader, hash backend |
| Real streamed inference gate | **passed**: 1 GiB arena, 26/34 layers streamed, 1,307 MiB peak, byte-identical output; `BENCHMARKS.md` §10 |
| Async read queue + prefetch gate | **passed**: 1 GiB arena, 28/34 layers streamed, 2 slots, **1,255.8 MiB** peak, **3.00 tok/s** (+45.6 % vs prefetch off), byte-identical; `BENCHMARKS.md` §11 |
| Thermal control for A/B runs | **partially mitigated** — `scripts/ab_prefetch.py` interleaves configs; device still reaches 74 °C, so spreads are reported |
| Platforms | Reference arm64 device **tested**; Linux/macOS expected, untested; Windows needs a port — `docs/PORTABILITY.md` |
| Redmi Turbo 3 benchmark summary | `benchmarks/REDMI_TURBO_3_BENCHMARK.md` (all gates, one page) |
| C framework API + installable SDK | **done**: `sdk/`, `docs/FRAMEWORK.md`, `msr-cli`, installed `find_package(msr)` example ran; `BENCHMARKS.md` §12 |
| Known limits + collaborator fixes | `LIMITATION.md` |
| Session work log | `updates.md` |

### Headline findings

- Q4_K_M MedGemma is **2.313 GiB** across **34 dense layers** (54.2 MiB each);
  `token_embd` alone is **525 MiB** and doubles as the output head.
- KV cache costs **136 KiB/token** (1.06 GiB at 8K, f16).
- Storage delivers **~1.4 GiB/s**; random 16 MiB ≈ sequential.
- Naive `mmap` of the GGUF → **2319 MiB RSS measured** (97.7% of the file).
  A 256 MiB window with `MADV_DONTNEED` → **16 MiB RSS / 272 MiB VmHWM**.
- Full layer streaming would cap at **≈0.6 tok/s**; the target is therefore a
  **hybrid residency** runtime with an enforced budget.
- Stock llama.cpp with CPU weight repacking uses **4,984 MiB**; with
  `--no-repack`, **2,624 MiB** — a 2.36 GiB penalty for a second copy.

## Getting started

```sh
git clone --recursive https://github.com/RavinEduard/Mobility_Engine.git
cd Mobility_Engine
scripts/setup.sh          # submodule, MSR hook patch, builds, CTest
```

Model weights are not committed: place the GGUF wherever you like (the
reference device uses `/storage/emulated/0/Download/gemma/`) and pass it with
`-m` / `MSR_MODEL_PATH`. Run commands are in `BENCHMARKS.md` §11.

## Layout

```
FEASIBILITY.md          Phase 0 gate document
LIMITATION.md           known limits, bugs, fix hand-off
BENCHMARKS.md           measured numbers + conditions
benchmarks/REDMI_TURBO_3_BENCHMARK.md   consolidated device results
ARCHITECTURE.md         (next)
docs/
  FRAMEWORK.md          C API reference, SDK build/consumption guide
  setup-fedora.md       Fedora x86_64 setup guide
  setup-lubuntu.md      Lubuntu x86_64 setup guide
  PORTABILITY.md        platform status: reference device / Linux / macOS / Windows
  streaming-design.md
  decisions/ADR-001-backend-integration.md
  model-inventory/      *.json (raw) + *.md (rendered tensor tables)
scripts/
  gguf_inspect.py       read-only GGUF header/tensor parser
  layer_budget.py       per-layer + KV + streaming-ceiling math
  io_bench.py           sequential/random throughput
  mmap_probe.py         mmap residency, whole-file vs windowed
  inventory_md.py       JSON -> Markdown
  run_measured.py       VmHWM/PSS sampler wrapper
  ab_prefetch.py        interleaved A/B runner for streaming configs
  setup.sh              fresh-clone setup + build + tests
sdk/                    installable C SDK (libmsr_engine, msr-cli, example)
native/
  include/msr/          public headers (storage, model, memory, streaming, backend, engine)
  src/                  implementations
  tools/model_inspect   GGUF inspector + bounded tensor reader
  tests/                GGUF malformed-file fixtures + streaming budget test
tests/byte_exact_check.sh  independent Phase 1 exit-gate check
updates.md              chronological session work log
benchmarks/results/     raw JSON output
third_party/llama.cpp   pinned shallow clone for inspection
```

## Reproduce

```bash
python3 scripts/gguf_inspect.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf \
    --json docs/model-inventory/medgemma-q4km.json
python3 scripts/layer_budget.py docs/model-inventory/medgemma-q4km.json
python3 scripts/io_bench.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf
python3 scripts/mmap_probe.py /storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf --windowed
```

Model weights are never committed. `checksums.txt` in the asset directory holds
the SHA-256 of both GGUFs.
