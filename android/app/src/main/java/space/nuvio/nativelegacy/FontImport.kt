package space.nuvio.nativelegacy

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import java.io.File

/** A document URI is used only while copying. Native code validates and owns the font. */
class FontImport(private val activity: Activity) {
    private var pending = false
    private var closed = false
    private var event: String? = null
    @Volatile var escolhendoArquivo = false
        private set

    @Synchronized fun abrir(): Boolean {
        if (closed || pending || activity.isFinishing || activity.isDestroyed) return false
        pending = true
        activity.runOnUiThread {
            synchronized(this) {
                if (closed || !pending) return@runOnUiThread
                try {
                    escolhendoArquivo = true
                    @Suppress("DEPRECATION")
                    activity.startActivityForResult(documento(), PEDIDO)
                } catch (_: Exception) {
                    escolhendoArquivo = false
                    event = "F"
                }
            }
        }
        return true
    }

    @Synchronized fun salvar(out: Bundle) {
        if (pending && escolhendoArquivo) out.putBoolean("fontImport", true)
    }
    @Synchronized fun restaurar(state: Bundle?) {
        if (state?.getBoolean("fontImport", false) == true) {
            // Native selection/profile ownership is not restored by a Bundle.
            // Cancel the old request instead of selecting for a new native session.
            pending = true
            event = "C"
        }
    }
    @Synchronized fun proximo(): String? {
        val result = event ?: return null
        event = null
        pending = false
        return result
    }
    @Synchronized fun fechar() {
        closed = true
        escolhendoArquivo = false
        event?.takeIf { it.startsWith("R:") }?.let {
            File(pasta(), it.substring(2)).delete()
        }
        event = null
        pending = false
    }
    private fun pasta() = File(activity.filesDir, "dados/fontes-importadas")

    @Synchronized fun resultado(code: Int, result: Int, data: Intent?): Boolean {
        if (code != PEDIDO) return false
        if (closed || !pending || !escolhendoArquivo) return true
        escolhendoArquivo = false
        val uri = data?.data
        if (result != Activity.RESULT_OK || uri == null) { event = "C"; return true }
        Thread({
            var staged: File? = null
            val answer = try {
                val dir = pasta()
                if (!dir.isDirectory && !dir.mkdirs()) throw IllegalStateException()
                val random = java.security.SecureRandom()
                for (attempt in 0 until 8) {
                    val candidate = File(dir, "entrada-" + java.lang.Long.toHexString(random.nextLong()) + ".tmp")
                    if (candidate.createNewFile()) { staged = candidate; break }
                }
                if (staged == null) throw IllegalStateException()
                activity.contentResolver.openInputStream(uri)?.use { input ->
                    staged!!.outputStream().use { output -> copiar(input, output) }
                } ?: throw IllegalStateException()
                "R:${staged!!.name}"
            } catch (_: Exception) {
                staged?.delete()
                "F"
            }
            synchronized(this) {
                if (closed) staged?.delete() else event = answer
            }
        }, "nuvio-import-font").start()
        return true
    }

    companion object {
        const val PEDIDO = 49118
        const val MAX_BYTES = 32 * 1024 * 1024
        fun documento() = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            // Some TV document providers report fonts as application/octet-stream.
            // The native SFNT/FreeType validation, not the MIME label, accepts a font.
            type = "*/*"
        }
        fun copiar(input: java.io.InputStream, output: java.io.OutputStream) {
            val buffer = ByteArray(16 * 1024)
            var total = 0
            while (true) {
                val n = input.read(buffer)
                if (n < 0) break
                if (n == 0) {
                    val one = input.read()
                    if (one < 0) break
                    if (total == MAX_BYTES) throw java.io.IOException("font too large")
                    output.write(one); total++
                    continue
                }
                if (n > MAX_BYTES - total) throw java.io.IOException("font too large")
                output.write(buffer, 0, n)
                total += n
            }
            if (total < 12) throw java.io.IOException("empty font")
        }
    }
}
