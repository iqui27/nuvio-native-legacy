package space.nuvio.nativelegacy

import android.net.Uri
import android.util.Log
import androidx.media3.common.C
import androidx.media3.exoplayer.upstream.DefaultLoadErrorHandlingPolicy
import androidx.media3.exoplayer.upstream.LoadErrorHandlingPolicy
import androidx.media3.datasource.BaseDataSource
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.HttpDataSource
import java.io.IOException
import java.io.InputStream
import java.io.InterruptedIOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.Collections
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future
import java.util.concurrent.TimeUnit

// DOWNLOAD EM VARIAS CONEXOES PARA ARQUIVO DE VIDEO PROGRESSIVO (MKV/MP4).
//
// POR QUE: o Android limita o buffer de recepcao de UMA conexao TCP (na TCL
// Smart TV Pro, tcp_rmem maximo de 2 MB; app comum nao passa de 1 MB por
// socket). A 250 ms de distancia isso da ~20 Mbps por conexao — exatamente os
// 18 Mbps que o teste de velocidade do app media contra o debrid na Europa,
// contra 60 na LG e ~300 num speedtest com servidor perto (medido em
// 30/09/2026: Hetzner EUA 27 Mbps na TV, 97 no Mac da mesma rede). Varias
// conexoes somam: o arquivo vem em pedacos de Range baixados ao mesmo tempo
// e entregues ao ExoPlayer EM ORDEM.
//
// So entra para http(s) que nao seja manifesto/segmento de HLS/DASH (esses ja
// sao pedacos pequenos) e so se o servidor honrar Range (206 com Content-Range).
// Senao, devolve o fluxo da conexao unica, como o DefaultHttpDataSource.
internal class FalhaDownloadParalelo(causa: IOException) : IOException(causa)

/** Nao repete no Loader o ciclo de tentativas que este DataSource ja esgotou. */
internal class PoliticaErroParalelo : DefaultLoadErrorHandlingPolicy() {
    override fun getRetryDelayMsFor(loadErrorInfo: LoadErrorHandlingPolicy.LoadErrorInfo): Long {
        var causa: Throwable? = loadErrorInfo.exception
        repeat(16) {
            if (causa is FalhaDownloadParalelo) return C.TIME_UNSET
            causa = causa?.cause ?: return super.getRetryDelayMsFor(loadErrorInfo)
        }
        return super.getRetryDelayMsFor(loadErrorInfo)
    }
}

class ParaleloDataSource(
    private val ua: String?,
    private val props: Map<String, String>,
    private val conexoes: Int = CONEXOES,
    private val pedaco: Int = PEDACO,
    // StreamFit passivo (PassivoMedidor.kt): conta os bytes LIDOS DO SOCKET
    // por estas transferencias, nunca a entrega de memoria ao ExoPlayer.
    private val medidor: PassivoMedidor? = null,
    private val token: Long = 0L,
    // Injetaveis para os testes de falha/cancelamento; producao usa a rede real.
    private val novaConexao: (URL) -> HttpURLConnection = { it.openConnection() as HttpURLConnection },
    private val dormir: (Long) -> Unit = { Thread.sleep(it) },
) : BaseDataSource(true) {

    // ESTATISTICAS AO VIVO (src/estatvivo.h, via NvPlayer.nativeStats): o que
    // ESTAS transferencias contam, por abertura do player (NvPlayer zera). So
    // contadores atomicos, nenhuma alocacao no caminho do byte.
    object Estat {
        val bytes = java.util.concurrent.atomic.AtomicLong()
        val conexoes = java.util.concurrent.atomic.AtomicInteger()
        @Volatile var usado = false
        @Volatile var primeiroByteMs = -1
        @Volatile var host = ""
        fun zerar() { bytes.set(0); conexoes.set(0); usado = false; primeiroByteMs = -1; host = "" }
        fun hostDe(u: URL) {
            val porta = if (u.port < 0 || u.port == u.defaultPort) "" else ":${u.port}"
            host = (u.host ?: "").lowercase() + porta
        }
    }

    companion object {
        private const val TAG = "nuvio-paralelo"
        const val CONEXOES = 4
        const val PEDACO = 4 shl 20           // 4 MB por pedido
        private const val ADIANTE = 3         // pedacos em voo por conexao
        private const val TENTATIVAS = 3
        private val ESPERA_TENTATIVA_MS = longArrayOf(250L, 750L)
        // O primeiro pedido e menor: a imagem nao espera 4 MB chegarem.
        private const val PRIMEIRO = 1 shl 20

        // Manifesto e segmento de streaming adaptativo: conexao unica.
        fun serve(uri: Uri): Boolean {
            val s = uri.scheme ?: return false
            if (!s.equals("http", true) && !s.equals("https", true)) return false
            val p = (uri.path ?: "").lowercase()
            return !(p.endsWith(".m3u8") || p.endsWith(".mpd") || p.endsWith(".ts") ||
                p.endsWith(".m4s") || p.endsWith(".aac") || p.endsWith(".vtt") ||
                p.endsWith(".srt") || p.contains("/manifest"))
        }
    }

    class Factory(private val ua: String?, private val props: Map<String, String>,
                  private val unica: HttpDataSource.Factory,
                  private val medidor: PassivoMedidor? = null,
                  private val token: Long = 0L) : DataSource.Factory {
        override fun createDataSource(): DataSource = Seletor(ua, props, unica.createDataSource(), medidor, token)
    }

    // Escolhe por pedido: paralelo para arquivo progressivo, unica para o resto.
    // Os ouvintes de transferencia (medidor de banda do ExoPlayer) vao para os dois.
    private class Seletor(private val ua: String?, private val props: Map<String, String>,
                          private val unica: DataSource,
                          private val medidor: PassivoMedidor?, private val token: Long) : DataSource {
        private var atual: DataSource? = null
        private val ouvintes = ArrayList<androidx.media3.datasource.TransferListener>()
        override fun open(dataSpec: DataSpec): Long {
            val d = if (serve(dataSpec.uri)) ParaleloDataSource(ua, props, medidor = medidor, token = token).also { p ->
                for (o in ouvintes) p.addTransferListener(o)
            } else unica
            atual = d
            return d.open(dataSpec)
        }
        override fun read(buffer: ByteArray, offset: Int, length: Int): Int =
            atual?.read(buffer, offset, length) ?: C.RESULT_END_OF_INPUT
        override fun getUri(): Uri? = atual?.uri
        override fun getResponseHeaders(): Map<String, List<String>> = atual?.responseHeaders ?: emptyMap()
        override fun close() { try { atual?.close() } finally { atual = null } }
        override fun addTransferListener(transferListener: androidx.media3.datasource.TransferListener) {
            ouvintes.add(transferListener)
            unica.addTransferListener(transferListener)
        }
    }

    private var uri: Uri? = null
    private var urlFinal: URL? = null
    private var pos = 0L          // proximo byte a entregar
    private var fim = 0L          // exclusivo
    private var total = C.LENGTH_UNSET.toLong()
    private var exec: ExecutorService? = null
    private val pedidos = HashMap<Long, Future<ByteArray>>()   // inicio do pedaco -> dados
    private var proximoPedido = 0L
    private var buf: ByteArray? = null
    private var bufIni = 0L
    // Conexao unica: servidor sem Range, ou arquivo pequeno.
    private var unicaCon: HttpURLConnection? = null
    private var unicaIn: InputStream? = null
    private var unicaOrigem: String? = null
    private var aberto = false
    @Volatile private var fechado = false
    // Future.cancel(true) nao interrompe connect/read de HttpURLConnection em
    // todos os Androids. Guardar as conexoes permite que close() as desconecte.
    private val ativas = Collections.newSetFromMap(
        java.util.concurrent.ConcurrentHashMap<HttpURLConnection, Boolean>())

    private class Cancelamento(causa: Throwable? = null) : InterruptedIOException("cancelado") {
        init { if (causa != null) initCause(causa) }
    }

    private fun cancelado(causa: Throwable? = null): InterruptedIOException = Cancelamento(causa)

    private fun foiCancelado(e: IOException): Boolean =
        fechado || Thread.currentThread().isInterrupted || e is Cancelamento

    private fun conferirCancelamento() {
        if (fechado || Thread.currentThread().isInterrupted) throw cancelado()
    }

    private fun desconectar(c: HttpURLConnection?) {
        if (c == null) return
        ativas.remove(c)
        c.disconnect()
    }

    private fun esperarTentativa(indice: Int) {
        conferirCancelamento()
        try { dormir(ESPERA_TENTATIVA_MS[indice.coerceAtMost(ESPERA_TENTATIVA_MS.lastIndex)]) }
        catch (e: InterruptedException) { Thread.currentThread().interrupt(); throw cancelado(e) }
        conferirCancelamento()
    }

    private fun conectar1(u: URL, ini: Long, ultimo: Long): HttpURLConnection {
        conferirCancelamento()
        val c = novaConexao(u)
        ativas.add(c)
        if (fechado) { desconectar(c); throw cancelado() }
        c.connectTimeout = 15000
        c.readTimeout = 20000
        // Redirecionamento seguido A MAO (conectar): o HttpURLConnection nao
        // segue troca de protocolo (https -> http), e o AIOStreams/ElfHosted faz
        // exatamente isso — a fonte ficava em "Opening source" com um 302.
        c.instanceFollowRedirects = false
        if (ua != null) c.setRequestProperty("User-Agent", ua)
        for ((k, v) in props) c.setRequestProperty(k, v)
        c.setRequestProperty("Accept-Encoding", "identity")
        c.setRequestProperty("Range", if (ultimo >= 0) "bytes=$ini-$ultimo" else "bytes=$ini-")
        return c
    }

    private fun conectar(u0: URL, ini: Long, ultimo: Long): HttpURLConnection {
        var u = u0
        repeat(10) {
            val t0 = android.os.SystemClock.elapsedRealtime()
            val c = conectar1(u, ini, ultimo)
            val code = try { c.responseCode } catch (e: IOException) {
                desconectar(c)
                throw e
            }
            if (fechado) { desconectar(c); throw cancelado() }
            Estat.primeiroByteMs = (android.os.SystemClock.elapsedRealtime() - t0).toInt()
            if (code in 300..399 && code != 304) {
                val loc = c.getHeaderField("Location")
                desconectar(c)
                if (loc.isNullOrEmpty()) throw IOException("HTTP $code sem Location")
                u = URL(u, loc)
                return@repeat
            }
            Estat.hostDe(c.url)
            return c
        }
        throw IOException("redirecionamentos demais")
    }

    // A abertura inicial depois de um seek antes nao tinha retry algum. Um
    // ConnectException escapava daqui e encerrava a fonte, embora os pedacos
    // posteriores aparentassem ter TENTATIVAS. Mantem o retry no Loader (nao no
    // fio do player) e curto o bastante para nao esconder uma rota morta.
    internal fun conectarComRetry(u: URL, ini: Long, ultimo: Long): HttpURLConnection {
        var erro: IOException? = null
        repeat(TENTATIVAS) { tentativa ->
            try { return conectar(u, ini, ultimo) }
            catch (e: IOException) {
                erro = e
                // SocketTimeoutException tambem herda InterruptedIOException:
                // so a nossa sentinela (ou close/interrupcao real) cancela.
                if (foiCancelado(e)) throw cancelado(e)
                if (tentativa + 1 < TENTATIVAS) {
                    Log.w(TAG, "conexao ${tentativa + 1}/$TENTATIVAS falhou em ${u.host}; tentando de novo")
                    esperarTentativa(tentativa)
                }
            }
        }
        throw FalhaDownloadParalelo(erro ?: IOException("conexao falhou"))
    }

    override fun open(dataSpec: DataSpec): Long {
        // Conectar, esperar o primeiro byte e baixar o primeiro pedaco e
        // transferencia ativa (o tempo entra na cobertura do segundo).
        medidor?.inicio(token)
        Estat.usado = true
        Estat.conexoes.incrementAndGet()
        try { return abrir(dataSpec) }
        catch (e: IOException) { if (foiCancelado(e)) throw cancelado(e) else throw e }
        finally { Estat.conexoes.decrementAndGet(); medidor?.fim(token) }
    }

    private fun abrir(dataSpec: DataSpec): Long {
        fechado = false
        uri = dataSpec.uri
        transferInitializing(dataSpec)
        Log.i(TAG, "abrindo ${dataSpec.uri.host} a partir de ${dataSpec.position}")
        pos = dataSpec.position
        // Primeiro pedaco pela URL original: segue os redirecionamentos do debrid
        // uma vez so; os pedacos seguintes vao direto ao endereco final.
        val c = conectarComRetry(URL(dataSpec.uri.toString()), pos, pos + PRIMEIRO - 1)
        val code = c.responseCode
        if (code == 206) {
            val cr = c.getHeaderField("Content-Range") ?: ""
            total = cr.substringAfter('/', "").trim().toLongOrNull() ?: C.LENGTH_UNSET.toLong()
        }
        if (code != 206 || total <= 0 || total - pos <= pedaco.toLong() * 2) {
            Log.i(TAG, "conexao unica: HTTP $code, total=$total, pos=$pos (${dataSpec.uri.host})")
            // Sem Range (ou pequeno demais para valer a pena): conexao unica.
            if (code == 206 || code == 200) {
                // 206 de um pedaco so: reabre sem teto para o arquivo inteiro.
                if (code == 206) { desconectar(c); unicaCon = conectarComRetry(URL(dataSpec.uri.toString()), pos, -1) }
                else unicaCon = c
                val uc = unicaCon!!
                if (uc.responseCode !in 200..299) {
                    val erro = HttpDataSource.InvalidResponseCodeException(
                        uc.responseCode, uc.responseMessage, null, uc.headerFields, dataSpec, ByteArray(0))
                    desconectar(uc)
                    unicaCon = null
                    throw erro
                }
                unicaIn = uc.inputStream
                Estat.conexoes.incrementAndGet()
                unicaOrigem = PassivoMedidor.origemDe(uc.url)
                if (code == 200 && pos > 0) unicaIn!!.skip(pos)   // servidor ignorou Range
                aberto = true
                transferStarted(dataSpec)
                val len = if (dataSpec.length != C.LENGTH_UNSET.toLong()) dataSpec.length
                          else if (total > 0) total - pos else C.LENGTH_UNSET.toLong()
                return len
            }
            val msg = c.responseMessage
            val hdr = c.headerFields
            desconectar(c)
            throw HttpDataSource.InvalidResponseCodeException(code, msg, null, hdr, dataSpec, ByteArray(0))
        }
        urlFinal = c.url
        fim = if (dataSpec.length != C.LENGTH_UNSET.toLong()) minOf(total, pos + dataSpec.length) else total
        val primeiro = try { lerTudo(c) } finally { desconectar(c) }
        bufIni = pos
        buf = primeiro
        proximoPedido = pos + primeiro.size
        exec = Executors.newFixedThreadPool(conexoes)
        encher()
        aberto = true
        transferStarted(dataSpec)
        Log.i(TAG, "paralelo: $conexoes conexoes, pedaco ${pedaco shr 20} MB, ${(fim - pos) shr 20} MB a partir de $pos")
        return fim - pos
    }

    private fun lerTudo(c: HttpURLConnection): ByteArray {
        val esperado = c.contentLengthLong
        val out = java.io.ByteArrayOutputStream(if (esperado > 0) esperado.toInt() else pedaco)
        val origem = PassivoMedidor.origemDe(c.url)
        conferirCancelamento()
        c.inputStream.use { inp ->
            val tmp = ByteArray(64 * 1024)
            while (true) {
                conferirCancelamento()
                val n = inp.read(tmp)
                conferirCancelamento()
                if (n < 0) break
                out.write(tmp, 0, n)
                medidor?.bytes(token, origem, n)
                Estat.bytes.addAndGet(n.toLong())
            }
        }
        return out.toByteArray()
    }

    // Mantem ate conexoes * ADIANTE pedacos pedidos a frente da leitura.
    private fun encher() {
        val ex = exec ?: return
        val u = urlFinal ?: return
        while (pedidos.size < conexoes * ADIANTE && proximoPedido < fim) {
            val ini = proximoPedido
            val ult = minOf(fim, ini + pedaco) - 1
            pedidos[ini] = ex.submit<ByteArray> { baixar(u, ini, ult) }
            proximoPedido = ult + 1
        }
    }

    private fun baixar(u: URL, ini: Long, ult: Long): ByteArray {
        var ultimoErro: IOException? = null
        repeat(TENTATIVAS) {
            conferirCancelamento()
            // Uma conexao paralela ativa, do connect ao ultimo byte. As outras
            // somam no mesmo segundo do medidor; nada e contado duas vezes.
            medidor?.inicio(token)
            Estat.conexoes.incrementAndGet()
            var c: HttpURLConnection? = null
            try {
                c = conectar(u, ini, ult)
                if (c.responseCode != 206) throw IOException("HTTP ${c.responseCode} no pedaco $ini-$ult")
                val d = lerTudo(c)
                if (d.size.toLong() != ult - ini + 1) throw IOException("pedaco curto: ${d.size} de ${ult - ini + 1}")
                return d
            } catch (e: IOException) {
                ultimoErro = e
                if (foiCancelado(e)) throw cancelado(e)
            } finally {
                desconectar(c)
                Estat.conexoes.decrementAndGet()
                medidor?.fim(token)
            }
            if (it + 1 < TENTATIVAS) esperarTentativa(it)
        }
        throw FalhaDownloadParalelo(ultimoErro ?: IOException("pedaco $ini-$ult falhou"))
    }

    override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
        if (length == 0) return 0
        conferirCancelamento()
        unicaIn?.let { inp ->
            // Conexao unica: so o tempo DENTRO do read e transferencia (um
            // stall bloqueia aqui); o intervalo entre reads e o ExoPlayer
            // parado com o buffer cheio, e nao conta.
            medidor?.inicio(token)
            val n = try { inp.read(buffer, offset, length).also { conferirCancelamento() } }
                catch (e: IOException) { if (foiCancelado(e)) throw cancelado(e) else throw e }
                finally { medidor?.fim(token) }
            if (n > 0) { medidor?.bytes(token, unicaOrigem, n); Estat.bytes.addAndGet(n.toLong()) }
            if (n < 0) return C.RESULT_END_OF_INPUT
            bytesTransferred(n)
            return n
        }
        if (pos >= fim) return C.RESULT_END_OF_INPUT
        var b = buf
        if (b == null || pos >= bufIni + b.size) {
            val f = pedidos.remove(pos) ?: run { encher(); pedidos.remove(pos) }
                ?: throw IOException("pedaco $pos nao foi pedido")
            b = try { f.get(60, TimeUnit.SECONDS) }
                catch (e: java.util.concurrent.ExecutionException) { throw (e.cause as? IOException) ?: IOException(e.cause) }
                catch (e: java.util.concurrent.TimeoutException) { throw IOException("pedaco $pos demorou mais de 60 s") }
                catch (e: InterruptedException) { Thread.currentThread().interrupt(); throw cancelado(e) }
                catch (e: java.util.concurrent.CancellationException) { throw cancelado(e) }
            conferirCancelamento()
            buf = b; bufIni = pos
            encher()
        }
        val off = (pos - bufIni).toInt()
        val n = minOf(length, b!!.size - off)
        System.arraycopy(b, off, buffer, offset, n)
        pos += n
        bytesTransferred(n)
        return n
    }

    override fun getUri(): Uri? = uri

    override fun close() {
        fechado = true
        exec?.shutdownNow()
        exec = null
        for (f in pedidos.values) f.cancel(true)
        pedidos.clear()
        // Interrompe connect/read antigos imediatamente ao buscar outra posicao.
        for (c in ativas.toList()) desconectar(c)
        buf = null
        if (unicaIn != null) Estat.conexoes.decrementAndGet()
        try { unicaIn?.close() } catch (_: IOException) {}
        unicaIn = null
        unicaOrigem = null
        desconectar(unicaCon)
        unicaCon = null
        if (aberto) { aberto = false; transferEnded() }
        uri = null
    }
}
