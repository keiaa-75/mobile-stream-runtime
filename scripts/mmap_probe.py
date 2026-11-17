#!/usr/bin/env python3
"""Demonstrate mmap residency behaviour on the real GGUF file.

Phase 0 question: does memory-mapping a model file keep RAM flat? This probe
answers it empirically on this device instead of assuming.

Mode A (default): mmap the whole file, touch every 4 KiB page sequentially,
sample RSS every 128 MiB. Shows the resident set climbing toward file size.

Mode B (--windowed): mmap in a sliding window with MADV_DONTNEED behind the
cursor. Shows whether a bounded mapping window keeps RSS flat.

Usage: mmap_probe.py MODEL.gguf [--windowed] [--limit-mib N]
"""
from __future__ import annotations

import argparse
import mmap
import os
import resource
import time

MiB = 1 << 20
PAGE = 4096


def rss_mib():
    with open("/proc/self/statm") as fh:
        pages = int(fh.read().split()[1])
    return pages * os.sysconf("SC_PAGE_SIZE") / MiB


def faults():
    ru = resource.getrusage(resource.RUSAGE_SELF)
    return ru.ru_minflt, ru.ru_majflt


def hwm_mib():
    with open("/proc/self/status") as fh:
        for line in fh:
            if line.startswith("VmHWM:"):
                return int(line.split()[1]) / 1024
    return 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("--windowed", action="store_true")
    ap.add_argument("--window-mib", type=int, default=256)
    ap.add_argument("--limit-mib", type=int, default=0)
    a = ap.parse_args()

    size = os.path.getsize(a.model)
    if a.limit_mib:
        size = min(size, a.limit_mib * MiB)

    base_rss = rss_mib()
    f0, mf0 = faults()
    print(f"file        {a.model}")
    print(f"mapped      {size / MiB:.0f} MiB   mode="
          f"{'windowed' if a.windowed else 'whole-file touch'}")
    print(f"rss before  {base_rss:.1f} MiB")

    t0 = time.perf_counter()
    peak = base_rss
    samples = []

    with open(a.model, "rb") as fh:
        if not a.windowed:
            mm = mmap.mmap(fh.fileno(), 0, prot=mmap.PROT_READ)
            try:
                touched = 0
                mark = 128 * MiB
                for off in range(0, size, PAGE):
                    touched += mm[off]
                    if off >= mark:
                        r = rss_mib()
                        peak = max(peak, r)
                        samples.append((off / MiB, r))
                        mark += 128 * MiB
            finally:
                mm.close()
        else:
            win = a.window_mib * MiB
            for off in range(0, size, win):
                n = min(win, size - off)
                mm = mmap.mmap(fh.fileno(), n, prot=mmap.PROT_READ,
                               offset=off - (off % mmap.ALLOCATIONGRANULARITY))
                try:
                    acc = 0
                    for p in range(0, n, PAGE):
                        acc += mm[p]
                    mm.madvise(mmap.MADV_DONTNEED)
                finally:
                    mm.close()
                r = rss_mib()
                peak = max(peak, r)
                samples.append(((off + n) / MiB, r))

    dt = time.perf_counter() - t0
    f1, mf1 = faults()
    print(f"elapsed     {dt:.2f} s   ({size / dt / MiB:.0f} MiB/s over touched span)")
    print(f"minor faults {mf1 - f0:,}   major faults {mf1 - mf0:,}")
    print(f"rss after   {rss_mib():.1f} MiB   peak {peak:.1f} MiB   VmHWM {hwm_mib():.1f} MiB")
    print(f"resident/file {peak / (size / MiB) * 100:.1f}% of touched span")
    print()
    print("rss samples (MiB touched -> MiB RSS):")
    step = max(1, len(samples) // 14)
    for t, r in samples[::step]:
        print(f"  {t:8.0f} -> {r:8.1f}")


if __name__ == "__main__":
    main()
