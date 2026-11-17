package dev.msr.engine

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.Build
import android.os.IBinder
import java.util.concurrent.Executors

/** Keeps inference alive while the app is backgrounded; owns the JNI handle. */
class StreamingService : Service() {
    inner class LocalBinder : Binder() { fun service(): StreamingService = this@StreamingService }
    private val binder = LocalBinder()
    private val executor = Executors.newSingleThreadExecutor()
    private var engine: MsrEngine? = null

    override fun onBind(intent: Intent): IBinder = binder

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIFICATION_ID, notification())
        return START_STICKY
    }

    fun load(path: String, budgetBytes: Long, prefetch: Boolean, readThreads: Int,
             onDone: (String) -> Unit, onError: (String) -> Unit) {
        executor.execute {
            try {
                engine?.close()
                val e = MsrEngine()
                e.load(path, budgetBytes, 2048, 4, prefetch, readThreads)
                engine = e
                onDone(e.stats().toString())
            } catch (t: Throwable) {
                onError(t.message ?: "load failed")
            }
        }
    }

    fun generate(prompt: String, maxTokens: Int, sink: MsrEngine.TokenSink,
                 onDone: (MsrEngine.Stats) -> Unit, onError: (String) -> Unit) {
        executor.execute {
            try {
                val e = engine ?: throw IllegalStateException("load a model first")
                e.generate(prompt, maxTokens, 0.0f, 42, sink)
                onDone(e.stats())
            } catch (t: Throwable) {
                onError(t.message ?: "generation failed")
            }
        }
    }

    fun cancel() { engine?.cancel() }

    override fun onDestroy() {
        engine?.close()
        engine = null
        executor.shutdownNow()
        super.onDestroy()
    }

    private fun notification(): android.app.Notification {
        val manager = getSystemService(NotificationManager::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            manager.createNotificationChannel(
                NotificationChannel(CHANNEL_ID, "Inference", NotificationManager.IMPORTANCE_LOW))
        }
        return Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("Mobility Engine")
            .setContentText("Local model inference")
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setOngoing(true)
            .build()
    }

    companion object {
        private const val CHANNEL_ID = "msr-inference"
        private const val NOTIFICATION_ID = 1
    }
}
