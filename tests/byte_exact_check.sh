#!/usr/bin/env bash
# Exit-gate check for Phase 1: the native bounded-buffer read must match an
# independent Python extraction of the exact same tensor byte range.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODEL="${1:?usage: byte_exact_check.sh MODEL TENSOR_NAME}"
TENSOR="${2:?usage: byte_exact_check.sh MODEL TENSOR_NAME}"
CLI="${3:-$ROOT/native/build/model-inspect}"
INSPECT="$ROOT/scripts/gguf_inspect.py"

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

python3 "$INSPECT" "$MODEL" --json "$TMP" >/dev/null
read -r START LEN < <(python3 - "$TMP" "$TENSOR" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
t = next(t for t in d["tensors"] if t["name"] == sys.argv[2])
print(d["data_start"] + t["offset"], t["bytes"])
PY
)

EXPECTED="$(python3 - "$MODEL" "$START" "$LEN" <<'PY'
import hashlib, sys
with open(sys.argv[1], "rb") as f:
    f.seek(int(sys.argv[2]))
    print(hashlib.sha256(f.read(int(sys.argv[3]))).hexdigest())
PY
)"

GOT="$("$CLI" "$MODEL" --tensor "$TENSOR" --read-bytes "$LEN" \
      | awk '/^READ_TENSOR/ {for (i=1;i<=NF;i++) if ($i ~ /^sha256=/) {split($i,a,"="); print a[2]}}')"

if [ "$EXPECTED" != "$GOT" ]; then
    echo "MISMATCH: expected $EXPECTED got $GOT" >&2
    exit 1
fi
echo "byte-exact OK: $TENSOR ($LEN bytes) sha256=$GOT"
