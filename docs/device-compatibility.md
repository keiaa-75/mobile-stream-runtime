# Device compatibility (first pass)

## Reference device (measured)

| Field | Value |
| --- | --- |
| Device | Redmi Turbo 3, `24069RA21C`, codename `peridot` |
| SoC | Snapdragon 8s Gen 3 (`SM8635`), 8 cores, arm64-v8a |
| RAM | 12 GB class; 11.0 GiB visible (`MemTotal` 11,542,640 kB) |
| OS | Android 15 |
| Storage | Emulated FUSE at `/storage/emulated/0`, ~1.4 GiB/s sequential |
| Kernel | 4K page size (assumed from the Termux build; verify on the target) |

All Phase 2/3 numbers in `BENCHMARKS.md` and
`benchmarks/REDMI_TURBO_3_BENCHMARK.md` are from this device.

## ABIs and build targets

- **arm64-v8a is the only supported ABI.** The Gradle skeleton filters to it.
  `armeabi-v7a` cannot host this model comfortably and was never tested.
  x86_64 could run the Termux/JNI CLI path but no measurement exists.
- The JNI library must be built with the **Android NDK** for an APK; the
  Termux-built `libmsr_jni.so` is only for the headless CLI in the same Termux
  process (different libc/sysroot). Copy the NDK build into
  `android/app/src/main/jniLibs/arm64-v8a/`.
- **Android 14+ / 16 KB pages:** devices shipping with 16 KB pages need NDK r27+
  and `-Wl,-z,max-page-size=16384` for every native library. The provider's
  arena aligns to 4096 B and ggml aligns tensors to 64 B; both are compatible
  with 16 KB pages, but the ELF load segments are not until rebuilt with the
  flag above. Untested on a 16 KB-page device.
- `minSdk = 26`, `targetSdk = 35`. Special-use foreground service is required
  from Android 14 onward; `POST_NOTIFICATIONS` is requested at runtime from 33.

## Storage and model management

- The app never uses broad storage permission. It imports through
  `ACTION_OPEN_DOCUMENT` (SAF), copies the file to `filesDir/models/`, and uses
  that private path for inference. `android:allowBackup="false"`.
- Weight files stay out of version control (`.gitignore` excludes `*.gguf`).
- Copying the 2.49 GB GGUF on this FUSE mount is slow; the UI reports copy
  progress state and only enables Load afterwards. A copy is not checksummed
  yet — verifying SHA-256 against `docs/model-inventory/` is a remaining task.
- Cold-cache behaviour cannot be forced from an unprivileged app:
  `posix_fadvise(DONTNEED)` is a no-op on this mount. A/B numbers are warm-cache
  by construction.

## Memory and process limits

- Weights live in native anonymous memory (`MemoryManager` arena), outside the
  Dalvik heap; `largeHeap` does not change this budget.
- `MSR_BUDGET_BYTES` is a **weights-only** cap. KV cache and compute buffers are
  allocated normally by llama.cpp outside the arena (the original all-or-nothing
  `no_alloc` crashes the allocator). Whole-process peak is sampled from
  `VmHWM`/PSS; the app reports it through `stats()`.
- At 2K context and 1 GiB budget the measured whole-process peak on the
  reference device is 1,255.8 MiB with prefetch (vs 2,645.6 MiB stock).
- Older/lower-RAM devices are unmeasured. `token_embd` (525 MiB) must stay
  resident, so budgets under ~600 MiB (one slot) / ~720 MiB (two slots) cannot
  fit this model.

## Thermal and battery

- The reference device reaches ~74 °C under sustained decode and throttles;
  throughput drifts up to ~30 % between runs. Benchmarks use interleaved A/B.
- `stats().thermalC` reads thermal zones when the OS permits; many apps cannot,
  so `-1` means "unavailable", not "cold".
- Battery delta is not in the JNI surface yet; the app must sample Android's
  `BatteryManager` around a run if that telemetry is required.

## What is and is not verified

| Item | Status |
| --- | --- |
| JNI load/generate/cancel/stats on the reference device | verified through the Java CLI |
| Kotlin sources, Gradle project, manifest/layout | written, **not compiled** (no SDK/NDK here) |
| Debug-signed APK, install, UI end-to-end generation | **not done** — Phase 4 exit gate remains open |
| SAF import and app-private copy | source implemented, not exercised |
| 16 KB-page devices, other SoCs, other Android versions | untested |
| NDK build of `msr_core` (OpenSSL dependency in `integrity.cpp`) | unresolved: use BoringSSL or a vendored SHA-256 for the APK build |

## Path to the APK exit gate

1. Install Android SDK platform 35, build-tools, CMake/NDK r27+ on a build host.
2. Resolve the OpenSSL dependency for `native/src/model/integrity.cpp`
   (drop it from the JNI link or replace with a small SHA-256 implementation).
3. Build `libmsr_jni.so` with the NDK (arm64-v8a, 16 KB-page flags), copy to
   `android/app/src/main/jniLibs/arm64-v8a/`.
4. `./gradlew assembleDebug`, install the APK on the reference device.
5. Pick the GGUF, load, generate, cancel mid-run, and confirm the displayed
   stats; compare greedy output against the CLI for the same prompt.
