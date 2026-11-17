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

# Static PIC llama + installable C SDK (docs/FRAMEWORK.md)
cmake -S third_party/llama.cpp -B build-static -G Ninja \
    -DBUILD_SHARED_LIBS=OFF -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=OFF -DGGML_OPENMP=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-static --target llama -j"$(nproc 2>/dev/null || echo 4)"

cmake -S sdk -B build-sdk -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PWD/build-sdk/prefix"
cmake --build build-sdk -j"$(nproc 2>/dev/null || echo 4)"
ctest --test-dir build-sdk --output-on-failure
cmake --install build-sdk
