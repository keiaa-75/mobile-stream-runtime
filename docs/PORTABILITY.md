# Portability — native framework

Status of the streaming engine outside the reference device. Only one row below
has actually been run; the rest are code-level assessments with concrete work
needed.

## Status matrix

| Platform | Status | Notes |
| --- | --- | --- |
| Reference device (arm64, Redmi Turbo 3) | **tested** | All numbers in `BENCHMARKS.md` and `benchmarks/REDMI_TURBO_3_BENCHMARK.md` come from here. |
| Linux / arm64 (other boards) | **expected to work, untested** | Same POSIX + `LD_PRELOAD` + ELF weak-symbol model already in use. |
| Linux / x86_64 (desktop, glibc) | **expected to work, untested** | Needs a build and a run; check `O_DIRECT` alignment on the target filesystem. |
| macOS | **plausible, untested** | POSIX file I/O and Mach-O weak symbols exist; use `DYLD_INSERT_LIBRARIES` instead of `LD_PRELOAD`, and `O_DIRECT` is absent (already guarded). |
| Windows (MSVC) | **not supported** | Needs a Win32 file backend, a replacement injection mechanism, and a replacement for GNU weak symbols. |
| Windows (MinGW / clang-cl) | **not supported, closer** | Weak symbols partially supported; file backend and injection still missing. |

## Portable code

These modules are plain C++17 and carry no platform code:

- `native/src/scheduler/async_reader.cpp` — bounded read queue and prefetcher
  (`std::thread`, `std::condition_variable`, reads through `ModelFile`).
- `native/src/memory/memory_manager.cpp` — arena allocator (`posix_memalign`;
  replaceable with `_aligned_malloc` on Windows).
- `native/src/model/gguf.cpp`, `integrity.cpp` — parsing/validation (OpenSSL for
  hashing).
- `native/src/streaming/layer_plan.cpp` — residency planner.
- `native/src/backend/backend_adapter.cpp`.
- The provider's *logic* (`native/tools/msr_external_preload.cpp`): layout
  planning, slot alternation, per-layer scheduling.

## Platform-coupled code

| Piece | Current implementation | Coupling |
| --- | --- | --- |
| `ModelFile` | `open` / `pread` / `fstat` / `posix_fadvise`, optional `O_DIRECT` | POSIX only |
| Provider injection | `LD_PRELOAD=libmsr-external.so` | ELF / Linux (`DYLD_INSERT_LIBRARIES` on macOS) |
| llama.cpp hooks | `__attribute__((weak))` declarations in 4 files | GNU/Clang extension; MSVC lacks it |
| Memory measurement | `scripts/run_measured.py` reads `/proc/<pid>/status` + `smaps_rollup` | Linux `/proc` |
| Cache control | `posix_fadvise(..., POSIX_FADV_DONTNEED)` | POSIX; a no-op on the reference FUSE mount |
| Paths in docs | `/storage/emulated/0/...` | reference-device asset layout |

`ModelFile` is a narrow interface (`open`, `size`, `pread_exact`, `cancel`).
Everything above it — scheduler, planner, arena, provider logic — is independent
of the OS.

## Linux build and run

Step-by-step distro guides: `docs/setup-fedora.md`, `docs/setup-lubuntu.md`.
No emulator is needed; both build natively. x86_64 is expected to work but is
not yet measured.

```sh
cd ~/mobile-stream-runtime
git submodule update --init --recursive
git -C third_party/llama.cpp apply ../../patches/0001-msr-external-buffer-hook.patch
cmake -S third_party/llama.cpp -B build -DLLAMA_CURL=OFF
cmake --build build --target llama-completion -j"$(nproc)"
cmake -S native -B native/build && cmake --build native/build -j"$(nproc)"
ctest --test-dir native/build --output-on-failure

M=/path/to/model.gguf
MSR_NO_ALLOC=1 MSR_BUDGET_BYTES=$((1<<30)) MSR_PREFETCH=1 MSR_READ_THREADS=4 \
MSR_MODEL_PATH=$M \
LD_PRELOAD=$PWD/native/build/libmsr-external.so \
build/bin/llama-completion -m $M -p "The capital of France is" -n 8 \
  --seed 42 --temp 0 -t 4 -lm none --no-repack -c 2048
```

Expected differences from the reference device:

- **Storage is much faster** (NVMe 3–7 GB/s vs ~1.4 GB/s), so per-token re-reads
  cost a fraction of the time. Measure before claiming a number.
- **`O_DIRECT` support varies**; `ModelFile::open` already falls back to
  buffered I/O.
- **Keep `--no-repack`**: repacking would allocate a second copy and defeat the
  arena binding.
- **Retune `MSR_READ_THREADS`** (start at 4–8; page-cache reads are
  memory-bandwidth-bound).
- `scripts/run_measured.py` works unchanged on Linux (`/proc`).

## macOS

Same build with `DYLD_INSERT_LIBRARIES` instead of `LD_PRELOAD`; `O_DIRECT` is
guarded. Untested.

## Windows — what a port requires

1. **Win32 file backend** behind `ModelFile`: `CreateFileW`,
   positional `ReadFile` with `OVERLAPPED`, `GetFileSizeEx`,
   `FILE_FLAG_NO_BUFFERING` with queried sector alignment.
2. **Replace `LD_PRELOAD`**: link the provider statically into the consuming
   binary and register hooks through a small function table.
3. **Replace GNU weak symbols** in the vendored patch: an MSVC branch calling
   through that hook table. The engine code itself does not change.
4. **Measurement tooling**: replace `/proc` sampling with native APIs; skip
   cache dropping or document it as unsupported.
5. **Build system**: CMake presets for the toolchain; resolve the OpenSSL hash
   dependency for the integrity module.

Estimate: **~2–4 days** for a tested native Windows port. Scheduler, planner,
arena and provider logic should port unchanged.

## Why portability matters for the trade-off

On the reference device the engine trades ~3× generation speed for ~2× smaller
footprint. On a machine with 16–64 GB of RAM, a 2.5 GB model fits in memory, so
the stock loader is the right choice — the engine's value appears only when a
model is **larger than RAM**, where faster storage makes streaming far more
competitive. That is the strongest reason to finish the desktop port.

## Ground rules

- No "works on X" claim without an actual inference run and byte-identical
  output versus the stock loader for that platform.
- Report peak memory from whole-process measurement, not from the arena.
- Keep platform-specific code inside `ModelFile` and hook registration; do not
  fork the scheduler or the provider logic per OS.
