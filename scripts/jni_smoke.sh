#!/usr/bin/env bash
# End-to-end JNI smoke test on the device (headless CLI). This exercises the
# exact native surface the Android app calls: load, greedy generate, per-token
# sink, stats, and both cancellation paths.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

model="${1:-/storage/emulated/0/Download/gemma/medgemma-1.5-4b-it.Q4_K_M.gguf}"
expected='thought'
prompt='The capital of France is'

mkdir -p native/jni/classes
javac -d native/jni/classes native/jni/java/dev/msr/engine/*.java

run_cli() {
    java -Xmx256m -Djava.library.path=native/build/jni -cp native/jni/classes \
        dev.msr.engine.MsrCli "$@"
}

echo "== greedy generation =="
out="$(run_cli "$model" "$prompt" 8 1024 4 4 1 2>/dev/null)"
echo "$out"
grep -q "$expected" <<<"$out" || { echo "unexpected output"; exit 1; }

echo "== cancel from token sink =="
run_cli "$model" "$prompt" 64 1024 4 4 1 --cancel-after-tokens 3 2>&1 | tail -2

echo "== cancel from another thread (mid-decode) =="
run_cli "$model" "$prompt" 64 1024 4 4 1 --cancel-after-ms 1500 2>&1 | tail -2

echo "JNI smoke test passed"
