# Android shell (source, not yet buildable here)

Kotlin/Gradle prototype that drives the same `libmsr_jni.so` the headless Java
CLI uses. **This directory is source-complete but unbuilt**: the environment
used for Phase 4 has no Android SDK/NDK/Gradle, so the debug APK and the UI exit
gate are still open. See `docs/device-compatibility.md` for the remaining steps.

## Files

| File | Role |
| --- | --- |
| `app/src/main/java/dev/msr/engine/MsrEngine.kt` | JNI wrapper (load/generate/cancel/stats). |
| `app/src/main/java/dev/msr/engine/StreamingService.kt` | Single-thread executor + foreground service; owns the engine handle. |
| `app/src/main/java/dev/msr/engine/MainActivity.kt` | SAF model import → app-private copy → load → generate/cancel → stats display. |
| `app/src/main/res/layout/activity_main.xml` | Plain framework Views; no AndroidX dependency. |
| `app/src/main/AndroidManifest.xml` | Main activity, special-use foreground service, notification permission. |
| `app/build.gradle.kts` | AGP + Kotlin, `compileSdk 35`, `minSdk 26`, arm64-v8a only. |

## Build on a host with SDK/NDK

```sh
# 1. build libmsr_jni.so with NDK r27+ (arm64-v8a, 16 KB-page flags)
#    see docs/native-api.md and docs/device-compatibility.md
cp <ndk-build>/libmsr_jni.so app/src/main/jniLibs/arm64-v8a/

# 2. from this directory
./gradlew assembleDebug
# install the APK on the device, then pick a GGUF through the UI
```

The Termux-built `libmsr_jni.so` is **not** usable in an APK: different
sysroot/libc. It exists only to test the JNI contract end-to-end on the device
via `dev.msr.engine.MsrCli`.

## Semantics the UI relies on

- `generate` runs on the service's executor; tokens are posted back to the UI
  thread. The sink can stop generation, and the Cancel button calls
  `engine.cancel()` from the UI thread (atomic flag + llama CPU abort callback).
- Stats are shown after each run: tokens, TTFT, tok/s, weight bytes, streamed
  layers and whole-process `VmHWM`/`VmRSS` from `/proc/self/status`.
- No broad storage permission: SAF import copies into `filesDir/models/`.
