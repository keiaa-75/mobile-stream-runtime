# Redmi Turbo 3 — benchmark results

All measured numbers for the reference device, in one place. Raw runs live in
`benchmarks/results/`; the per-gate analysis is in `BENCHMARKS.md` §10 (Phase 2)
and §11 (Phase 3).

## Device

| Field | Value |
| --- | --- |
| Market name | Redmi Turbo 3 (Xiaomi) |
| Model / codename | `24069RA21C` / `peridot` |
| SoC | Qualcomm Snapdragon 8s Gen 3 (`SM8635`), 8 cores |
| RAM | 12 GB class — `MemTotal` 11,542,640 kB (11.0 GiB visible to Linux) |
| OS | Android 15 (HyperOS) |
| Storage | Emulated/FUSE `/storage/emulated/0`, measured ~1.4 GiB/s sequential |
| Thermals | Reaches ~74 °C under sustained load; throttles visibly |

## Conditions

| Field | Value |
| --- | --- |
| Model | `medgemma-1.5-4b-it.Q4_K_M.gguf`, 2,489,894,624 B, payload 2,483,352,832 B, 34 layers |
| Runtime | vendored llama.cpp `ef6bc6f`, `-t 4`, `-lm none`, `--no-repack`, `-c 2048` |
| Prompt | `The capital of France is`, 14 prompt tokens, 7 generated, seed 42, temp 0 |
| Memory measurement | `scripts/run_measured.py`, 0.2 s sampling of `/proc/<pid>/status` + `smaps_rollup` (`VmHWM`, PSS) |
| A/B driver | `scripts/ab_prefetch.py`, 5 runs per config, interleaved round-robin, 4 s cooldown |
| Output check | greedy text diff against the stock loader |

Interleaving matters on this device: non-interleaved runs drift up to ~30 %
(thermal and page-cache state), so only medians within one sweep are directly
comparable. Spreads are reported everywhere.

## Headline results

Same prompt, same flags, byte-identical greedy output in every streamed run:

| Configuration | Peak VmHWM | Generation | Prompt eval | vs stock |
| --- | ---: | ---: | ---: | --- |
| Stock loader (everything resident) | 2,645.6 MiB | 9.91 tok/s | 16.89 tok/s | reference |
| Arena 2 GiB, no prefetch | 2,312.2 MiB | — | — | −12.6 % RAM |
| Arena 1 GiB, no prefetch | 1,307.4 MiB | 2.34 tok/s | 11.68 tok/s | −50.6 % RAM, 4.2× slower |
| Arena 1 GiB + prefetch, 4 readers | **1,255.8 MiB** | **3.00 tok/s** | 15.15 tok/s | **−52.5 % RAM**, 3.3× slower |
| Arena 1.5 GiB + prefetch, 4 readers | 1,790.8 MiB | 3.74 tok/s | 14.16 tok/s | −32.3 % RAM, 2.6× slower |

Best configuration for this device: **1 GiB budget + prefetch + 4 reader
threads** — lowest memory ever measured and faster than a 1.5 GiB budget
without prefetch.

## Phase 2 — bounded arena (3 runs each)

| Configuration | Streamed layers | Peak VmHWM (median) | Spread |
| --- | ---: | ---: | ---: |
| Stock loader | 0 | 2,645.6 MiB | 0.6 MiB |
| Arena, 2 GiB budget | 7 / 34 | 2,312.2 MiB | single run |
| Arena, 1 GiB budget | 26 / 34 | **1,307.4 MiB** | 0.5 MiB |

Peak weight bytes reported by the provider: 2,127,478,784 B (2 GiB) and
1,073,418,240 B (1 GiB) — inside budget.

## Phase 3 — async prefetch (5 interleaved runs each)

1 GiB budget:

| Config | Resident weights | Slots | Streamed | Peak weight bytes | VmHWM | eval tok/s | prompt tok/s | wall s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Prefetch off | 1,012,854,784 | 1 | 26 | 1,073,418,240 | 1,306.9 (0.5) | 2.06 (0.29) | 12.11 (3.50) | 7.80 (0.96) |
| Prefetch, 1 reader | 899,162,112 | 2 | 28 | 1,020,289,024 | 1,256.2 (0.6) | 2.27 (0.24) | 16.62 (4.69) | 6.94 (0.83) |
| Prefetch, 4 readers | 899,162,112 | 2 | 28 | 1,020,289,024 | **1,255.8 (0.6)** | **3.00 (0.44)** | 15.15 (3.32) | **6.11 (0.62)** |

1.5 GiB budget:

| Config | Resident weights | Slots | Streamed | Peak weight bytes | VmHWM | eval tok/s | prompt tok/s | wall s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Prefetch off | 1,513,320,448 | 1 | 17 | 1,573,883,904 | 1,784.1 (0.6) | 2.10 (1.02) | 9.39 (5.97) | 8.58 (1.32) |
| Prefetch, 4 readers | 1,460,191,232 | 2 | 18 | 1,581,318,144 | 1,790.8 (1.2) | **3.74 (0.40)** | **14.16 (5.10)** | **6.38 (1.39)** |

Read-side counters (medians, same runs):

| Config | Bytes read | Worker I/O time | Compute-thread wait | Peak queue depth |
| --- | ---: | ---: | ---: | ---: |
| 1 GiB, off | 14.71 GB | 3.45 s | 3.82 s | 1 |
| 1 GiB, 1 reader | 15.84 GB | 3.95 s | 2.69 s | 13 |
| 1 GiB, 4 readers | 15.84 GB | 7.36 s (sum of 4) | **1.42 s** | 13 |
| 1.5 GiB, off | 9.70 GB | 2.97 s | 3.48 s | 1 |
| 1.5 GiB, 4 readers | 10.23 GB | 4.66 s (sum of 4) | **0.91 s** | 13 |

## What the numbers mean

- The engine trades generation speed for memory: **−52 % peak RAM for ~3.3×
  slower generation** at the 1 GiB budget. Prompt processing stays close to
  stock (15.15 vs 16.89 tok/s).
- Prefetch recovers **+45.6 %** generation over the serial refill path at
  1 GiB, and **+78.1 %** at 1.5 GiB, while *lowering* peak memory (the second
  slot lets the planner keep two extra layers out of RAM).
- 1 GiB + prefetch beats 1.5 GiB without it in both memory and speed
  (1,255.8 MiB / 3.00 tok/s vs 1,784.1 MiB / 2.10 tok/s), so spending RAM on a
  larger resident set is not a win on this device.
- The 1.5 GiB prefetch-off row is barely faster than 1 GiB off despite 35 %
  fewer streamed bytes: the larger resident arena leaves less page cache for
  the model file, pushing more reads to storage.

## Caveats

- Warm page cache: the realistic steady state for repeated decoding. Cold
  storage could not be forced — `posix_fadvise(DONTNEED)` is a no-op on this
  FUSE mount and there is no root to drop caches.
- Thermal drift remains; medians come from interleaved runs, spreads are shown.
- Cancellation is implemented and unit-tested but not wired to a runtime abort.
- `token_embd` (525 MiB) must stay resident, so the practical budget floor is
  ~600 MiB with one slot and ~720 MiB with two. 512 MiB is infeasible for this
  model.
- No claim is made for 6–8 GiB devices; this is the only device measured.

## Raw data index

| Path | Contents |
| --- | --- |
| `benchmarks/results/gate2-stock-r{1,2,3}.{json,txt}` | Phase 2 stock loader |
| `benchmarks/results/gate2-arena1g-r{1,2,3}.{json,txt}` | Phase 2 arena, 1 GiB |
| `benchmarks/results/phase2-arena-{2g,1g}*.{json,txt}` | Phase 2 budget sweep |
| `benchmarks/results/phase3-warm-1g/` | Phase 3, 1 GiB: `off`, `prefetch-1t`, `prefetch-4t`, `summary.json` |
| `benchmarks/results/phase3-warm-15g/` | Phase 3, 1.5 GiB: `off`, `prefetch-4t`, `summary.json` |
| `benchmarks/results/{repack,none-nr,dio-nr,auto-nr}*` | Storage/repack baseline runs (BENCHMARKS §4–5) |

Reproduce any row with the commands in `BENCHMARKS.md` §11 and
`scripts/ab_prefetch.py`.
