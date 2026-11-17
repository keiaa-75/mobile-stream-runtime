package dev.msr.engine;

/** Java mirror of the Kotlin-facing JNI surface. Used by the headless smoke CLI. */
public final class MsrEngine implements AutoCloseable {
    static {
        System.loadLibrary("msr_jni");
    }

    private long handle;

    public MsrEngine() {
        handle = nativeCreate();
    }

    public interface TokenSink {
        /** @return false (or throwing) to stop generation. */
        boolean onToken(String piece);
    }

    public static final class Stats {
        public final long promptTokens, genTokens;
        public final long ttftMs, promptMs, genMs;
        public final long budgetBytes, residentWeightBytes, peakWeightBytes, readBytes;
        public final int streamedLayers, streamSlots, prefetch, readThreads;
        public final long peakVmHwmKb, peakVmRssKb;
        public final int thermalC;

        Stats(long[] v) {
            promptTokens = v[0]; genTokens = v[1];
            ttftMs = v[2]; promptMs = v[3]; genMs = v[4];
            budgetBytes = v[5]; residentWeightBytes = v[6]; peakWeightBytes = v[7];
            readBytes = v[8];
            streamedLayers = (int) v[9]; streamSlots = (int) v[10];
            prefetch = (int) v[11]; readThreads = (int) v[12];
            peakVmHwmKb = v[13]; peakVmRssKb = v[14];
            thermalC = (int) v[15];
        }

        @Override public String toString() {
            return String.format(
                "prompt=%d gen=%d ttft=%dms prompt_ms=%d gen_ms=%d%n" +
                "weight_budget=%d resident=%d peak_weight=%d streamed=%d slots=%d%n" +
                "prefetch=%d readers=%d read_bytes=%d%n" +
                "VmHWM=%d KiB VmRSS=%d KiB thermal=%d C",
                promptTokens, genTokens, ttftMs, promptMs, genMs,
                budgetBytes, residentWeightBytes, peakWeightBytes, streamedLayers, streamSlots,
                prefetch, readThreads, readBytes, peakVmHwmKb, peakVmRssKb, thermalC);
        }
    }

    public void load(String modelPath, long budgetBytes, int ctxTokens, int threads,
                     boolean prefetch, int readThreads) {
        nativeLoad(handle, modelPath, budgetBytes, ctxTokens, threads, prefetch, readThreads);
    }

    public void generate(String prompt, int maxTokens, float temperature, int seed,
                         TokenSink sink) {
        nativeGenerate(handle, prompt, maxTokens, temperature, seed, sink);
    }

    public void cancel() {
        nativeCancel(handle);
    }

    public Stats stats() {
        return new Stats(nativeStats(handle));
    }

    public String lastError() {
        return nativeLastError(handle);
    }

    @Override public void close() {
        if (handle != 0) {
            nativeDestroy(handle);
            handle = 0;
        }
    }

    private native long nativeCreate();
    private native void nativeDestroy(long h);
    private native void nativeLoad(long h, String path, long budgetBytes, int ctxTokens,
                                   int threads, boolean prefetch, int readThreads);
    private native void nativeGenerate(long h, String prompt, int maxTokens, float temperature,
                                       int seed, TokenSink sink);
    private native void nativeCancel(long h);
    private native long[] nativeStats(long h);
    private native String nativeLastError(long h);
}
