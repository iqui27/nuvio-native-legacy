package space.nuvio.nativelegacy

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.widget.Toast
import java.io.File
import java.io.RandomAccessFile
import java.net.URI
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Copia local do mesmo arquivo que o painel nativo le. Nao coleta nem envia logs. */
class RegistroDownload(private val activity: Activity) {
    private data class Pedido(val arquivo: File, val sucesso: String, val falha: String)
    private var pedido: Pedido? = null
    private var fechado = false
    var escolhendoArquivo = false
        private set

    fun salvar(estado: Bundle) {
        val atual = pedido ?: return
        if (!escolhendoArquivo) return
        estado.putBundle("registroDownload", Bundle().apply {
            putString("arquivo", atual.arquivo.path)
            putString("sucesso", atual.sucesso)
            putString("falha", atual.falha)
        })
    }

    fun restaurar(estado: Bundle?) {
        val salvo = estado?.getBundle("registroDownload") ?: return
        val arquivo = salvo.getString("arquivo") ?: return
        val sucesso = salvo.getString("sucesso") ?: return
        val falha = salvo.getString("falha") ?: return
        pedido = Pedido(File(arquivo), sucesso, falha)
        escolhendoArquivo = true
    }

    fun fechar() {
        fechado = true
        pedido = null
        escolhendoArquivo = false
    }

    fun abrir(arquivo: String, sucesso: String, falha: String) {
        if (fechado || pedido != null || activity.isFinishing || activity.isDestroyed) return
        val novo = Pedido(File(arquivo), sucesso, falha)
        pedido = novo
        try {
            escolhendoArquivo = true
            @Suppress("DEPRECATION")
            activity.startActivityForResult(documento(), PEDIDO)
        } catch (_: Exception) {
            pedido = null
            escolhendoArquivo = false
            avisar(falha)
        }
    }

    fun resultado(codigo: Int, resultado: Int, data: Intent?): Boolean {
        if (codigo != PEDIDO) return false
        if (!escolhendoArquivo) return true
        escolhendoArquivo = false
        val atual = pedido ?: return true
        val uri = data?.data
        if (resultado != Activity.RESULT_OK || uri == null) {
            pedido = null
            return true
        }
        Thread({
            val mensagem = try {
                val texto = limpar(lerFim(atual.arquivo)).toByteArray(Charsets.UTF_8)
                activity.contentResolver.openOutputStream(uri, "wt")?.use { it.write(texto) }
                    ?: throw IllegalStateException()
                atual.sucesso
            } catch (_: Exception) {
                atual.falha
            }
            activity.runOnUiThread {
                if (pedido === atual) {
                    pedido = null
                    avisar(mensagem)
                }
            }
        }, "nuvio-baixar-registro").start()
        return true
    }

    private fun avisar(mensagem: String) {
        if (!fechado && !activity.isFinishing && !activity.isDestroyed)
            Toast.makeText(activity, mensagem, Toast.LENGTH_LONG).show()
    }

    companion object {
        const val PEDIDO = 49117
        const val MAX_BYTES = 200 * 1024

        fun documento() = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "text/plain"
            putExtra(Intent.EXTRA_TITLE, "Nuvio-Log-" +
                SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()) + ".txt")
        }

        fun lerFim(arquivo: File): String = RandomAccessFile(arquivo, "r").use { origem ->
            val tamanho = origem.length()
            val inicio = maxOf(0L, tamanho - MAX_BYTES)
            origem.seek(inicio)
            val dados = ByteArray((tamanho - inicio).toInt())
            origem.readFully(dados)
            val texto = String(dados, Charsets.UTF_8)
            // Nao exponha uma URL/credencial cortada antes do inicio da janela.
            if (inicio == 0L) texto else texto.substringAfter('\n', "")
        }

        fun limpar(texto: String): String {
            var valor = texto.replace("\\/", "/")
            valor = Regex("(?i)\\b(?:https?|ftp|stremio)://[^\\s<>\\\"']+").replace(valor) { trecho ->
                try {
                    val url = URI(trecho.value)
                    "${url.scheme}://${url.host ?: "redacted"}/[…]"
                } catch (_: Exception) { "[url]" }
            }
            valor = Regex("(?i)magnet:\\?[^\\s<>\\\"']+").replace(valor, "[magnet]")
            valor = Regex("(?im)[\\\"']?(authorization|cookie|set-cookie)[\\\"']?\\s*[:=].*$")
                .replace(valor, "[…] ")
            // Quantificadores possessivos: um valor longo nao cresce a pilha
            // do motor de regex, inclusive quando contem aspas escapadas.
            // O arquivo pode terminar no meio de um campo ainda sendo escrito.
            valor = Regex("""(?i)["']?\b(api[_-]?key|access[_-]?token|refresh[_-]?token|password|passwd|secret|token|chave|senha|username)\b["']?\s*[:=]\s*(?:"(?:[^"\\\r\n]++|\\[^\r\n]?)*+(?:"|(?=[\r\n]|\z))|'(?:[^'\\\r\n]++|\\[^\r\n]?)*+(?:'|(?=[\r\n]|\z))|[^\s,;}]++)""")
                .replace(valor, "[…]")
            valor = Regex("(?i)\\bBearer\\s+[^\\s,;]+").replace(valor, "Bearer […]")
            return Regex("\\beyJ[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\.[A-Za-z0-9_-]+\\b")
                .replace(valor, "[…]")
        }
    }
}
