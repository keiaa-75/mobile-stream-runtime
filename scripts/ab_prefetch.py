#!/usr/bin/env python3
"""A/B runner for the streaming provider: interleaves configs, then reports
medians and spread for eval/prompt throughput plus VmHWM.

Interleaving (round-robin over configs) keeps thermal drift and page-cache
state from favouring one config.

Usage:
  scripts/ab_prefetch.py --runs 5 --out-dir benchmarks/results/phase3-warm \\
      --model /path/model.gguf --prompt "The capital of France is" --tokens 8
"""
from __future__ import annotations

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MEASURED = os.path.join(ROOT, "scripts", "run_measured.py")
BIN = os.path.join(ROOT, "build", "bin", "llama-completion")
PRELOAD = os.path.join(ROOT, "native", "build", "libmsr-external.so")

EVAL_RE = re.compile(r"eval time =\s+([0-9.]+) ms /\s+(\d+) runs")
PROMPT_RE = re.compile(r"prompt eval time =\s+([0-9.]+) ms /\s+(\d+) tokens")


def parse_timings(text: str):
    ev = EVAL_RE.search(text)
    pr = PROMPT_RE.search(text)
    eval_tok_s = None
    prompt_tok_s = None
    if ev:
        n = int(ev.group(2))
        eval_tok_s = 1000.0 * n / float(ev.group(1)) if float(ev.group(1)) > 0 else None
    if pr:
        n = int(pr.group(2))
        prompt_tok_s = 1000.0 * n / float(pr.group(1)) if float(pr.group(1)) > 0 else None
    return eval_tok_s, prompt_tok_s


def stats(values):
    vals = [v for v in values if v is not None]
    if not vals:
        return None
    return {
        "n": len(vals),
        "median": round(statistics.median(vals), 2),
        "min": round(min(vals), 2),
        "max": round(max(vals), 2),
        "spread": round(max(vals) - min(vals), 2),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", required=True)
    ap.add_argument("--prompt", default="The capital of France is")
    ap.add_argument("--tokens", type=int, default=8)
    ap.add_argument("--ctx", type=int, default=2048)
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--budget", type=int, default=1 << 30)
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--cooldown", type=float, default=5.0)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--configs", default="off,prefetch-1t,prefetch-4t")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    configs = {}
    for name in args.configs.split(","):
        env = {}
        if name != "off":
            env["MSR_PREFETCH"] = "1"
            if name.startswith("prefetch-"):
                env["MSR_READ_THREADS"] = name.split("-", 1)[1].rstrip("t")
        configs[name] = env

    base_cmd = [
        sys.executable, MEASURED,
        "--label", "", "--json", "",
        "--",
        BIN, "-m", args.model,
        "-p", args.prompt,
        "-n", str(args.tokens),
        "--seed", "42", "--temp", "0",
        "-t", str(args.threads),
        "-lm", "none", "--no-repack",
        "-c", str(args.ctx),
    ]

    results = {name: [] for name in configs}
    for run in range(1, args.runs + 1):
        for name, extra_env in configs.items():
            label = f"{name}-r{run}"
            json_path = os.path.join(args.out_dir, f"{label}.json")
            cmd = list(base_cmd)
            cmd[cmd.index("--label") + 1] = label
            cmd[cmd.index("--json") + 1] = json_path

            env = dict(os.environ)
            env.setdefault("MSR_NO_ALLOC", "1")
            env["MSR_BUDGET_BYTES"] = str(args.budget)
            env["MSR_MODEL_PATH"] = args.model
            env["LD_PRELOAD"] = PRELOAD
            env.update(extra_env)

            print(f"=== {label} (budget={args.budget}) ===", flush=True)
            subprocess.run(cmd, env=env, check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            with open(json_path) as fh:
                res = json.load(fh)
            text = res.get("output_tail", "")
            eval_tok_s, prompt_tok_s = parse_timings(text)
            res.update({
                "config": name,
                "eval_tok_s": eval_tok_s,
                "prompt_tok_s": prompt_tok_s,
                "read_stats": sorted({
                    line.split(": ", 1)[1].strip()
                    for line in text.splitlines()
                    if line.startswith("msr_external: reads_submitted")
                }),
            })
            with open(json_path, "w") as fh:
                json.dump(res, fh, indent=1)
            results[name].append(res)
            print(f"    VmHWM={res['peak_vmhwm_mib']} MiB "
                  f"eval={eval_tok_s} tok/s prompt={prompt_tok_s} tok/s", flush=True)
            time.sleep(args.cooldown)

    summary = {}
    for name, runs in results.items():
        summary[name] = {
            "eval_tok_s": stats([r["eval_tok_s"] for r in runs]),
            "prompt_tok_s": stats([r["prompt_tok_s"] for r in runs]),
            "vmhwm_mib": stats([r["peak_vmhwm_mib"] for r in runs]),
            "pss_mib": stats([r["peak_pss_mib"] for r in runs]),
            "wall_s": stats([r["wall_s"] for r in runs]),
        }

    out = {
        "budget_bytes": args.budget,
        "model": args.model,
        "prompt": args.prompt,
        "tokens": args.tokens,
        "ctx": args.ctx,
        "threads": args.threads,
        "runs_per_config": args.runs,
        "configs": {k: v for k, v in configs.items()},
        "summary": summary,
    }
    with open(os.path.join(args.out_dir, "summary.json"), "w") as fh:
        json.dump(out, fh, indent=1)

    print("\nconfig        VmHWM MiB (spread)   eval tok/s (spread)   prompt tok/s (spread)")
    for name, s in summary.items():
        def fmt(d):
            return "n/a" if not d else f"{d['median']:8.2f} ({d['spread']:.2f})"
        print(f"{name:14s} {fmt(s['vmhwm_mib']):>20s} {fmt(s['eval_tok_s']):>22s} "
              f"{fmt(s['prompt_tok_s']):>24s}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
