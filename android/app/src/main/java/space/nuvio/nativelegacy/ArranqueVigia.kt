package space.nuvio.nativelegacy

import android.app.AlertDialog
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.os.SystemClock
import android.util.Log
import android.widget.TextView
import org.json.JSONObject
import java.io.File
import java.io.RandomAccessFile
import java.net.HttpURLConnection
import java.net.URL
import java.util.Locale

// VIGIA DO ARRANQUE (#266). TVs Android 11 (TCL 43P745, Shield, uma caixa)
// abrem numa tela preta, o Voltar nao sai e nenhum log chega: o envio do
// registro exige conta, e elas nem chegam ao login. Este vigia roda no fio da
// interface, que segue livre mesmo com o main() do C preso, e le do C a etapa
// (android_etapa) e o contador de quadros (android_quadro):
//   - nenhum quadro em ARRANQUE_MS com o app na frente, ou
//   - o laco parado ha PARADO_MS depois de ja ter desenhado, ou
//   - Voltar apertado com o C sem desenhar (o SDL entrega o Voltar ao C, que
//     preso nunca responde: era o "nao consigo sair, so reiniciando"),
// abre um dialogo Android (janela propria, por cima da SurfaceView do SDL) com
// a etapa, o fim do log, "Enviar registro" (sem conta) e "Sair".
class ArranqueVigia(private val act: NuvioActivity) {
    companion object {
        const val ARRANQUE_MS = 25_000L
        const val PARADO_MS = 30_000L
        const val VOLTAR_PARADO_MS = 5_000L
        private const val TAG = "Nuvio"
    }

    private val h = Handler(Looper.getMainLooper())
    private val nascido = SystemClock.elapsedRealtime()
    private var naFrente = false
    private var naFrenteDesde = 0L
    private var ultQuadros = -1L
    private var ultMudou = SystemClock.elapsedRealtime()
    private var adiadoAte = 0L
    private var dialogo: AlertDialog? = null
    private var texto: TextView? = null
    private var enviando = false
    private val pt = Locale.getDefault().language == "pt"

    private fun t(p: String, e: String) = if (pt) p else e

    private val tique = object : Runnable {
        override fun run() {
            verificar()
            h.postDelayed(this, 1000)
        }
    }

    fun iniciar() { if (!desligado) h.postDelayed(tique, 1000) }
    fun parar() { h.removeCallbacksAndMessages(null); dialogo?.dismiss(); dialogo = null }
    fun frente(sim: Boolean) {
        naFrente = sim
        val agora = SystemClock.elapsedRealtime()
        if (sim) { naFrenteDesde = agora; ultMudou = agora }
    }

    private fun quadros(): Long = try { act.nativeQuadros() } catch (_: Throwable) { -1L }
    private fun etapa(): String = try { act.nativeEtapa() } catch (_: Throwable) { "lib-nao-carregou" }

    private fun verificar() {
        val agora = SystemClock.elapsedRealtime()
        val q = quadros()
        // Um quadro apresentado prova que o app abriu: o vigia se desliga para
        // sempre (falso positivo na TCL: o seletor de perfil ja estava na tela).
        if (q > 0 || etapa() == "pronto") { desligar(); return }
        if (q != ultQuadros) { ultQuadros = q; ultMudou = agora }
        if (dialogo != null || !naFrente || agora < adiadoAte) return
        val travou = if (q <= 0) agora - maxOf(nascido, naFrenteDesde) >= ARRANQUE_MS
                     else agora - maxOf(ultMudou, naFrenteDesde) >= PARADO_MS
        if (travou) mostrar(if (q <= 0) "sem-quadro" else "laco-parado")
    }

    private var desligado = false
    private fun desligar() {
        desligado = true
        h.removeCallbacksAndMessages(null)
    }

    // Voltar: true = o vigia cuidou (o C nao esta respondendo). So com ZERO
    // quadros apresentados e mais de VOLTAR_PARADO_MS desde o inicio do processo.
    fun voltar(): Boolean {
        if (desligado) return false
        val agora = SystemClock.elapsedRealtime()
        if (quadros() > 0 || etapa() == "pronto") { desligar(); return false }
        if (agora - nascido < VOLTAR_PARADO_MS) return false
        if (dialogo == null) mostrar("voltar")
        return true
    }

    private fun estadoJava(): String =
        act.estadoSdl()

    private fun resumo(motivo: String): String {
        val seg = (SystemClock.elapsedRealtime() - nascido) / 1000
        return "[vigia] motivo=$motivo etapa=${etapa()} quadros=${quadros()} t=${seg}s " +
            "${estadoJava()} tv=${Build.MANUFACTURER} ${Build.MODEL} android=${Build.VERSION.RELEASE} " +
            "abi=${Build.SUPPORTED_ABIS.firstOrNull() ?: "?"}"
    }

    private fun mostrar(motivo: String) {
        val linha = resumo(motivo)
        Log.w(TAG, linha)
        val corpo = TextView(act).apply {
            setPadding(48, 24, 48, 0)
            textSize = 13f
            text = mensagem(motivo, null)
        }
        texto = corpo
        val d = AlertDialog.Builder(act, android.R.style.Theme_DeviceDefault_Dialog_Alert)
            .setTitle(if (motivo == "laco-parado") t("O Nuvio parou de responder", "Nuvio stopped responding")
                      else t("O Nuvio não terminou de abrir", "Nuvio didn't finish opening"))
            .setView(corpo)
            .setPositiveButton(t("Enviar registro", "Send log"), null)
            .setNeutralButton(t("Esperar", "Wait"), null)
            .setNegativeButton(t("Sair", "Exit"), null)
            .setCancelable(true)
            .setOnCancelListener { sair() }   // Voltar no dialogo = sair
            .create()
        d.setOnShowListener {
            d.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener { enviar(motivo, linha) }
            d.getButton(AlertDialog.BUTTON_NEUTRAL).setOnClickListener {
                adiadoAte = SystemClock.elapsedRealtime() + 30_000
                d.dismiss(); dialogo = null
            }
            d.getButton(AlertDialog.BUTTON_NEGATIVE).setOnClickListener { sair() }
            d.getButton(AlertDialog.BUTTON_POSITIVE).requestFocus()
        }
        dialogo = d
        try { d.show() } catch (e: Exception) { Log.w(TAG, "vigia: dialogo falhou: $e"); dialogo = null }
    }

    private fun mensagem(motivo: String, status: String?): String {
        val sb = StringBuilder()
        sb.append(t("Parou em: ", "Stuck at: ")).append(etapa())
            .append("  (").append(motivo).append(", ")
            .append((SystemClock.elapsedRealtime() - nascido) / 1000).append(" s)\n")
        sb.append("${Build.MANUFACTURER} ${Build.MODEL} · Android ${Build.VERSION.RELEASE} · ${estadoJava()}\n")
        if (status != null) sb.append('\n').append(status).append('\n')
        sb.append('\n').append(t("Fim do registro:", "End of log:")).append('\n')
        sb.append(fimDoLog(1500).lines().takeLast(10).joinToString("\n"))
        return sb.toString()
    }

    private fun arquivoLog() = File(act.filesDir, "dados/nuvio.log")

    private fun fimDoLog(max: Int): String = try {
        RandomAccessFile(arquivoLog(), "r").use { f ->
            val n = f.length()
            val de = if (n > max) n - max else 0
            f.seek(de)
            val b = ByteArray((n - de).toInt())
            f.readFully(b)
            String(b, Charsets.UTF_8)
        }
    } catch (_: Exception) { "(sem nuvio.log)" }

    // Logcat do proprio processo, so as etiquetas do SDL/EGL/Java do app (o
    // espelho "nuvio" do C ja esta no arquivo, e o player pode logar URL).
    private fun logcat(): String = try {
        val p = Runtime.getRuntime().exec(arrayOf("logcat", "-d", "-t", "300", "-v", "time",
            "--pid=${Process.myPid()}", "-s", "SDL:V", "SDL/APP:V", "SDLActivity:V", "Nuvio:V",
            "libEGL:V", "EGL_emulation:V", "AndroidRuntime:V", "DEBUG:V"))
        val s = p.inputStream.bufferedReader().readText()
        p.waitFor()
        s.takeLast(20 * 1024)
    } catch (e: Exception) { "(logcat indisponivel: $e)" }

    private fun enviar(motivo: String, linha: String) {
        if (enviando) return
        enviando = true
        texto?.text = mensagem(motivo, t("Enviando…", "Sending…"))
        val base = try { act.nativeRecUrl() } catch (_: Throwable) { "" }
        Thread({
            val st = try {
                if (base.isEmpty()) throw IllegalStateException(t("envio indisponível nesta build", "upload unavailable in this build"))
                val log = fimDoLog(40 * 1024)
                val corpo = JSONObject()
                    .put("versao", versao())
                    .put("plataforma", "android")
                    .put("quando", "arranque $motivo")
                    .put("tv", "${Build.MANUFACTURER}-${Build.MODEL}")
                    .put("texto", "$linha\n--- nuvio.log ---\n$log\n--- logcat ---\n${logcat()}")
                    .toString().toByteArray(Charsets.UTF_8)
                val c = URL("$base/v1/registro/arranque").openConnection() as HttpURLConnection
                c.connectTimeout = 15_000; c.readTimeout = 20_000
                c.requestMethod = "POST"; c.doOutput = true
                c.setRequestProperty("Content-Type", "application/json")
                c.outputStream.use { it.write(corpo) }
                val code = c.responseCode
                val resp = try { (if (code < 400) c.inputStream else c.errorStream)?.bufferedReader()?.readText() ?: "" }
                           catch (_: Exception) { "" }
                c.disconnect()
                val cod = try { JSONObject(resp).optString("codigo", "") } catch (_: Exception) { "" }
                if (code in 200..299 && cod.isNotEmpty())
                    t("Enviado. Código do registro: $cod", "Sent. Log code: $cod")
                else if (code in 200..299) t("Enviado.", "Sent.")
                else t("Não foi possível enviar (HTTP $code).", "Couldn't send (HTTP $code).")
            } catch (e: Exception) {
                t("Não foi possível enviar: ", "Couldn't send: ") + (e.message ?: e.javaClass.simpleName)
            }
            Log.w(TAG, "[vigia] envio: $st")
            h.post { enviando = false; texto?.text = mensagem(motivo, st) }
        }, "nuvio-vigia-envio").start()
    }

    private fun versao(): String = try {
        act.packageManager.getPackageInfo(act.packageName, 0).versionName ?: ""
    } catch (_: Exception) { "" }

    // Sai de verdade: o fio do C pode estar preso, entao nada de esperar o
    // join do SDL (onDestroy) — mata o processo; a proxima abertura nasce limpa.
    fun sair() {
        Log.w(TAG, "[vigia] saindo pelo vigia (etapa=${etapa()})")
        try { act.finishAndRemoveTask() } catch (_: Exception) {}
        Process.killProcess(Process.myPid())
    }
}
