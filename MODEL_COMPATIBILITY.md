# MODEL COMPATIBILITY

Status: **Phase 1 inventory and read-path validation complete**. Inference
support is *not* claimed here; that is gated by Phases 5 and 6.

Asset root: `/storage/emulated/0/Download/gemma`

---

## 1. Files on disk

| Role | File | Bytes | Source repo (from plan) | License note |
| --- | --- | ---: | --- | --- |
| Language model | `medgemma-1.5-4b-it.Q4_K_M.gguf` | 2,489,894,624 | `mradermacher/medgemma-1.5-4b-it-GGUF` | Health AI Developer Foundations terms |
| Vision projector | `mmproj-medgemma-1.5-4b-it-BF16.gguf` | 851,251,744 | `billylo/medgemma-1.5-4b-it-mmproj-gguf` | Apache-2.0 (project); base model terms apply |

Self-computed SHA-256 (recorded in `checksums.txt`, cross-check with upstream
still open):

```
ee5121f1b6ffda000f65bcf14b86a653f1beae2438663381f61980e3cf639454  medgemma-1.5-4b-it.Q4_K_M.gguf
d2adfa43e4e2db7ddd8667c0f543844f7457121068f5abd96359bea3c2caf7e1  mmproj-medgemma-1.5-4b-it-BF16.gguf
```

`model-inspect --verify` now reproduces both hashes byte-for-byte.

---

## 2. Parsed GGUF facts (native parser, Phase 1)

| Field | Main model | mmproj |
| --- | --- | --- |
| GGUF version | 3 | 3 |
| Architecture (`general.architecture`) | `gemma3` | `clip` |
| Name | `Medgemma 1.5 4b It` | `Medgemma 1.5 4b It` |
| `general.file_type` | 15 | 32 |
| Quantization version | 2 | 2 |
| Metadata KVs | 48 | 25 |
| Tensors | 444 | 439 |
| Alignment | 32 B | 32 B |
| Header bytes | 6,541,777 | 25,677 |
| Data start | 6,541,792 | 25,696 |
| Tensor payload | 2,483,352,832 B (2.313 GiB) | 851,226,048 B (0.793 GiB) |
| Quant mix | Q4_K 1,529,118,720 · Q6_K 952,761,600 · F32 1,472,512 | BF16 822,140,928 · F32 23,186,880 · F16 5,898,240 |
| Unsupported types found | none | none |
| Offset/overlap/block validation | pass | pass |

Architecture details from metadata:

- Main: context length 131,072; embedding length 2,560; 34 dense layers;
  tied `token_embd.weight` (550,636,800 B, Q6_K).
- Projector: `clip.has_vision_encoder = 1`; image size 896; patch size 14;
  projection dim 2,560; 27 vision blocks; vision embedding length 1,152.

Full tensor tables: `docs/model-inventory/medgemma-q4km.{json,md}` and
`docs/model-inventory/mmproj-bf16.{json,md}`.

---

## 3. Bounded read evidence (Phase 1 exit gate)

Native `model-inspect` reads a selected tensor into a fixed buffer; an
independent Python extraction of the same byte range is compared by
`tests/byte_exact_check.sh`.

| Model | Tensor | Bytes | SHA-256 of read | Direct I/O |
| --- | --- | ---: | --- | --- |
| Main | `output_norm.weight` | 10,240 | `ff0f152b2ca0307c9a8a3fd867da8cd8c2b99f356fec90b35fd6dfcfc27efa5c` | no and `--direct` |
| Projector | `mm.input_projection.weight` | 5,898,240 | `cd7986d234e516e5867957dbfd4cde215d7164b3bed62c6c6f84db91e7e9e8f8` | no |

Both independent comparisons returned `byte-exact OK`. Whole-file hashes also
match `checksums.txt`.

---

## 4. Not yet established

- Upstream `SHA256SUMS` / LFS pointer cross-check (hashes are self-computed).
- Tokenizer / Gemma-3 chat template fidelity and greedy-output equivalence.
- Whether the llama.cpp build used for the baseline accepts this exact
  Q4_K_M + BF16 projector pairing in an image run.
- Any memory or throughput number for the streamed path; Phase 2 onward.
- Clinical accuracy — explicitly out of scope.

---

## 5. Reproduction

```bash
ASSET=/storage/emulated/0/Download/gemma
cmake -S native -B native/build -G Ninja
cmake --build native/build -j4
ctest --test-dir native/build --output-on-failure

native/build/model-inspect "$ASSET/medgemma-1.5-4b-it.Q4_K_M.gguf" \
  --tensor output_norm.weight \
  --verify "$(head -1 "$ASSET/checksums.txt" | awk '{print $1}')"

tests/byte_exact_check.sh "$ASSET/medgemma-1.5-4b-it.Q4_K_M.gguf" output_norm.weight
tests/byte_exact_check.sh "$ASSET/mmproj-medgemma-1.5-4b-it-BF16.gguf" mm.input_projection.weight
```
