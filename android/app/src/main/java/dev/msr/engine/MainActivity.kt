package dev.msr.engine

import android.Manifest
import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.provider.OpenableColumns
import android.widget.Button
import android.widget.EditText
import android.widget.ScrollView
import android.widget.TextView
import java.io.File
import java.io.FileOutputStream

class MainActivity : Activity() {
    private lateinit var modelLabel: TextView
    private lateinit var promptEdit: EditText
    private lateinit var status: TextView
    private lateinit var statsView: TextView
    private var modelFile: File? = null
    private var service: StreamingService? = null
    private var bound = false

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder) {
            service = (binder as StreamingService.LocalBinder).service()
            bound = true
            status.text = "service ready"
        }
        override fun onServiceDisconnected(name: ComponentName?) {
            service = null
            bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        modelLabel = findViewById(R.id.modelLabel)
        promptEdit = findViewById(R.id.prompt)
        status = findViewById(R.id.status)
        statsView = findViewById(R.id.stats)

        findViewById<Button>(R.id.pickModel).setOnClickListener { pickModel() }
        findViewById<Button>(R.id.loadModel).setOnClickListener { loadModel() }
        findViewById<Button>(R.id.generate).setOnClickListener { runGeneration() }
        findViewById<Button>(R.id.cancel).setOnClickListener { service?.cancel() }

        if (Build.VERSION.SDK_INT >= 33 &&
            checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(arrayOf(Manifest.permission.POST_NOTIFICATIONS), 1)
        }
    }

    private fun pickModel() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        @Suppress("DEPRECATION")
        startActivityForResult(intent, REQUEST_PICK)
    }

    @Deprecated("framework-only SAF result for the prototype")
    override fun onActivityResult(request: Int, result: Int, data: Intent?) {
        super.onActivityResult(request, result, data)
        if (request != REQUEST_PICK || result != RESULT_OK) return
        val uri = data?.data ?: return
        val name = contentResolver.query(uri, null, null, null, null)?.use { c ->
            val i = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (i >= 0 && c.moveToFirst()) c.getString(i) else "model.gguf"
        } ?: "model.gguf"
        val dest = File(filesDir, "models/$name")
        dest.parentFile?.mkdirs()
        status.text = "copying $name into app-private storage..."
        Thread {
            try {
                contentResolver.openInputStream(uri).use { input ->
                    FileOutputStream(dest).use { out -> input!!.copyTo(out) }
                }
                runOnUiThread {
                    modelFile = dest
                    modelLabel.text = "$name (${dest.length()} bytes)"
                    status.text = "copied; press Load model"
                }
            } catch (t: Throwable) {
                runOnUiThread { status.text = "copy failed: ${t.message}" }
            }
        }.start()
    }

    private fun ensureService() {
        if (bound) return
        val intent = Intent(this, StreamingService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) startForegroundService(intent)
        else startService(intent)
        bindService(intent, connection, Context.BIND_AUTO_CREATE)
    }

    private fun loadModel() {
        val file = modelFile ?: run { status.text = "pick a model first"; return }
        ensureService()
        status.text = "loading weights into arena..."
        service?.load(file.path, 1L shl 30, true, 4,
            onDone = { s -> runOnUiThread { status.text = "loaded"; statsView.text = s } },
            onError = { e -> runOnUiThread { status.text = "load failed: $e" } })
            ?: run { status.text = "service not connected yet; retry" }
    }

    private fun runGeneration() {
        ensureService()
        val text = promptEdit.text.toString()
        if (text.isBlank()) { status.text = "enter a prompt"; return }
        status.text = "generating..."
        val output = StringBuilder()
        service?.generate(text, 32, object : MsrEngine.TokenSink {
            override fun onToken(piece: String): Boolean {
                runOnUiThread {
                    output.append(piece)
                    status.text = "generating... (${output.length} chars)"
                    statsView.text = output.toString()
                }
                return true
            }
        }, onDone = { s ->
            runOnUiThread {
                status.text = "done — %.2f tok/s".format(s.tokensPerSecond)
                statsView.text = s.toString() + "\n\n" + output
            }
        }, onError = { e -> runOnUiThread { status.text = "generation failed: $e" } })
            ?: run { status.text = "service not connected yet; retry" }
    }

    override fun onDestroy() {
        if (bound) unbindService(connection)
        super.onDestroy()
    }

    companion object { private const val REQUEST_PICK = 42 }
}
