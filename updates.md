# Updates Log

Canonical work log for this session. Every action taken in this repository and
on the model assets is recorded here, in chronological order.

**Session date:** 2026-11-17
**Environment:** Termux on Android 15 (`24069RA21C`, aarch64, 11.5 GB RAM)
**Canonical project:** `~/mobile-stream-runtime/`

---

## 1. Asset discovery and relocation

| # | Action | Result |
| --- | --- | --- |
| 1 | Searched Termux downloads (`~/downloads`) for a directory named `gemna2` | Not found |
| 2 | Extended the `gemna2` search to `~/` and the Termux prefix | Not found |
| 3 | Searched `~/downloads` for `gemma` (exact name and partial `*gemma*`) | Not found |
| 4 | Listed `~/downloads` | Only two files present: `UNLIMITED PINAY SET - LINK.url`, `Web_Communication_API_Review_2_Pages.pdf` |
| 5 | Searched Android shared storage via `/storage/emulated` | Parent path itself not listable, but `/storage/emulated/0` is accessible |
| 6 | Searched `/storage/emulated/0` (maxdepth 8) for `*gemma*` | Found **`/storage/emulated/0/gemma`** |
| 7 | Inspected that directory | 5 files: `DEVELOPMENT_PHASES.md`, `checksums.txt`, `medgemma-1.5-4b-it.Q4_K_M.gguf` (2,489,894,624 B), `mmproj-medgemma-1.5-4b-it-BF16.gguf` (851,251,744 B), `mobile_storage_streamed_llm_runtime_context.md` |
| 8 | Checked destination `/storage/emulated/0/Download` | Exists, no `gemma` subfolder conflict, same FUSE filesystem (`/dev/fuse`), ~21 GB free |
| 9 | `mv /storage/emulated/0/gemma /storage/emulated/0/Download/gemma` | Succeeded (instant rename, same filesystem) |
| 10 | Verified move | New path contains all 5 files with original sizes; old path no longer exists |
| 11 | Created and removed a temporary `.write_test` file in the new folder | Confirmed Termux can write there; no leftover file |

**New asset root:** `/storage/emulated/0/Download/gemma`

---

## 2. Project state review (read-only)

Documents read and cross-checked:

- `/storage/emulated/0/Download/gemma/DEVELOPMENT_PHASES.md` — phase map, gates, immediate next actions.
- `/storage/emulated/0/Download/gemma/mobile_storage_streamed_llm_runtime_context.md` — original project context and engineering rules.
- `/storage/emulated/0/Download/gemma/checksums.txt` — self-computed SHA-256 for both GGUFs.
- `README.md`, `FEASIBILITY.md`, `docs/streaming-design.md`,
  `docs/decisions/ADR-001-backend-integration.md`.
- `scripts/gguf_inspect.py` — existing read-only Python GGUF parser (to be ported in Phase 1).

Findings:

- **Phase 0 is complete** with a *conditional GO*. The runtime goal was corrected
  to a **hybrid residency** engine because dense streaming of MedGemma Q4_K_M
  caps at ≈0.6 tok/s on the measured 1.4 GiB/s storage.
- The hard rule is that a phase cannot start until the previous gate is
  evidenced in writing. Therefore the next unblocked phase is **Phase 1 —
  Storage reader & model inspection**.
- One medium-priority Phase 0 follow-up remains (interleaved,
  temperature-controlled repack A/B); it does **not** block Phase 1.
- Toolchain available: `clang++`/`c++`, `cmake`, `ninja`, `python3`,
  `sha256sum`, OpenSSL 3.6.5 headers/libs (already used by the llama.cpp build).

---

## 3. Documentation path correction (pending)

Because the asset root moved, the following stale references still say
`/storage/emulated/0/gemma` and must be updated to
`/storage/emulated/0/Download/gemma`:

- `README.md` (lines 9–11, 63–67)
- `FEASIBILITY.md` (reproduction commands, §10/§Appendix)
- `/storage/emulated/0/Download/gemma/DEVELOPMENT_PHASES.md` (§0.1)
- Any script default paths that assume the old root

---

## 4. Implementation log

### 2026-11-17 — Phase 1: Storage reader & model inspection (COMPLETE)

Added the native Phase 1 code, tests and docs:

- `native/CMakeLists.txt` — builds `msr_core`, `model-inspect`, `test-gguf` with
  OpenSSL 3.6.5 (OpenSSL available in the existing Termux toolchain).
- `native/include/msr/status.hpp` — explicit status codes (IO, EOF, parse,
  unsupported, cancelled, integrity mismatch, …).
- `native/storage/model_file.*` — positional `pread_exact`, size/fd handling,
  cancellation flag, `O_DIRECT` open with buffered fallback, page-rounded
  `read_aligned` with reusable `AlignedBuffer`.
- `native/model/gguf.*` — buffered read-only GGUF parser: magic/version, all
  metadata value types (including arrays), tensor table, alignment padding,
  block-size compatibility, bounds and overlap validation, quant type table,
  bounded tensor-slice reads.
- `native/model/integrity.*` — SHA-256 of memory, file, and file ranges via EVP;
  hex parse/format; whole-file verification against `checksums.txt`.
- `native/tools/model_inspect.cpp` — prints header/tensor table, reads a chosen
  tensor into a fixed buffer (`READ_TENSOR`), hashes a tensor (`TENSOR_SHA`),
  verifies whole-file hash (`VERIFY`), optional JSON and `--direct`.
- `native/tests/make_fixtures.py` + `native/tests/test_gguf.cpp` — valid,
  truncated, wrong-endian, bad-version, zero-length, overlapping, unaligned,
  bad-block and out-of-bounds fixtures; 24 assertions, all passing via CTest.
- `tests/byte_exact_check.sh` — independent Python extraction compared to the
  native bounded read (Phase 1 exit gate).
- `MODEL_COMPATIBILITY.md` — parsed facts for both GGUFs, hashes, read evidence,
  and explicit unverified items.

Fixed during implementation:

- `Cursor::pos()` initially reported fetched bytes instead of consumed bytes,
  corrupting `data_start` on small files (found by the fixture tests).
- Fixture tensor offsets needed a 16 B alignment gap before tensor `c`.
- `read_tensor_range` needed separate destination and O_DIRECT scratch buffers.

Verification performed on the real assets (now under
`/storage/emulated/0/Download/gemma`):

- Main model: v3, `gemma3`, 444 tensors, alignment 32, payload 2,483,352,832 B,
  whole-file SHA-256 `ee5121f…639454` verified, tensor `output_norm.weight`
  (10,240 B) byte-exact across buffered and `--direct` paths.
- Projector: v3, `clip`, 439 tensors, alignment 32, payload 851,226,048 B,
  tensor `mm.input_projection.weight` (5,898,240 B) byte-exact.
- `ctest` passes 1/1; both `byte-exact OK` checks pass.

**Phase 1 gate: MET.** The runtime reads an arbitrary selected tensor range
byte-exactly into a bounded buffer, verified by an independent tool, and all
malformed-file tests pass. Phase 2 may now begin.

### 2026-11-17 — Phase 2: Bounded memory & layer streaming (FOUNDATIONS COMPLETE, GATE OPEN)

Note: a tool call was interrupted while writing the backend adapter; the session
was resumed and no files were lost. All files listed below were written after
the resume.

Added the Phase 2 foundations:

- `native/memory/memory_manager.*` — single aligned arena, first-fit reuse,
  page-rounded accounting, pin/unpin, failure counting, hard budget with **no**
  fallback allocation.
- `native/streaming/layer_plan.*` — groups GGUF tensors into 36 execution units
  for MedGemma (tied embedding, output norm, 34 dense layers); numeric layer
  order; residency planner that fails loudly when the largest unit exceeds the
  budget and reserves one stream-buffer slot so pinned units cannot starve
  streaming.
- `native/backend/backend_adapter.*` — minimal backend interface plus a
  `HashBackend` test adapter. It is explicitly **not** an inference backend; it
  only proves staged buffer contents are intact.
- `native/streaming/streaming_loader.*` — stages each unit into a bounded
  buffer, pins resident units, streams the rest per token, calls the backend,
  releases buffers, checks cancellation, accounts reads, and hard-fails if peak
  weight bytes ever reach the full-model payload.
- `native/tests/test_streaming.cpp` — allocator tests, real-model layer-plan
  tests, and a two-token staged pass with measured memory/read accounting.
- `native/CMakeLists.txt` — added the new modules and the `streaming_budget`
  CTest target.

Bugs found and fixed during implementation:

1. Page-rounded arena consumption was not included in the residency plan; the
   planner could declare a set "fitting" while the allocator rejected it.
   Added `resident_alloc_bytes` and budget checks against rounded costs.
2. The planner initially consumed the whole budget with resident units, leaving
   no room for a streamed buffer. It now reserves the largest unit's cost when
   any unit must be streamed.
3. `BackendAdapter::execute` was initially passed the padded buffer size
   instead of the exact unit byte count, which would corrupt hash comparisons.
4. A test expectation used 704512 B instead of the correct 700416 B rounded
   allocation size.

Verification (both CTest targets pass):

```
PROOF budget_bytes=671088640 model_payload_bytes=2483352832
      peak_weight_bytes=664342528 total_read_bytes=4853006848
      read_amplification=1.95422 seconds=7.34205
```

Interpretation: with a 640 MiB weight budget, the staged pass peaked at
~634 MiB of weight buffers versus a 2.313 GiB payload, and all buffers were
returned. Read amplification for two tokens is 1.95× the payload because
streamed layers are re-read per token; this is I/O evidence, not a throughput
or inference claim.

**Phase 2 gate remains OPEN.** The exit gate requires a *real inference path*
completing generation with output matching a conventional baseline. That still
needs the llama.cpp `create_tensor` external-buffer patch (ADR-001) and a small
dense model for correctness comparison. Per the project rule, Phase 3 is not
started until this gate is evidenced in writing.

---

## 5. Asset folder `cd` command

The moved gemma folder is here:

```bash
cd /storage/emulated/0/Download/gemma
```

Its contents: `DEVELOPMENT_PHASES.md`, `checksums.txt`,
`medgemma-1.5-4b-it.Q4_K_M.gguf`,
`mmproj-medgemma-1.5-4b-it-BF16.gguf`, and
`mobile_storage_streamed_llm_runtime_context.md`.

---

## 6. Pending user decision — small model for the Phase 2 correctness gate

No small GGUF currently exists on the device (only the two MedGemma assets,
llama.cpp vocab files, and tiny synthetic test fixtures). A small dense model
is required to compare greedy output from the patched/streamed path against a
conventional llama.cpp load.

### Options (verified from Hugging Face metadata on 2026-11-17)

| # | Model / file | Repo | Size | Notes |
| --- | --- | --- | ---: | --- |
| **A — recommended** | `qwen2.5-0.5b-instruct-q4_k_m.gguf` | `Qwen/Qwen2.5-0.5B-Instruct-GGUF` (official) | ~0.39–0.42 GB | Dense `qwen2`, tied embeddings, excellent size/quality, fast enough to iterate. Closest small analogue to the tied-embedding layout we must support. |
| B | `SmolLM2-135M-Instruct-Q4_K_M.gguf` | `bartowski/SmolLM2-135M-Instruct-GGUF` | 105 MB | SHA-256 `2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`. Tiny `llama` architecture, fastest download, but very small for stressing the layer plan. |
| C | `Llama-3.2-1B-Instruct-Q4_K_M.gguf` | `bartowski/Llama-3.2-1B-Instruct-GGUF` | 808 MB | SHA-256 `6f85a640a97cf2bf5b8e764087b1e83da0fdb51d7c9fab7d0fece9385611df83`. Dense `llama`, well supported, larger and more realistic; Meta Llama 3.2 license applies. |

**Recommendation: Option A (Qwen2.5-0.5B Q4_K_M).** It is small, dense, has a
tied embedding like MedGemma, and is well supported by the pinned llama.cpp
commit. Option B is the fastest if you want the quickest possible gate test;
Option C is the most representative but costs the most download time.

### What happens after you choose

1. Download only that single GGUF into `/storage/emulated/0/Download/gemma/`
   (explicit download, never automatic in builds/tests).
2. Compute its SHA-256, add it to `checksums.txt`, and cross-check against the
   repository metadata where published.
3. Run `model-inspect` and `byte_exact_check.sh` on it to confirm the Phase 1
   reader handles its GGUF layout.
4. Implement the `LLAMA_STREAMING_BUFFERS` loader patch in the vendored
   llama.cpp (ADR-001) and prove **one layer** executes from an external buffer.
5. Run fixed-prompt greedy decode with stock llama.cpp vs the patched/streamed
   path, compare token-by-token, and record whole-process RSS/PSS, TTFT and
   tok/s in `BENCHMARKS.md`.
6. Only then write the Phase 2 gate verdict and start Phase 3.

**Action needed from you:** pick A, B, C, or supply your own small GGUF. Nothing
further will be downloaded until you decide.

### Decision recorded (same session)

You chose **“I'll supply my own.”** No download has been started. The next
required input is the location of your GGUF: either a local path on the device
or a Hugging Face repo + filename / direct URL. Once provided, the download (if
needed), hash check, `model-inspect` validation, llama.cpp loader patch, and
stock-vs-streamed greedy comparison can proceed.

### Rescan result (same session)

You indicated the model is already in the gemma folder. I rescanned
`/storage/emulated/0/Download/gemma` at 07:33. It contains only the two original
GGUFs (`medgemma-1.5-4b-it.Q4_K_M.gguf` and
`mmproj-medgemma-1.5-4b-it-BF16.gguf`) plus the docs and `updates.md`. **No new
small GGUF is present.** Waiting for you to place the file there; no download or
loader patch has been started.

### Clarification — why MedGemma itself is not the Phase 2 small model

You asked whether the gemma model is “not small enough.” Correct: it is not the
right size for the Phase 2 *correctness gate*, though it remains the target for
Phase 5.

- MedGemma Q4_K_M is a **4B dense model** with a **2,489,894,624 B** file and a
  **2,483,352,832 B (2.313 GiB)** tensor payload.
- At the measured ~1.4 GiB/s storage rate, fully streaming it costs ~2.313 GiB
  of reads **per token** → ≈0.6 tok/s ceiling before compute. Under our 640 MiB
  budget most layers are streamed, so a correctness run would take a very long
  time and make token-by-token debugging impractical.
- The project plan explicitly says: start with a small dense model (≤1B) to
  iterate; use MedGemma here **only for tensor-layout validation**, not full
  generation. MedGemma text evaluation is Phase 5.
- The small model is needed to prove the llama.cpp external-buffer patch with a
  fixed prompt and compare greedy output against stock llama.cpp quickly.

Nothing has been downloaded or patched. We are waiting for a ≤1B GGUF to be
placed in `/storage/emulated/0/Download/gemma/`, or for you to approve one of
the download options already listed.

### Assessment — “download the smallest quantized MedGemma instead?”

You asked whether the smallest MedGemma quant would be enough. Findings from
Hugging Face metadata (2026-11-17):

| Quant | Repo | File size | Compatibility with our Phase 1 parser |
| --- | --- | ---: | --- |
| `i1-IQ1_S` | `mradermacher/medgemma-1.5-4b-it-i1-GGUF` | ~1.2 GB | **Not supported yet** — IQ quant types are absent from our tensor-type table; quality is labelled “for the desperate”. |
| `Q2_K` (static) | `unsloth/medgemma-1.5-4b-it-GGUF` / `mradermacher/...` | **1,729,165,376 B** (SHA-256 `e3cecb7ad1dfcf36e2226669f7a46286ec9eb899dbf0e330220c18105a7e8a4a`) | **Supported** — Q2_K is type 10 and present in our block-size table. |

Verdict: **technically enough, but not recommended.** Q2_K is still a 4B dense
model. With a ~640 MiB budget it would stream roughly 1.0–1.2 GiB per token
(≈0.7–1 s/token before compute), so a 16–32 token greedy comparison is feasible
but slow. It also bends the project rule that Phase 2 should iterate on a ≤1B
model. The small 0.5B model remains the better engineering choice.

If you still prefer to stay inside the MedGemma family, I recommend the static
**Q2_K** file (not IQ1_S, because of parser support and deterministic output),
with a documented deviation from the ≤1B guidance. No download has started.

### Decision reversed — project is MedGemma-specific

You chose Qwen2.5-0.5B Q4_K_M, so I downloaded it to
`/storage/emulated/0/Download/gemma/qwen2.5-0.5b-instruct-q4_k_m.gguf`:

- Bytes: 491,400,032 (matches Hugging Face metadata)
- SHA-256: `74a4da8c9fdbcd15bd1f6d01d621410d31c6fc00986f5eb687824e7b93d7a9db`
  (exact match to the official LFS OID)
- `model-inspect` parses it: GGUF v3, `qwen2`, 291 tensors, 26 KVs, payload
  485,452,288 B. The Phase 1 reader handles it correctly.

**You then clarified that this project is specifically for the MedGemma models**
and asked whether there is any way to use the gemma model instead. There is.
The small model was only a convenience for fast iteration, **not a hard
requirement**. Corrected plan:

- Use the **existing MedGemma Q4_K_M** already on disk for the Phase 2 gate.
- Raise the weight budget from 640 MiB to about **2.0 GiB**. The model payload is
  2.313 GiB, so the budget is still genuinely bounded and smaller than the model,
  but only ~8–10 layers need to be streamed per token instead of ~32.
- Expected streamed I/O drops from ~1.7 GiB/token to roughly **0.4–0.5 GiB/token**
  (~0.3 s I/O/token), making a 16–32 token greedy comparison practical.
- Fix the residency planner's reserve calculation first: it currently reserves
  the largest unit in the whole model (the 525 MiB tied embedding) even when
  that unit will be resident. It should reserve the largest *streamed* unit
  (≈57 MiB layer) instead.
- Then implement the llama.cpp external-buffer patch and run stock vs streamed
  greedy decode on MedGemma with a fixed prompt, recording RSS/PSS, TTFT and
  tok/s in `BENCHMARKS.md`.

The Qwen file is verified and usable as an optional generic sanity check, but it
is no longer needed for the MedGemma gate. Ask before deleting it.

### Planner fix for the MedGemma budget (same session)

Implemented and tested the corrected reserve logic in
`native/src/streaming/layer_plan.cpp`: when not everything fits, the planner now
reserves the largest *potentially streamed* unit (a ≈57 MiB layer) rather than
the largest unit overall (the 525 MiB tied embedding, which is resident). This
lets a 2 GiB budget keep ~26 layers resident and stream only ~8.

Added `test_streaming.cpp` assertions for the 2 GiB MedGemma plan: it succeeds,
fits, still streams at least one layer, streams at most 12 of 34 layers, and
every streamed unit fits the leftover arena. Both CTest targets pass after the
change.

### Qwen file relocated (same session)

Per your instruction, created `/storage/emulated/0/Download/qwrn` and moved the
Qwen model out of the gemma folder:

- From: `/storage/emulated/0/Download/gemma/qwen2.5-0.5b-instruct-q4_k_m.gguf`
- To: `/storage/emulated/0/Download/qwrn/qwen2.5-0.5b-instruct-q4_k_m.gguf`

Verified: `qwrn/` contains the 491,400,032 B file; `gemma/` again contains only
the two MedGemma GGUFs, the docs, `checksums.txt`, and `updates.md`. The Qwen
file is retained as an optional generic sanity model outside the MedGemma asset
root. Next: llama.cpp external-buffer loader patch on MedGemma.

### llama.cpp external-buffer hook — one-layer proof (same session)

Implemented the minimal loader hook:

- `patches/0001-msr-external-buffer-hook.patch` records the vendored change to
  `third_party/llama.cpp/src/llama-model-loader.cpp`.
- The change declares a weak C symbol `msr_streaming_supply_tensor(name, &data,
  size)`. In `load_all_data()`, if the symbol resolves and returns a buffer, the
  loader uses it for that tensor and skips its normal file/mmap load. Stock
  builds are unaffected because the symbol is weak.
- `native/tools/msr_external_preload.cpp` builds `libmsr-external.so`, which uses
  the Phase 1 reader to fetch exact GGUF byte ranges for every tensor whose name
  starts with `MSR_EXTERNAL_PREFIX`.
- Rebuilt llama.cpp; core libs and tools built successfully. The known
  `llama-app` link failure (`-lllama-server-impl`, `-lllama-cli-impl` with
  `LLAMA_BUILD_SERVER=OFF`) is unchanged and unrelated.
- Ran `llama-completion` twice on MedGemma Q4_K_M with identical fixed prompt,
  seed 42, temp 0, 16 tokens, `--no-repack -lm none`:
  - stock: `benchmarks/results/external-proof-stock.txt`
  - with `LD_PRELOAD=libmsr-external.so`, `MSR_EXTERNAL_PREFIX=blk.0.`:
    `benchmarks/results/external-proof-preload.txt` + `.log`
- The hook supplied **13 tensors** of `blk.0` (≈57 MB). Output was
  **byte-identical** (`diff -q` passed). This closes FEASIBILITY §7 condition 1.

Added a §7 record to `BENCHMARKS.md`. Scope limit documented: this is a
load-time external-buffer proof, not per-token layer swapping or a bounded
whole-process RSS result. The Phase 2 generation gate remains open.

### Per-token layer refill (same session)

Extended the vendored patch with a weak `msr_streaming_prepare_layer(int layer)`
hook in `src/models/gemma3.cpp`, invoked before each layer's graph nodes are
built. Extended `libmsr-external.so` to accept comma-separated prefixes and
refill that layer's cached buffers on every graph build. Updated
`patches/0001-msr-external-buffer-hook.patch` to cover both source files.

Ran MedGemma Q4_K_M with the same fixed prompt/seed/settings and
`MSR_EXTERNAL_PREFIX=blk.0.,...,blk.7.` (8 layers, 104 tensors):

- Output byte-identical to stock (`diff -q` passed).
- Prepare hook fired for all 34 layers; only the 8 cached ones were refilled.
- Whole-process peak RSS (sampled from `/proc/<pid>/status` `VmHWM`):
  - stock: **2,708,208 kB**
  - 8-layer refill: **3,159,392 kB**
- Logs: `benchmarks/results/stream8-preload.{txt,log}`.

**Honest result:** per-token buffer refresh works and preserves correctness,
but RSS increased because the normal loader still holds non-external weights and
the external buffers are additional. This does **not** yet prove a bounded 2 GiB
footprint, so Phase 2 remains open. Added this as `BENCHMARKS.md` §8.

Open design work: avoid duplicate residency — either stage *all* weights from
one bounded arena with a true per-layer swap that releases the previous layer,
or keep non-streamed layers in a revoked mmap window so only the external arena
counts toward the budget.

### Failed experiment — per-node mmap revoke (same session)

You chose the windowed-mmap strategy. Implemented:

- Weak `msr_streaming_node_done()` in `ggml/src/ggml-cpu/ggml-cpu.c`.
- Weak `msr_streaming_layer_boundary(layer, tensor)` in `gemma3.cpp` to tag each
  layer's final graph node.
- Provider resolves the model mmap base from `/proc/self/maps`, computes each
  `blk.N` file range from the GGUF, and calls `madvise(MADV_DONTNEED)` when that
  layer's boundary node completes.
- Patch file updated; llama.cpp rebuilt successfully.

Result: **failed.** With `-lm mmap`, 8 external layers and only 4 tokens, the run
hit the 120 s timeout (`rc=124`, ~109 s user CPU), with revoke logs for all 34
layers and no output. Per-node revocation on this FUSE-backed mmap is not viable
as implemented. Recorded as `BENCHMARKS.md` §9 with logs
`benchmarks/results/window4.{txt,log}`. No RSS claim from this path.

Phase 2 remains open. Next candidates: revoke once per token at graph start
(cheaper, but may not lower peak), or move fully to `O_DIRECT`/arena staging
without mmap so no duplicate loader residency exists.

---

## 7. STOP POINT / HANDOFF (user decision, same session)

You instructed: **stop here and update `updates.md` for the next step.** No
further code changes will be made in this session.

### Final state

- **Phase 0:** complete (`FEASIBILITY.md`, ADR-001, benchmarks).
- **Phase 1:** complete; native GGUF reader/parser/integrity/inspector, 9
  fixtures, byte-exact independent checks, `MODEL_COMPATIBILITY.md`.
- **Phase 2:** foundations complete plus two correctness proofs:
  1. load-time external buffer, 13 `blk.0` tensors, output identical to stock;
  2. per-token refill, 8 layers / 104 tensors, output identical to stock.
  Both CTest suites pass. **Gate still open** because no bounded whole-process
  RSS was demonstrated.
- **Failed experiment:** per-node `MADV_DONTNEED` on mmap layers hung; logged in
  `BENCHMARKS.md` §9.
- Asset root: `/storage/emulated/0/Download/gemma` (moved from
  `/storage/emulated/0/gemma`).
- Qwen sanity model relocated to `/storage/emulated/0/Download/qwrn/`.
- All source changes to vendored llama.cpp are summarised in
  `patches/0001-msr-external-buffer-hook.patch`.

### Next step (recommended, not started)

Implement **full O_DIRECT / bounded-arena staging** so llama.cpp's loader does
not keep a second resident copy of any weight:

1. Extend the weak provider so *all* tensors are supplied from one arena whose
   size is the configured budget (e.g. 2 GiB), with the remaining payload
   streamed per layer and the previous layer's buffer released before the next
   is filled.
2. Remove the failed per-node mmap revoke path from the active configuration
   (keep the patch/notes as a documented failed experiment).
3. Re-run the fixed-prompt greedy comparison (stock vs streamed) and sample
   whole-process `VmHWM`/PSS; require streamed peak < 2.313 GiB payload.
4. Add the resulting row to `BENCHMARKS.md` and write the Phase 2 gate verdict
   before starting Phase 3.

Alternative if arena staging proves too invasive: revoke mmap ranges once at
graph start (cheap) and measure whether peak RSS drops; this is likely weaker
evidence because all non-external layers are still touched within one token.









---

## 8. Phase 2 gate closed — bounded-arena staging (2026-11-17, same day)

This session continued the arena work that had started but was not logged.

### State found at start

- `native/tools/msr_external_preload.cpp` had already been rewritten for a
  single-arena `MSR_NO_ALLOC` design, but the vendored loader wiring was
  incomplete and nine debug logs (`arena*.log`) ended in a hang or SIGSEGV.
- `patches/0001-msr-external-buffer-hook.patch` was stale: it did not contain the
  `llama-model.cpp` no-alloc binding or the `ggml-cpu.c` node hooks.

### Root causes found and fixed

1. **Compute buffers were never allocated.** llama.cpp's `no_alloc` makes
   `graph_reserve()` do a size-only reservation, so
   `ggml_gallocr_alloc_graph()` dereferenced `galloc->buffers[0] == NULL`
   (trace from logcat + `addr2line`: `ggml_gallocr_alloc_graph` ←
   `ggml_backend_sched_alloc_graph` ← `llama_context::process_ubatch`).
2. **`no_alloc` is all-or-nothing.** It also disabled KV-cache allocation, so
   `cache_k_l0` got a zero-size dummy buffer and tripped
   `GGML_ASSERT(tensor->buffer == NULL)` in `ggml_backend_tensor_alloc`.
   Fix: a new `weights_no_alloc` flag (`ml.no_alloc || MSR_NO_ALLOC`) used only
   in `llama_model_base::load_tensors`; `mparams.no_alloc` is no longer forced,
   so KV cache and scheduler buffers follow the normal path.
3. **Per-layer refill markers never matched.** The tensor registered by
   `msr_streaming_layer_first_node()` is absorbed by fused ops (e.g. the
   norm+scale MUL disappears), so pointer lookups in compute missed every layer
   and no refill happened. Fix: refill on the `-<layer>` suffix of graph node
   names (`norm-33`, `Qcur-7`, …), which fusion preserves.

Also removed all temporary `MSR_DEBUG`/`MSR_ALLOC` instrumentation from
`ggml-alloc.c`, `llama-context.cpp`, and `gemma3.cpp`, and regenerated
`patches/0001-msr-external-buffer-hook.patch` from the live diff (4 files:
`ggml-cpu.c`, `llama-model-loader.cpp`, `llama-model.cpp`, `gemma3.cpp`).

### Measured result

Fixed prompt, seed 42, temp 0, `-t 4 -lm none --no-repack -c 2048`, 14 prompt
tokens + 7 generated, three runs per configuration:

| Configuration | Streamed layers | Peak VmHWM (median) | Spread | Output |
| --- | ---: | ---: | ---: | --- |
| Stock loader | 0 | 2,645.6 MiB | 0.6 MiB | reference |
| Arena, 2 GiB budget | 7 | 2,312.2 MiB | single run | byte-identical |
| Arena, 1 GiB budget | 26 | **1,307.4 MiB** | 0.5 MiB | byte-identical |

All six arena runs reproduced the stock greedy tokens exactly
(`thought\n1.  **Identify`). At the 1 GiB budget, generation drops from
9.91 to 2.34 tok/s — the expected cost of re-reading ~1.5 GiB per token.

### Gate verdict

**Phase 2 exit gate: PASS.** A real inference path completes generation with 26
of 34 layers streamed through one 60.6 MB buffer, whole-process peak 1,307 MiB
(50.6 % below the stock loader, well under the 2,313 MiB payload), measured
externally rather than asserted, with identical output.

Full write-up and caveats: `BENCHMARKS.md` §10. Raw data:
`benchmarks/results/gate2-{stock,arena1g}-r{1,2,3}.{json,txt}` and
`benchmarks/results/phase2-arena-{2g,1g}-2k.{json,txt}`. Both CTest targets pass.

### Repository

The project is now under version control and pushed to
`https://github.com/RavinEduard/Mobility_Engine` (private, `main`). Vendored
llama.cpp is a submodule pinned at `ef6bc6f`; the hook patch stays in
`patches/` so the submodule is clean. Fresh clone:
`git clone --recursive ... && scripts/setup.sh`. New `docs/PORTABILITY.md`
records platform status: Android/arm64 tested; Linux/macOS expected but
untested; Windows needs a Win32 file backend plus a `__attribute__((weak))`
replacement (2–4 day estimate).

### Next step (not started)

Phase 3 — asynchronous read queue + deterministic next-layer prefetch, measured
A/B at the 1 GiB budget. Lowering the budget further (512 MiB) and repeating on
lower-RAM hardware are Phase 7 rows; no 6–8 GiB claim is made yet.

## 9. Phase 3 gate closed — async reads + two-slot prefetch (2026-11-17, same day)

### What changed

- New scheduler module: `native/include/msr/scheduler/async_reader.hpp`,
  `native/src/scheduler/async_reader.cpp`.
  - `AsyncReader`: bounded queue (default 32) over `ModelFile::pread_exact`,
    1–16 worker threads, per-tag completion, `drain()`, `cancel_pending()`
    (queued reads become `Cancelled`; in-flight finishes), and counters for
    bytes, I/O time, compute-thread wait, submit blocking, peak depth.
  - `LayerPrefetcher`: groups the 13 tensor ranges of a layer, supports a
    synchronous first layer, per-layer waits, cancellation and reset.
  - Portable by design (Android FUSE has no usable io_uring/AIO here); the
    interface does not assume completion order.
- Provider (`native/tools/msr_external_preload.cpp`):
  - `MSR_PREFETCH=1` → two stream slots; streamed layer `k` binds to slot
    `k % 2`. At the first node of layer `k`: wait for its prefetched ranges (or
    read synchronously), then submit layer `k+1` into the other slot.
  - `prepare_layer(0)` schedules the first streamed layer during graph build.
  - `MSR_READ_THREADS=N` (default 1) controls read fan-out.
  - `msr_streaming_cancel()` exported for Phase 4; no runtime caller yet.
  - Peak weight bytes stay under budget; the planner streams 26 → 28 layers at
    1 GiB to pay for the second slot.
- New CTest `scheduler_async` (bounded depth, parallel completion, unknown tags,
  cancellation, multi-range layers, sync first layer). All 3 suites pass.

### Measured (5 interleaved runs per config, 4 s cooldown, medians with spread)

1 GiB budget, same device/model/flags as the Phase 2 gate:

| Configuration | Streamed | VmHWM | eval tok/s | prompt tok/s | wall s |
| --- | ---: | ---: | ---: | ---: | ---: |
| Prefetch off | 26 | 1,306.9 (0.5) | 2.06 (0.29) | 12.11 (3.50) | 7.80 |
| Prefetch, 1 reader | 28 | 1,256.2 (0.6) | 2.27 (0.24) | 16.62 (4.69) | 6.94 |
| Prefetch, 4 readers | 28 | **1,255.8 (0.6)** | **3.00 (0.44)** | 15.15 (3.32) | **6.11** |

1.5 GiB budget: off 1,784.1 MiB / 2.10 tok/s → 4 readers 1,790.8 MiB /
3.74 tok/s (+78.1 %).

Compute-thread wait on reads at 1 GiB: 3.82 s → 2.69 s (1 reader) → 1.42 s
(4 readers), while read volume rose 7.7 %. Output is byte-identical to stock at
`-n 24` with 1 and 4 readers.

### Gate verdict

**Phase 3 exit gate: PASS.** Correctness proven by identical greedy output,
prefetch gains +45.6 % generation at 1 GiB (+78.1 % at 1.5 GiB), and the memory
cap is still respected (peak weight 1,020,289,024 B ≤ 1,073,741,824 B; whole
process 1,255.8 MiB, −3.9 % vs prefetch off and −52.5 % vs stock).

Notable: **1 GiB + prefetch beats 1.5 GiB without it** in both memory and speed
(1,255.8 MiB / 3.00 tok/s vs 1,784.1 MiB / 2.10 tok/s). The larger resident
arena leaves less page cache, which is why the bigger budget alone did not help.

Full write-up and caveats: `BENCHMARKS.md` §11. Raw data:
`benchmarks/results/phase3-warm-1g/`, `benchmarks/results/phase3-warm-15g/`.

### Limitations recorded

- Warm page cache only; cold-storage behaviour could not be forced
  (`posix_fadvise DONTNEED` is a no-op on this FUSE mount, no root).
- No lower-RAM device tested; `token_embd` (525 MiB) sets a ~600–720 MiB floor.
- Cancellation is tested but not yet wired to a runtime abort path.
- mmproj/vision remains Phase 6.

### Repository

The project is now under version control and pushed to
`https://github.com/RavinEduard/Mobility_Engine` (private, `main`). Vendored
llama.cpp is a submodule pinned at `ef6bc6f`; the hook patch stays in
`patches/` so the submodule is clean. Fresh clone:
`git clone --recursive ... && scripts/setup.sh`. New `docs/PORTABILITY.md`
records platform status: Android/arm64 tested; Linux/macOS expected but
untested; Windows needs a Win32 file backend plus a `__attribute__((weak))`
replacement (2–4 day estimate).

### Next step (not started)

Phase 4 — JNI + Kotlin shell over the streaming provider, including wiring
`msr_streaming_cancel()` to UI/user cancellation. Phase 5 (thermal-aware A/B) is
still open; the interleaved sweeps reduce but do not remove thermal noise.

## 10. Phase 4 in progress — JNI bridge tested, app shell written (2026-11-17, same day)

### What changed

- Stable provider C ABI: `native/include/msr/msr_external_abi.h`
  (`msr_external_configure`, `msr_external_get_stats`). The LD_PRELOAD path is
  unchanged; env vars remain the fallback.
- JNI bridge: `native/jni/msr_jni.cpp` + `native/jni/CMakeLists.txt`. The
  provider, `msr_core` and static llama.cpp are linked **into** one
  `libmsr_jni.so`, so the weak hooks resolve without `LD_PRELOAD` (unavailable
  in an app). API: `create/load/generate/cancel/destroy/stats/lastError`.
  llama's CPU abort callback plus `msr_streaming_cancel()` implement
  cancellation; token sink can also stop generation by returning false.
- Java mirror (`native/jni/java/dev/msr/engine/`) and headless CLI
  (`scripts/jni_smoke.sh`) test the **exact** JNI symbols the Kotlin app uses.
- Android shell sources in `android/`: Kotlin `MsrEngine`/`StreamingService`/
  `MainActivity`, manifest (SAF + foreground service), layout, Gradle files.
  Framework-only UI (no AndroidX).
- Docs: `docs/native-api.md`, `docs/device-compatibility.md`,
  `android/README.md`. `BENCHMARKS.md` §12 records the smoke test.

### Measured on the device

JNI CLI, 1 GiB budget, prefetch + 4 readers, 8 tokens, `-Xmx256m`:

- Output identical to stock; `stats()` reports 1,020,289,024 B peak weight
  bytes (inside budget), 28/34 layers streamed, TTFT ~0.7–1.4 s.
- Cancellation works both via the token sink (stopped after 3 tokens) and from
  another thread mid-decode (stopped after 1 token). A first thread-cancel run
  surfaced a native exception when `llama_decode` returned the abort status;
  fixed by treating `abort_now()` as graceful termination.
- External sampler: JVM process VmHWM 3,312.0 MiB / PSS 2,605.3 MiB — JVM
  reservations dominate; not comparable to the native CLI peaks.

### Gate status and limits

**Phase 4 exit gate is NOT passed.** Only the JNI half is verified. The Kotlin
sources are unbuilt (no SDK/NDK/Gradle here), so no debug APK, no install, no
UI end-to-end run, and no UI-level cancellation/stats display. Also unresolved
for the NDK build: `integrity.cpp`'s OpenSSL dependency (NDK has no OpenSSL),
16 KB-page ELF flags, and the final SAF copy verification. See
`docs/device-compatibility.md` for the concrete path to the APK gate.
