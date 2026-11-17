# Setup — Fedora (x86_64)

**No emulator is needed.** This is a native C++ framework: CMake compiles it
directly on Fedora. The original measurements came from an arm64 handheld, so
x86_64 is *expected* to work but is **not verified yet** — run the checks below
and report results before treating it as supported.

## 1. Install build tools

```sh
sudo dnf install -y git cmake ninja-build gcc-c++ openssl-devel python3
```

`openssl-devel` is required by the model-integrity module. CMake ≥ 3.16 is
needed; Fedora's default package satisfies this.

## 2. Get the code

```sh
git clone --recursive https://github.com/RavinEduard/Mobility_Engine.git
cd Mobility_Engine
```

If you cloned without `--recursive`, `scripts/setup.sh` will fetch the pinned
llama.cpp submodule for you.

## 3. One-command setup

```sh
scripts/setup.sh
```

This performs, in order:

1. fetches the vendored llama.cpp submodule at the pinned commit;
2. applies `patches/0001-msr-external-buffer-hook.patch` (4 files);
3. builds the reference `llama-completion` binary (`build/`);
4. builds the native core and runs 3 CTest suites (`native/build/`);
5. builds PIC static llama.cpp (`build-static/`);
6. builds the SDK, runs its CTest, and installs to `build-sdk/prefix/`.

If a step fails because a build tree used a different CMake generator, remove
that build directory and rerun the script.

## 4. Provide a model

Model files are **not** in the repository. Obtain the MedGemma Q4_K_M GGUF from
its official distribution and verify its SHA-256 against the value recorded in
`DEVELOPMENT_PHASES.md` / `docs/model-inventory/`. Put it anywhere, e.g.:

```sh
mkdir -p ~/models
# copy or download the GGUF into ~/models/
ls -lh ~/models/medgemma-1.5-4b-it.Q4_K_M.gguf
```

## 5. Run the reference CLI

```sh
M=~/models/medgemma-1.5-4b-it.Q4_K_M.gguf
LD_LIBRARY_PATH=$PWD/build-sdk/lib \
  build-sdk/bin/msr-cli "$M" "The capital of France is" 8 1024 4 4 1
```

Arguments after the prompt: `max_tokens budget_mb threads read_threads prefetch`.
Expected output for the reference prompt:

```
thought
1.  **Identify
```

The statistics line reports peak weight bytes, streamed layers and whole-process
`VmHWM`. **No `LD_PRELOAD` is needed** for this path: the SDK bundles llama.cpp
and the provider into one shared library.

## 6. Use the SDK from your own project

```cmake
find_package(msr REQUIRED CONFIG)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE msr::engine)
```

```sh
cmake -S . -B build -Dmsr_DIR=~/mobile-stream-runtime/build-sdk/prefix/lib/cmake/msr
cmake --build build -j4
LD_LIBRARY_PATH=~/mobile-stream-runtime/build-sdk/prefix/lib ./build/my_app
```

A complete example is in `sdk/examples/`. The public header is
`include/msr/engine/engine.h`; no backend headers are needed.

## 7. Verify

```sh
ctest --test-dir native/build --output-on-failure   # 3 suites
ctest --test-dir build-sdk --output-on-failure      # reference CLI
```

Both must pass. Greedy output should match the stock loader exactly. If a
streamed run produces garbage, check that the provider printed
`msr_external: streamed layer ... ready` lines — no refill means no streaming.

## 8. Fedora notes

- SELinux does not require changes for a locally built binary in your home
  directory; the model file is read with ordinary `open`/`pread`.
- Builds use `CMAKE_POSITION_INDEPENDENT_CODE=ON` for the static llama.cpp so it
  can be linked into `libmsr_engine.so`. Do not disable it.
- `MSR_BUDGET_BYTES` / `LD_PRELOAD` are only for the alternative
  `llama-completion` path; the SDK takes the budget from `msr_engine_config`.
- Filesystem `O_DIRECT` support varies (ext4/xfs generally fine). The reader
  falls back to buffered I/O automatically.

## 9. Status of this platform

| Item | Status |
| --- | --- |
| Native build on Fedora x86_64 | expected, **not yet measured** |
| Runtime output | must match stock before making a support claim |
| Throughput/memory numbers | only available for the arm64 reference device |
| Tuning | start with budget 1 GiB, prefetch 1, 4 readers; re-measure on your CPU/NVMe |

Reference-device numbers for comparison: 1,255.8 MiB peak at a 1 GiB budget,
3.00 tok/s greedy, stock 2,645.6 MiB / 9.91 tok/s. Do not quote these as Fedora
results. Details: `docs/FRAMEWORK.md`, `BENCHMARKS.md`, `LIMITATION.md`.
