#!/usr/bin/env python3
"""Run a command and record its memory high-water marks.

Polls /proc/<pid>/status (VmHWM, VmRSS) and smaps_rollup (Pss) while the child
runs. Also reports wall time and, optionally, the child's stdout.

Usage:
  run_measured.py --label baseline-2k --json out.json -- llama-cli -m ... -p ...
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

MiB = 1 << 20


def read_status(pid):
    vals = {}
    try:
        with open(f"/proc/{pid}/status") as fh:
            for line in fh:
                if line.startswith(("VmHWM:", "VmRSS:", "VmPeak:")):
                    k, v = line.split(":", 1)
                    vals[k] = int(v.split()[0]) * 1024
    except (FileNotFoundError, ProcessLookupError):
        return vals
    return vals


def read_pss(pid):
    try:
        with open(f"/proc/{pid}/smaps_rollup") as fh:
            for line in fh:
                if line.startswith("Pss:"):
                    return int(line.split()[1]) * 1024
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None
    return None


def read_avail():
    try:
        with open("/proc/meminfo") as fh:
            for line in fh:
                if line.startswith("MemAvailable:"):
                    return int(line.split()[1]) * 1024
    except OSError:
        return None
    return None


def read_temp_max():
    """ hottest thermal zone, in degrees C (from milli-C) """
    hottest = 0
    import glob
    for p in glob.glob("/sys/class/thermal/thermal_zone*/temp"):
        try:
            hottest = max(hottest, int(open(p).read().strip()))
        except (OSError, ValueError):
            pass
    return hottest / 1000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--label", default="run")
    ap.add_argument("--json")
    ap.add_argument("--interval", type=float, default=0.2)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
    if not cmd:
        sys.exit("no command given")

    avail_before = read_avail()
    temp_before = read_temp_max()
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         text=True, bufsize=1)

    peak_hwm = peak_rss = peak_pss = 0
    pss_samples = []
    while p.poll() is None:
        s = read_status(p.pid)
        peak_hwm = max(peak_hwm, s.get("VmHWM", 0))
        peak_rss = max(peak_rss, s.get("VmRSS", 0))
        pss = read_pss(p.pid)
        if pss:
            peak_pss = max(peak_pss, pss)
            pss_samples.append((round(time.perf_counter() - t0, 2), pss / MiB))
        time.sleep(a.interval)

    out, _ = p.communicate()
    wall = time.perf_counter() - t0
    avail_after = read_avail()

    res = {
        "label": a.label,
        "cmd": cmd,
        "exit_code": p.returncode,
        "wall_s": round(wall, 2),
        "peak_vmhwm_mib": round(peak_hwm / MiB, 1),
        "peak_rss_mib": round(peak_rss / MiB, 1),
        "peak_pss_mib": round(peak_pss / MiB, 1),
        "memavailable_before_mib": round(avail_before / MiB, 1) if avail_before else None,
        "memavailable_after_mib": round(avail_after / MiB, 1) if avail_after else None,
        "temp_before_c": temp_before,
        "temp_after_c": read_temp_max(),
        "pss_samples": pss_samples[::max(1, len(pss_samples) // 40)],
        "output_tail": (out or "")[-4000:],
    }
    if a.json:
        with open(a.json, "w") as fh:
            json.dump(res, fh, indent=1)

    print(f"[{a.label}] exit={p.returncode} wall={wall:.2f}s "
          f"VmHWM={res['peak_vmhwm_mib']} MiB PSS={res['peak_pss_mib']} MiB "
          f"temp {temp_before}->{res['temp_after_c']}C "
          f"avail {res['memavailable_before_mib']}->{res['memavailable_after_mib']} MiB")
    print(out or "")


if __name__ == "__main__":
    main()
