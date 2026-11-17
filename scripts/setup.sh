#!/usr/bin/env bash
# Fresh-clone setup: vendored llama.cpp submodule, MSR hook patch, builds, tests.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

if [ ! -f third_party/llama.cpp/ggml/include/ggml.h ]; then
    git submodule update --init --recursive third_party/llama.cpp
fi

patch="patches/0001-msr-external-buffer-hook.patch"
if git -C third_party/llama.cpp apply --check "../../$patch" 2>/dev/null; then
    git -C third_party/llama.cpp apply "../../$patch"
    echo "applied $patch"
else
    echo "$patch is already applied (or conflicts with this checkout)"
fi

cmake -S third_party/llama.cpp -B build -DLLAMA_CURL=OFF
cmake --build build --target llama-completion -j"$(nproc 2>/dev/null || echo 4)"

cmake -S native -B native/build
cmake --build native/build -j"$(nproc 2>/dev/null || echo 4)"
ctest --test-dir native/build --output-on-failure
