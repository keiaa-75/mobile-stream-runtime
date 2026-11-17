# Android platform constraints (measured, not assumed)

Reference device: `24069RA21C` / `peridot`, Android 15 (SDK 35),
kernel `6.1.118-android14-11`, 11.5 GB RAM + 8.4 GB swap, 8 cores.

Anything below marked **measured** was verified on this device. Anything
marked **to verify** is still an open question and must not be designed
against.

## 1. Storage

| Topic | Finding |
| --- | --- |
| Shared storage path | `/storage/emulated/0` is a FUSE mount, not a block device. `statfs` reports 227 GB, 23 GB free. |
| App-private storage | `/data/data/<pkg>/files` — no FUSE layer, no permission prompt. **Preferred location for weights** unless the model must be user-visible. |
| Sequential read | ~1.42 GiB/s on the FUSE mount (measured, 8 MiB chunks). |
| Random 16 MiB read | ~1.34 GiB/s median, cold-ish (measured). |
| 4 KiB read latency | sub-millisecond only when cached; cold latency **not established** (no root to drop caches). |
| `O_DIRECT` | **Supported** on the FUSE mount (measured: open + aligned 4 KiB read). |
| `mmap` | Works; pages become resident on touch; no automatic eviction. |
| `posix_fadvise` / `MADV_DONTNEED` | `MADV_DONTNEED` on a mapped window reclaims pages (measured: RSS returned to 16 MiB). |
| Scoped storage | App-private dir needs no runtime permission. Shared storage needs `MANAGE_EXTERNAL_STORAGE` or SAF; avoid for the model path. |
| File sizes > 4 GiB | Not exercised. GGUF is split, and our model is 2.4 GB, so this is fine for now. |

**Decision:** default model location is app-private storage; allow a
user-imported copy from shared storage. Do not build the runtime around
assumptions about UFS generation — measure per device.

## 2. Memory

| Topic | Finding |
| --- | --- |
| `MemTotal` | 11,542,640 kB |
| `MemAvailable` (idle, after our probes) | 4,572,176 kB |
| Swap | 8,388,604 kB available to the process |
| Android LMK | Kills by oom_score_adj; a foreground service lowers the risk but does not guarantee survival. |
| Per-process limit | No hard `RLIMIT_AS` issue observed; the binding constraint is system-wide pressure. |
| RSS accounting | `/proc/self/statm` and `VmHWM` both usable from app code (verified). |
| Page cache | Counted globally, not against the app, but it competes for the same physical RAM. `O_DIRECT` avoids inflating it. |

**Budgeting rule:** never plan against `MemTotal`. Plan against
`MemAvailable` measured immediately before load, minus a 512 MiB safety
margin, minus the KV cache, minus activations.

## 3. CPU

| Topic | Finding |
| --- | --- |
| Cores | 8, heterogeneous (big/little). |
| Thread affinity | Not yet tested; Android may migrate threads and change effective throughput mid-generation. |
| NEON / dotprod / i8mm | Present on this SoC class; confirm at runtime with `cpuinfo`/`getauxval`. Quantized matmul speed depends on it. |
| Thermal throttling | Not yet measured. Expect sustained-load decay; must be characterised in Phase 5/6. |

## 4. Process lifecycle

- Long inference must run in a **foreground service** with a persistent
  notification, or it will be killed when backgrounded.
- Process death is normal on Android. Runtime state must be reconstructible:
  reload model, re-create context, never assume the previous process survived.
- Cancellation must be cooperative and must not corrupt the model file or
  leave pinned buffers behind.
- `onTrimMemory` / `onLowMemory` are advisory. Treat them as a signal to drop
  caches, not as a guarantee of time.

## 5. Native build

| Topic | Finding |
| --- | --- |
| Toolchain present | `clang` 21.1.8, `make` 4.4.1, `git`, `python3` (Termux). `cmake` and `ninja` are **not installed**. |
| llama.cpp build system | CMake only (Makefile builds removed upstream). Phase 0's build baseline is blocked until `cmake` (+`ninja`) is installed. |
| Android app build | `aapt` and `zipalign` present; Android SDK/Gradle not verified. |
| ABI | `arm64-v8a` only for the reference device. |

## 6. Power and thermals

- Continuous storage reads plus CPU inference will heat the device. Thermal
  status is readable via `PowerManager.getCurrentThermalStatus()` from Kotlin.
- Battery delta per 1K tokens is a required benchmark metric, not an
  afterthought.
- Charging while benchmarking changes thermal behaviour; record it.

## 7. Things we must not assume

1. That desktop Linux I/O semantics apply. They do not: FUSE, LMK, and
   scoped storage all differ.
2. That `mmap` is cheap. Measured: 97.7% of the file becomes resident.
3. That `O_DIRECT` is available everywhere. It works here; it must degrade to
   buffered reads elsewhere.
4. That thread count == core count is optimal. Measure.
5. That a foreground service prevents LMK. It only lowers the probability.
