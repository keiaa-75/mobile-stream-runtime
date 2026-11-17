#!/usr/bin/env python3
"""Derive a streaming memory budget from a GGUF tensor table.

Answers three questions Phase 0 needs:
  1. How many bytes must be resident per transformer layer?
  2. What is the largest unavoidable (non-layer) blob?
  3. How does KV cache scale with context, per token and at common sizes?

Usage: layer_budget.py INSPECT.json
"""
from __future__ import annotations

import json
import re
import sys

KB = 1024


def fmt(n: float) -> str:
    if n >= 1 << 30:
        return f"{n / (1 << 30):8.3f} GiB"
    if n >= 1 << 20:
        return f"{n / (1 << 20):8.1f} MiB"
    return f"{n / KB:8.1f} KiB"


def main(path):
    d = json.load(open(path))
    meta = d["metadata"]
    arch = d["architecture"]
    tensors = d["tensors"]

    blocks = {}
    non_block = []
    for t in tensors:
        m = re.match(r"blk\.(\d+)\.", t["name"])
        if m:
            blocks.setdefault(int(m.group(1)), []).append(t)
        else:
            non_block.append(t)

    print(f"== {path}")
    print(f"arch={arch}  tensors={len(tensors)}  payload={fmt(d['payload_bytes'])}")
    print()

    n_layers = int(meta.get(f"{arch}.block_count", 0))
    print(f"layers={n_layers}  blocks_found={len(blocks)}")
    per = {i: sum(t["bytes"] for t in ts) for i, ts in blocks.items()}
    if per:
        vals = sorted(per.items())
        print(f"per-layer bytes: min={fmt(min(per.values()))} "
              f"max={fmt(max(per.values()))} mean={fmt(sum(per.values()) / len(per))}")
        print("  layer  bytes      largest tensor in layer")
        for i, b in vals[:3] + [("...", 0)] + vals[-2:]:
            if i == "...":
                print("    ...")
                continue
            big = max(blocks[i], key=lambda t: t["bytes"])
            print(f"  {i:>5}  {fmt(b)}  {big['name']} {fmt(big['bytes'])}")
        print()
        print(f"all-layer total: {fmt(sum(per.values()))}")
        print(f"max single layer: {fmt(max(per.values()))}  "
              f"(streaming floor per token if no cache)")
    print()

    print("non-layer tensors (must be resident or separately streamed):")
    for t in sorted(non_block, key=lambda t: -t["bytes"])[:10]:
        print(f"  {t['name']:<32} {str(t['dims']):<22} {t['type']:<6} {fmt(t['bytes'])}")
    print(f"  non-layer total: {fmt(sum(t['bytes'] for t in non_block))}")
    print()

    big = max(tensors, key=lambda t: t["bytes"])
    print(f"largest tensor overall: {big['name']} {fmt(big['bytes'])} ({big['type']})")
    print()

    # KV cache: gemma3 uses separate key/value head dims
    hc = int(meta.get(f"{arch}.attention.head_count_kv", 0))
    kl = int(meta.get(f"{arch}.attention.key_length", 0))
    vl = int(meta.get(f"{arch}.attention.value_length", 0))
    if arch == "clip":
        return
    if hc and kl and vl:
        per_tok_f16 = n_layers * hc * (kl + vl) * 2
        print("KV cache model (f16):")
        print(f"  layers={n_layers} kv_heads={hc} k_len={kl} v_len={vl}")
        print(f"  per token          {fmt(per_tok_f16)}   ({per_tok_f16} B)")
        for ctx in (2048, 4096, 8192, 16384, 32768):
            print(f"  ctx {ctx:>6}         {fmt(per_tok_f16 * ctx)}")
        print()
        print(f"  at ctx=8192 KV is {per_tok_f16 * 8192 / d['payload_bytes'] * 100:.1f}% "
              f"of the model file size")
    else:
        print("KV params not found in metadata; check arch keys.")

    # Dense streaming ceiling
    print()
    print("Full-streaming bytes/token (no weight cache, dense model):")
    print(f"  {fmt(d['payload_bytes'])} per generated token")
    for gbps in (0.5, 1.0, 2.0, 3.0, 4.0):
        print(f"  at {gbps:>4.1f} GB/s effective read: "
              f"{gbps * 1e9 / d['payload_bytes']:.2f} tok/s ceiling")


if __name__ == "__main__":
    main(sys.argv[1])
