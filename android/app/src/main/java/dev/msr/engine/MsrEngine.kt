package dev.msr.engine

/** Kotlin surface implemented by native/jni/msr_jni.cpp (same JNI names as the Java CLI mirror). */
class MsrEngine : AutoCloseable {
    private var handle: Long = nativeCreate()

    interface TokenSink { fun onToken(piece: String): Boolean }

    data class Stats(
        val promptTokens: Long, val genTokens: Long,
        val ttftMs: Long, val promptMs: Long, val genMs: Long,
        val budgetBytes: Long, val residentWeightBytes: Long, val peakWeightBytes: Long,
        val readBytes: Long,
        val streamedLayers: Int, val streamSlots: Int, val prefetch: Int, val readThreads: Int,
        val peakVmHwmKb: Long, val peakVmRssKb: Long, val thermalC: Int,
    ) {
        val tokensPerSecond: Double
            get() = if (genMs > 0) genTokens * 1000.0 / genMs else 0.0
    }

    fun load(path: String, budgetBytes: Long, ctxTokens: Int, threads: Int,
             prefetch: Boolean, readThreads: Int) {
        nativeLoad(handle, path, budgetBytes, ctxTokens, threads, prefetch, readThreads)
    }

    fun generate(prompt: String, maxTokens: Int, temperature: Float = 0.0f,
                 seed: Int = 42, sink: TokenSink) {
        nativeGenerate(handle, prompt, maxTokens, temperature, seed, sink)
    }

    fun cancel() = nativeCancel(handle)

    fun stats(): Stats {
        val v = nativeStats(handle)
        return Stats(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8],
            v[9].toInt(), v[10].toInt(), v[11].toInt(), v[12].toInt(),
            v[13], v[14], v[15].toInt())
    }

    fun lastError(): String? = nativeLastError(handle)

    override fun close() {
        if (handle != 0L) {
            nativeDestroy(handle)
            handle = 0L
        }
    }

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(h: Long)
    private external fun nativeLoad(h: Long, path: String, budgetBytes: Long, ctxTokens: Int,
                                    threads: Int, prefetch: Boolean, readThreads: Int)
    private external fun nativeGenerate(h: Long, prompt: String, maxTokens: Int,
                                        temperature: Float, seed: Int, sink: TokenSink)
    private external fun nativeCancel(h: Long)
    private external fun nativeStats(h: Long): LongArray
    private external fun nativeLastError(h: Long): String?

    companion object {
        init { System.loadLibrary("msr_jni") }
    }
}
