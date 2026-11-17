#!/usr/bin/env python3
"""Read-only GGUF inspector.

Parses the GGUF header, metadata KVs and tensor-info table using pread-style
positional reads so that no tensor payload is ever loaded into memory.
Emits a human summary or JSON.

Usage:
    gguf_inspect.py MODEL.gguf [--json out.json] [--max-tensors N]
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

GGUF_MAGIC = 0x46554747  # "GGUF" little-endian
GGML_QUANT = {
    0: "F32", 1: "F16", 2: "Q4_0", 3: "Q4_1", 6: "Q5_0", 7: "Q5_1",
    8: "Q8_0", 9: "Q8_1", 10: "Q2_K", 11: "Q3_K", 12: "Q4_K", 13: "Q5_K",
    14: "Q6_K", 15: "Q8_K", 16: "IQ2_XXS", 17: "IQ2_XS", 18: "IQ3_XXS",
    19: "IQ1_S", 20: "IQ4_NL", 21: "IQ3_S", 22: "IQ2_S", 23: "IQ4_XS",
    24: "I8", 25: "I16", 26: "I32", 27: "I64", 28: "F64", 29: "IQ1_M",
    30: "BF16", 34: "TQ1_0", 35: "TQ2_0",
}
# bytes per block for size estimation (block size, type size)
BLOCK = {
    0: (1, 4), 1: (1, 2), 2: (32, 18), 3: (32, 20), 6: (32, 22), 7: (32, 24),
    8: (32, 34), 9: (32, 40), 10: (256, 84), 11: (256, 110), 12: (256, 144),
    13: (256, 176), 14: (256, 210), 15: (256, 292), 16: (256, 66),
    17: (256, 74), 18: (256, 98), 19: (256, 50), 20: (32, 18),
    21: (256, 110), 22: (256, 82), 23: (256, 136), 24: (1, 1), 25: (1, 2),
    26: (1, 4), 27: (1, 8), 28: (1, 8), 29: (256, 56), 30: (1, 2),
    34: (256, 54), 35: (256, 66),
}
# Feb 2025 GGML type ids moved; map a few current ones as observed in files.
TYPE_NAMES = {
    0: "F32", 1: "F16", 2: "Q4_0", 3: "Q4_1", 6: "Q5_0", 7: "Q5_1",
    8: "Q8_0", 9: "Q8_1", 10: "Q2_K", 11: "Q3_K", 12: "Q4_K", 13: "Q5_K",
    14: "Q6_K", 15: "Q8_K", 16: "IQ2_XXS", 17: "IQ2_XS", 18: "IQ3_XXS",
    19: "IQ1_S", 20: "IQ4_NL", 21: "IQ3_S", 22: "IQ2_S", 23: "IQ4_XS",
    24: "I8", 25: "I16", 26: "I32", 27: "I64", 28: "F64", 29: "IQ1_M",
    30: "BF16", 34: "TQ1_0", 35: "TQ2_0", 39: "MXFP4",
}

VAL_TYPES = {
    0: "u8", 1: "i8", 2: "u16", 3: "i16", 4: "u32", 5: "i32", 6: "f32",
    7: "bool", 8: "str", 9: "arr", 10: "u64", 11: "i64", 12: "f64",
}
FMT = {
    0: "<B", 1: "<b", 2: "<H", 3: "<h", 4: "<I", 5: "<i", 6: "<f",
    7: "<B", 10: "<Q", 11: "<q", 12: "<d",
}


class Reader:
    def __init__(self, fh):
        self.fh = fh

    def read(self, n: int) -> bytes:
        b = self.fh.read(n)
        if len(b) != n:
            raise EOFError(f"wanted {n} bytes, got {len(b)}")
        return b

    def scalar(self, t: int):
        return struct.unpack(FMT[t], self.read(struct.calcsize(FMT[t])))[0]

    def string(self) -> str:
        n = struct.unpack("<Q", self.read(8))[0]
        if n > 1 << 28:
            raise ValueError(f"absurd string length {n}")
        return self.read(n).decode("utf-8", "replace")

    def value(self, t: int):
        if t == 8:
            return self.string()
        if t == 9:
            et = struct.unpack("<I", self.read(4))[0]
            n = struct.unpack("<Q", self.read(8))[0]
            if n <= 16 and et not in (8, 9):
                return [self.value(et) for _ in range(n)]
            head = [self.value(et) for _ in range(min(n, 6))]
            for _ in range(n - min(n, 6)):
                self.value(et)  # consume without retaining
            return {"__len__": n, "__head__": head}
        return self.scalar(t)


def n_elements(dims):
    n = 1
    for d in dims:
        n *= d
    return n


def tensor_bytes(ggml_type: int, dims):
    cnt = n_elements(dims)
    bs, ts = BLOCK.get(ggml_type, (1, 0))
    if ts == 0:
        return 0, 0
    if cnt % bs != 0:
        return 0, bs
    return (cnt // bs) * ts, bs


def inspect(path: Path, max_tensors: int = 0):
    with path.open("rb") as fh:
        r = Reader(fh)
        magic = struct.unpack("<I", r.read(4))[0]
        if magic != GGUF_MAGIC:
            raise SystemExit(f"{path}: bad magic {magic:#x}")
        version = struct.unpack("<I", r.read(4))[0]
        n_tensors = struct.unpack("<Q", r.read(8))[0]
        n_kv = struct.unpack("<Q", r.read(8))[0]

        meta = {}
        for _ in range(n_kv):
            key = r.string()
            vt = struct.unpack("<I", r.read(4))[0]
            meta[key] = r.value(vt)

        tensors = []
        for i in range(n_tensors):
            name = r.string()
            nd = struct.unpack("<I", r.read(4))[0]
            dims = list(struct.unpack("<" + "Q" * nd, r.read(8 * nd)))
            ttype = struct.unpack("<I", r.read(4))[0]
            off = struct.unpack("<Q", r.read(8))[0]
            size, bs = tensor_bytes(ttype, dims)
            tensors.append({
                "name": name, "ndims": nd, "dims": dims,
                "type_id": ttype, "type": TYPE_NAMES.get(ttype, f"?{ttype}"),
                "offset": off, "bytes": size,
                "elements": n_elements(dims),
                "block": bs,
            })
        header_end = fh.tell()

    data_start = (header_end + int(meta.get("general.alignment", 32)) - 1) \
        // int(meta.get("general.alignment", 32)) * int(meta.get("general.alignment", 32))

    total = sum(t["bytes"] for t in tensors)
    # detect overlaps in offset space
    spans = sorted(((t["offset"], t["offset"] + t["bytes"], t["name"]) for t in tensors))
    overlaps = []
    for a, b in zip(spans, spans[1:]):
        if b[0] < a[1]:
            overlaps.append((a[2], b[2], a[1] - b[0]))

    by_type = {}
    for t in tensors:
        by_type[t["type"]] = by_type.get(t["type"], 0) + t["bytes"]

    res = {
        "file": str(path), "file_bytes": path.stat().st_size,
        "version": version, "tensor_count": n_tensors, "kv_count": n_kv,
        "alignment": meta.get("general.alignment", 32),
        "data_start": data_start,
        "header_bytes": header_end,
        "payload_bytes": total,
        "padding_bytes": path.stat().st_size - data_start - total,
        "architecture": meta.get("general.architecture"),
        "name": meta.get("general.name"),
        "quantization_version": meta.get("general.quantization_version"),
        "file_type": meta.get("general.file_type"),
        "byte_by_type": by_type,
        "overlaps": overlaps[:10],
        "tensor_type_ids": sorted({t["type_id"] for t in tensors}),
        "metadata": meta,
        "tensors": tensors[:max_tensors] if max_tensors else tensors,
    }
    return res


def human(res) -> str:
    L = []
    L.append(f"file            {res['file']}")
    L.append(f"size            {res['file_bytes']:,} B")
    L.append(f"gguf version    {res['version']}")
    L.append(f"architecture    {res['architecture']}")
    L.append(f"name            {res['name']}")
    L.append(f"tensors         {res['tensor_count']}   kv pairs {res['kv_count']}")
    L.append(f"alignment       {res['alignment']} B")
    L.append(f"header          {res['header_bytes']:,} B")
    L.append(f"data start      {res['data_start']:,} B")
    L.append(f"payload         {res['payload_bytes']:,} B "
             f"({res['payload_bytes'] / 0x40000000:.3f} GiB)")
    L.append(f"trailing pad    {res['padding_bytes']:,} B")
    L.append(f"overlaps        {len(res['overlaps'])}")
    L.append("")
    L.append("bytes by quant type:")
    for k, v in sorted(res["byte_by_type"].items(), key=lambda kv: -kv[1]):
        L.append(f"  {k:<10} {v:>14,} B  ({v / res['payload_bytes'] * 100:5.2f}%)")
    L.append("")
    L.append("notable metadata:")
    for k in sorted(res["metadata"]):
        v = res["metadata"][k]
        if isinstance(v, dict) and "__len__" in v:
            L.append(f"  {k:<44} <array of {v['__len__']} "
                     f"e.g. {str(v['__head__'][:2])[:40]}>")
            continue
        if k.startswith(("tokenizer.ggml.tokens", "tokenizer.ggml.merges")):
            L.append(f"  {k:<44} <vocab list>")
            continue
        s = str(v)
        if len(s) > 90:
            s = s[:87] + "..."
        L.append(f"  {k:<44} {s}")
    L.append("")
    L.append("first tensors:")
    for t in res["tensors"][:12]:
        L.append(f"  {t['name']:<38} {str(t['dims']):<24} {t['type']:<7} "
                 f"off={t['offset']:>12,} bytes={t['bytes']:>12,}")
    if res["tensor_count"] > 12:
        L.append(f"  ... {res['tensor_count'] - 12} more")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("--json")
    ap.add_argument("--max-tensors", type=int, default=0)
    a = ap.parse_args()
    res = inspect(Path(a.model), a.max_tensors)
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=1))
        print(f"wrote {a.json}")
    print(human(res))


if __name__ == "__main__":
    sys.exit(main())
