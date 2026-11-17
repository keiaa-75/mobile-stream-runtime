# Portability — Android, Linux, macOS, Windows

Status of the streaming engine outside the reference device. Only one column
below has actually been run; everything else is an assessment of the code as it
stands, with the concrete work each platform needs.

## Status matrix

| Platform | Status | Notes |
| --- | --- | --- |
| Android / arm64 (reference device) | **tested** | All numbers in `BENCHMARKS.md` come from here. |
| Linux / x86_64 (desktop, glibc) | **expected to work, untested** | Same POSIX + `LD_PRELOAD` + ELF weak-symbol model already in use. Needs a build and a run. |
| Linux / arm64 (non-Android, e.g. SBC) | **expected to work, untested** | Same as above; Termux on Android is already this shape. |
| macOS | **plausible, untested** | POSIX file I/O and Mach-O weak symbols exist; build must use `DYLD_INSERT_LIBRARIES` instead of `LD_PRELOAD`, and `O_DIRECT` is absent (the code already guards it). |
| Windows (MSVC) | **not supported** | Needs a Win32 file backend, a replacement for `LD_PRELOAD`, and a replacement for GNU weak symbols. |
| Windows (MinGW / clang-cl) | **not supported, closer** | Weak symbols are partially supported; the file backend and injection story are still missing. |

## What is already portable

These modules are plain C++17 and carry no platform code:

- `native/src/scheduler/async_reader.cpp` — bounded read queue and prefetcher
  (`std::thread`, `std::condition_variable`, reads through `ModelFile`).
- `native/src/memory/memory_manager.cpp` — arena allocator (`posix_memalign`;
  trivially replaceable with `_aligned_malloc` on Windows).
- `native/src/model/gguf.cpp`, `integrity.cpp` — GGUF parsing/validation
  (OpenSSL for hashing).
- `native/src/streaming/layer_plan.cpp` — residency planner.
- `native/src/backend/backend_adapter.cpp`.
- The provider's *logic* (`native/tools/msr_external_preload.cpp`): layout
  planning, slot alternation, per-layer scheduling. Only its injection and the
  file backend are platform-specific.

## What is platform-coupled

| Piece | Current implementation | Coupling |
| --- | --- | --- |
| `ModelFile` | `open` / `pread` / `fstat` / `posix_fadvise`, optional `O_DIRECT` | POSIX only |
| Provider injection | `LD_PRELOAD=libmsr-external.so` | ELF / Linux (`DYLD_INSERT_LIBRARIES` on macOS) |
| llama.cpp hooks | `__attribute__((weak))` declarations in 4 files | GNU/Clang extension; MSVC lacks it |
| Memory measurement | `scripts/run_measured.py` reads `/proc/<pid>/status` + `smaps_rollup` | Linux `/proc` |
| Cache control | `posix_fadvise(..., POSIX_FADV_DONTNEED)` | POSIX; a no-op on the reference FUSE mount |
| Paths in docs | `/storage/emulated/0/...` | Android asset layout |

The important part for a port is that `ModelFile` is a narrow interface
(`open`, `size`, `pread_exact`, `cancel`). Everything above it, including the
whole scheduler and the provider logic, is independent of the OS.

## Linux desktop

Build steps are the same as on the reference device:

```sh
git clone --recursive <this repo> && cd Mobility_Engine
# vendored llama.cpp is pinned to ef6bc6f; the 4-file hook patch is separate
git -C third_party/llama.cpp apply ../../patches/0001-msr-external-buffer-hook.patch
cmake -S third_party/llama.cpp -B build -DLLAMA_CURL=OFF
cmake --build build --target llama-completion -j"$(nproc)"
cmake -S native -B native/build && cmake --build native/build -j"$(nproc)"
ctest --test-dir native/build --output-on-failure
```

Run:

```sh
M=/path/to/model.gguf
MSR_NO_ALLOC=1 MSR_BUDGET_BYTES=$((1<<30)) MSR_PREFETCH=1 MSR_READ_THREADS=4 \
MSR_MODEL_PATH=$M \
LD_PRELOAD=$PWD/native/build/libmsr-external.so \
build/bin/llama-completion -m $M -p "The capital of France is" -n 8 \
  --seed 42 --temp 0 -t 4 -lm none --no-repack -c 2048
```

Expected differences from the phone:

- **Storage is much faster.** NVMe delivers 3–7 GB/s instead of ~1.4 GB/s, so
  the same per-token re-read costs a fraction of the time. On a desktop the
  streaming path should land far closer to stock throughput; measure before
  claiming a number.
- **`O_DIRECT` support varies.** ext4/xfs generally support it; some
  filesystems and LUKS setups do not. `ModelFile::open` already falls back to
  buffered I/O.
- **`--no-repack` matters.** Keep it: repacking would allocate a second,
  repacked copy of the weights and defeat the arena binding.
- **`MSR_READ_THREADS`** should be re-tuned (start at 4–8; page-cache reads are
  memory-bandwidth-bound, so more workers stop helping at some point).
- **`scripts/run_measured.py` works unchanged** on Linux (`/proc`).

Known risk to check first: `O_DIRECT` alignment/offset rules on the target
filesystem, and whether the CPU backend needs a different
`GGML_CPU_*` build flag set than the Android build.

## macOS

The same build should work with two substitutions:

- replace `LD_PRELOAD` with `DYLD_INSERT_LIBRARIES` (and expect SIP restrictions
  for system binaries — use a locally built `llama-completion`, which is not
  restricted);
- `O_DIRECT` does not exist; `#ifdef O_DIRECT` already compiles the buffered
  path.

`posix_fadvise` and Mach-O weak symbols exist, so no code change is expected.
Untested.

## Windows — what a port requires

1. **Win32 file backend behind the existing interface.**
   `CreateFileW` (with `FILE_FLAG_OVERLAPPED`), positional `ReadFile` using
   `OVERLAPPED.Offset/OffsetHigh`, `GetFileSizeEx`, and
   `FILE_FLAG_NO_BUFFERING` + sector-aligned buffers for the direct variant
   (alignment must be queried per volume; 4096 is usually safe). This is one
   self-contained file; `ModelFile` callers do not change.
2. **Replace `LD_PRELOAD`.** Recommended: build the provider as a **static
   library linked into the `llama-completion` executable**, and register the
   hooks through a small function table initialised at startup. Alternatives:
   a DLL plus an explicit `msr_install()` call from the executable, or MSVC
   `/alternatename` trampolines. Static linking is the least fragile.
3. **Replace GNU weak symbols in the vendored patch.** MSVC has no
   `__attribute__((weak))`. The 4-file patch would gain a `_WIN32` branch that
   calls through the hook table instead of testing a weak symbol. The engine
   code itself does not change.
4. **Measurement tooling.** `run_measured.py` needs a Windows path using
   `GetProcessMemoryInfo` (peak working set) or ETW; `posix_fadvise` cache
   dropping has no direct equivalent (there is `SetSystemFileCacheSize`, but it
   is a machine-wide privileged operation — better to skip it and document
   warm/cold conditions explicitly).
5. **Build system.** CMake works, but OpenSSL, the CPU backend flags, and the
   runtime DLL layout need a Windows preset. MSVC and clang-cl should both be
   viable after (1)–(3); MinGW is closer today because it supports weak
   symbols, but still needs (1).

Estimate: **~2–4 days** for a tested MSVC port (file backend, hook table,
build/packaging, measurement, one correctness run against stock). The scheduler,
planner, arena, and provider logic should port without changes.

## Why portability matters for the trade-off

On a phone the engine trades ~3× generation speed for a ~2× smaller footprint.
On a PC with 16–64 GB of RAM, a 2.5 GB model fits in RAM, so the stock loader is
the right choice there — the engine's value appears only when the model is
**larger than RAM** (e.g. a 70B Q4 at ~40 GB on a 32 GB machine, or a 30B on a
16 GB laptop). That is exactly the same mechanism, with a larger budget and a
faster NVMe backend, and it is the strongest reason to finish the desktop port.

## Ground rules for those columns

- No "works on X" claim without an actual inference run and byte-identical
  output versus the stock loader for that platform.
- Report peak memory from whole-process measurement, not from the arena.
- Keep the platform-specific surface inside `ModelFile` and the hook
  registration; do not fork the scheduler or the provider logic per OS.
