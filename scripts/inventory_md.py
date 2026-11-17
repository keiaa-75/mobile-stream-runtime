#!/usr/bin/env python3
"""Render a GGUF inspect JSON into a Markdown model-inventory document."""
from __future__ import annotations

import json
import sys


def fmt_bytes(n: int) -> str:
    if n >= 1 << 30:
        return f"{n / (1 << 30):.3f} GiB"
    if n >= 1 << 20:
        return f"{n / (1 << 20):.2f} MiB"
    return f"{n / 1024:.1f} KiB"


def main(src, dst, title):
    d = json.load(open(src))
    m = d["metadata"]
    out = [f"# Model inventory — {title}", ""]
    out += [
        "| Property | Value |",
        "| --- | --- |",
        f"| File | `{d['file']}` |",
        f"| Bytes | {d['file_bytes']:,} |",
        f"| GGUF version | {d['version']} |",
        f"| Architecture | `{d['architecture']}` |",
        f"| Name | {d['name']} |",
        f"| Tensors | {d['tensor_count']} |",
        f"| Metadata KV pairs | {d['kv_count']} |",
        f"| Alignment | {d['alignment']} B |",
        f"| Header bytes | {d['header_bytes']:,} |",
        f"| Data start | {d['data_start']:,} |",
        f"| Payload bytes | {d['payload_bytes']:,} ({fmt_bytes(d['payload_bytes'])}) |",
        f"| Trailing padding | {d['padding_bytes']:,} |",
        f"| Overlapping tensors | {len(d['overlaps'])} |",
        "",
        "## Bytes by quantization type",
        "",
        "| Type | Bytes | Share |",
        "| --- | ---: | ---: |",
    ]
    for k, v in sorted(d["byte_by_type"].items(), key=lambda kv: -kv[1]):
        out.append(f"| {k} | {v:,} | {v / d['payload_bytes'] * 100:.2f}% |")

    out += ["", "## Metadata", "", "| Key | Value |", "| --- | --- |"]
    for k in sorted(m):
        v = m[k]
        if isinstance(v, dict) and "__len__" in v:
            s = f"*array[{v['__len__']}]* e.g. `{v['__head__'][:2]}`"
        else:
            s = str(v).replace("|", "\\|").replace("\n", " ")
            if len(s) > 160:
                s = s[:157] + "..."
        out.append(f"| `{k}` | {s} |")

    out += ["", "## Tensor table", "",
            "Offsets are relative to the start of the tensor-data region.",
            "",
            "| # | Tensor | Dims | Type | Offset | Bytes |",
            "| ---: | --- | --- | --- | ---: | ---: |"]
    for i, t in enumerate(d["tensors"]):
        dims = "×".join(str(x) for x in t["dims"])
        out.append(f"| {i} | `{t['name']}` | {dims} | {t['type']} | "
                   f"{t['offset']:,} | {t['bytes']:,} |")
    out.append("")
    open(dst, "w").write("\n".join(out))
    print(f"wrote {dst} ({len(out)} lines)")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3])
