#!/usr/bin/env python3
"""Storage read-throughput probe for on-device GGUF streaming.

Measures:
  * random aligned pread at 1/4/16/64 MiB request sizes
  * sequential read throughput over a large span
  * small-read latency (4 KiB) distribution

Caveats recorded in output: this runs over the FUSE-mounted shared storage
and is subject to page-cache effects. Without root we cannot drop caches, so
"cold" numbers are approximated by scanning a span larger than the measured
cache footprint first.

Usage: io_bench.py MODEL.gguf [--json out.json]
"""
from __future__ import annotations

import argparse
import json
import os
import random
import statistics
import time

MiB = 1 << 20


def pct(xs, p):
    if not xs:
        return 0.0
    xs = sorted(xs)
    k = min(len(xs) - 1, int(round((p / 100) * (len(xs) - 1))))
    return xs[k]


def timed(fn):
    t0 = time.perf_counter()
    n = fn()
    return n, time.perf_counter() - t0


def random_reads(fd, size, size_bytes, samples, span):
    rates, lats = [], []
    for _ in range(samples):
        off = random.randrange(0, max(1, span - size_bytes))
        off -= off % 4096
        n, dt = timed(lambda: len(os.pread(fd, size_bytes, off)))
        if n:
            rates.append(n / dt / 1e6)
            lats.append(dt * 1000)
    return {
        "chunk_mib": size,
        "median_mibs": statistics.median(rates) if rates else 0,
        "p95_ms": pct(lats, 95),
        "min_mibs": min(rates) if rates else 0,
        "max_mibs": max(rates) if rates else 0,
        "samples": len(rates),
    }


def sequential(fd, span, chunk=8 * MiB):
    off, total, t0 = 0, 0, time.perf_counter()
    while off < span:
        n = len(os.pread(fd, chunk, off))
        if not n:
            break
        total += n
        off += n
    dt = time.perf_counter() - t0
    return {"span_mib": span / MiB, "mibs": total / dt / 1e6, "seconds": dt}


def warm(fd, span, chunk=8 * MiB):
    off = 0
    while off < span:
        n = len(os.pread(fd, chunk, off))
        if not n:
            break
        off += n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("--json")
    ap.add_argument("--span-mib", type=int, default=1024)
    a = ap.parse_args()

    fd = os.open(a.model, os.O_RDONLY)
    try:
        st = os.fstat(fd)
        span = min(a.span_mib * MiB, st.st_size)
        out = {
            "file": a.model,
            "file_bytes": st.st_size,
            "span_mib": span / MiB,
            "note": ("FUSE shared storage; page cache not dropped (no root). "
                     "Treat absolute values as best-case warm numbers."),
            "sequential": [],
            "random": [],
        }

        # two sequential passes: first is as-cold-as-we-can-get
        for label in ("pass1_uncached", "pass2_warm"):
            r = sequential(fd, span)
            r["label"] = label
            out["sequential"].append(r)

        for size in (1, 4, 16, 64):
            b = size * MiB
            out["random"].append(random_reads(fd, size, b, 20, span))

        # tiny read latency
        lats = []
        for _ in range(200):
            off = random.randrange(0, span - 4096)
            off -= off % 4096
            _, dt = timed(lambda: os.pread(fd, 4096, off))
            lats.append(dt * 1000)
        out["read_4k_ms"] = {
            "median": statistics.median(lats),
            "p95": pct(lats, 95),
            "p99": pct(lats, 99),
            "max": max(lats),
        }
        out["storage_total_mib"] = st.st_size / MiB
    finally:
        os.close(fd)

    if a.json:
        with open(a.json, "w") as fh:
            json.dump(out, fh, indent=1)

    print(f"file      {out['file']}")
    print(f"span      {out['span_mib']:.0f} MiB")
    print("\nsequential (8 MiB chunks):")
    for r in out["sequential"]:
        print(f"  {r['label']:<16} {r['mibs']:7.1f} MiB/s   ({r['seconds']:.2f}s)")
    print("\nrandom aligned pread:")
    print("  chunk     median      min       max     p95 lat")
    for r in out["random"]:
        print(f"  {r['chunk_mib']:>4} MiB {r['median_mibs']:8.1f} "
              f"{r['min_mibs']:8.1f} {r['max_mibs']:8.1f} "
              f"{r['p95_ms']:9.2f} ms")
    print("\n4 KiB read latency: median {median:.2f} ms  p95 {p95:.2f} ms  "
          "p99 {p99:.2f} ms  max {max:.2f} ms".format(**out["read_4k_ms"]))


if __name__ == "__main__":
    main()
