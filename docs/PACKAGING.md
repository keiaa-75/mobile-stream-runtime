# Ready-to-run Linux packages

Two native x86_64 tarballs are produced automatically by GitHub Actions — one
built on Ubuntu (runs on Lubuntu with the matching glibc), one built inside a
Fedora container. **No emulator and no build tools are needed to test them.**

## Download

On a tag such as `v0.1.0`, the workflow `.github/workflows/linux-packages.yml`
attaches these assets to the GitHub release:

| Asset | Built on | Use on |
| --- | --- | --- |
| `msr-ubuntu-x86_64-v0.1.0.tar.gz` | Ubuntu 24.04 | Lubuntu 24.04 / compatible glibc |
| `msr-fedora-x86_64-v0.1.0.tar.gz` | Fedora 40 container | Fedora 40+ (forward-compatible glibc) |

Each has a `.sha256` sidecar. Verify before running:

```sh
sha256sum -c msr-ubuntu-x86_64-v0.1.0.tar.gz.sha256
```

For manual builds without a release, run the workflow with **Run workflow** in
the Actions tab and download the `linux-packages` artifact.

## Contents

```
bin/msr-cli                 reference CLI
lib/libmsr_engine.so*        engine + llama.cpp + provider, bundled
include/msr/engine/engine.h  public C API
lib/cmake/msr/               find_package(msr) / msr::engine
examples/                    minimal host program
docs/                        API guide, setup guides, limitations
install.sh                   optional system installer
README.txt                   quick start
```

Model weights are **not** included (2.49 GB, separately licensed).

## Run

```sh
tar -xzf msr-ubuntu-x86_64-v0.1.0.tar.gz
cd msr-ubuntu-x86_64-v0.1.0
M=/path/to/medgemma-1.5-4b-it.Q4_K_M.gguf
LD_LIBRARY_PATH=./lib ./bin/msr-cli "$M" "The capital of France is" 8 1024 4 4 1
```

Expected greedy output: `thought\n1.  **Identify`. Stats print to stderr.

Optional system install:
```sh
sudo ./install.sh /usr/local
msr-cli "$M" "The capital of France is" 8 1024 4 4 1
```

The budget is the 5th argument after the prompt (`1024` = 1 GiB; use `1536`
for 1.5 GiB). Other args: `max_tokens threads read_threads prefetch`.

## Use from your own project

```cmake
find_package(msr REQUIRED CONFIG)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE msr::engine)
```

Point CMake at the extracted package if not installed system-wide:
```sh
cmake -S . -B build -Dmsr_DIR=/path/to/extracted/lib/cmake/msr
```

## Status / caveats

- The packages are **expected to work**, not yet verified on x86_64 hardware —
  no support claim until someone runs the CLI and compares with stock output.
- Glibc forward compatibility is not backward compatibility: a Fedora 40 build
  will not run on older Fedora releases. Build on the oldest target you need.
- The Ubuntu package targets Lubuntu/Ubuntu 24.04. For older Lubuntu, rebuild
  locally with `scripts/setup.sh` (see `docs/setup-lubuntu.md`).
- All throughput/memory numbers in `BENCHMARKS.md` come from the arm64 reference
  device, not from these packages.
