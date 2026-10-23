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

// DESTINO DO LINK DO ADDON, por sessao do player. O link do addon (AIOStreams,
// Torrentio, MediaFusion...) redireciona para o CDN do debrid, e cada abertura
// nova do Media3 (o indice no fim do arquivo, a volta ao inicio, cada seek)
// pagava esse redirecionamento de novo: 1-2 s pelo Torrentio, ate 10 s pelo
// MediaFusion no Shield (16/10/2026). Guardado o destino, a abertura seguinte
// vai direto ao CDN; se ele recusar (link vencido, 403/410) ou cair, volta UMA
// vez ao link do addon, que pede um novo. Mesma ideia do cdnlink.c do .tpk
// (PR #567), aqui dentro do proprio DataSource. Nunca loga o destino: o link
// do CDN carrega token.
class DestinosAddon(private val agora: () -> Long = { android.os.SystemClock.elapsedRealtime() }) {
    companion object {
        // O TorBox da ~3 h ao link do CDN; o resto do caminho cobre a sobra.
        const val VALIDADE_MS = 2L * 60 * 60 * 1000
    }
    private var original: String? = null
    private var destino: URL? = null
    private var tamanho = -1L
    private var desde = 0L

    @Synchronized fun buscar(orig: String): URL? =
        if (orig == original && agora() - desde < VALIDADE_MS) destino else null

    // Tamanho do arquivo que o destino guardado serviu (-1 = desconhecido).
    // Uma resposta "OK" do destino com OUTRO tamanho nao e este arquivo: um
    // CDN vencido pode responder 200 com um clipe de erro em vez de 403.
    @Synchronized fun tamanhoDe(orig: String): Long = if (orig == original) tamanho else -1L

    // So guarda um redirecionamento de verdade (host diferente).
    @Synchronized fun lembrar(orig: String, fim: URL, total: Long = -1L) {
        val o = try { URL(orig) } catch (_: java.net.MalformedURLException) { return }
        if (fim.host.isNullOrEmpty() || fim.host.equals(o.host, true)) return
        if (orig == original && fim == destino && (total <= 0 || total == tamanho)) return
        original = orig; destino = fim; tamanho = if (total > 0) total else -1L; desde = agora()
    }

    @Synchronized fun esquecer(orig: String) {
        if (orig == original) { original = null; destino = null }
    }
}

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
    private val destinos: DestinosAddon? = null,
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
        // Menor pedaco do FIM DO ARQUIVO (ver `cauda` em abrir).
        private const val PEDACO_CAUDA_MIN = 128 shl 10

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
        // Uma fabrica por sessao do player (NvPlayer): o destino vale para todas
        // as aberturas dela.
        private val destinos = DestinosAddon()
        override fun createDataSource(): DataSource = Seletor(ua, props, unica.createDataSource(), medidor, token, destinos)
    }

    // Escolhe por pedido: paralelo para arquivo progressivo, unica para o resto.
    // Os ouvintes de transferencia (medidor de banda do ExoPlayer) vao para os dois.
    private class Seletor(private val ua: String?, private val props: Map<String, String>,
                          private val unica: DataSource,
                          private val medidor: PassivoMedidor?, private val token: Long,
                          private val destinos: DestinosAddon) : DataSource {
        private var atual: DataSource? = null
        private val ouvintes = ArrayList<androidx.media3.datasource.TransferListener>()
        override fun open(dataSpec: DataSpec): Long {
            val d = if (serve(dataSpec.uri)) ParaleloDataSource(ua, props, medidor = medidor, token = token, destinos = destinos).also { p ->
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
    private var pedacoAtual = pedaco
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

    private fun esperarTentativa(indice: Int, atrasoMs: Long = -1L) {
        conferirCancelamento()
        val ms = if (atrasoMs >= 0) atrasoMs
                   else ESPERA_TENTATIVA_MS[indice.coerceAtMost(ESPERA_TENTATIVA_MS.lastIndex)]
        if (ms <= 0) return
        try { dormir(ms) }
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
    internal fun conectarComRetry(u: URL, ini: Long, ultimo: Long, atrasoPrimeiraMs: Long = -1L): HttpURLConnection {
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
                    esperarTentativa(tentativa, if (tentativa == 0) atrasoPrimeiraMs else -1L)
                }
            }
        }
        throw FalhaDownloadParalelo(erro ?: IOException("conexao falhou"))
    }

    // Abertura pelo destino ja conhecido do link do addon; recusa ou queda
    // volta ao link do addon com o retry normal, e o destino novo fica guardado.
    private fun abrirSonda(original: URL, ini: Long, ultimo: Long): HttpURLConnection {
        val chave = original.toString()
        val guardado = destinos?.buscar(chave)
        if (guardado != null) {
            try {
                val c = conectar(guardado, ini, ultimo)
                val esperado = destinos?.tamanhoDe(chave) ?: -1L
                val visto = totalDe(c)
                if (c.responseCode in 200..299 && (esperado <= 0 || visto == esperado)) {
                    Log.i(TAG, "destino guardado: ${guardado.host} direto, sem passar por ${original.host}")
                    return c
                }
                if (c.responseCode in 200..299)
                    Log.i(TAG, "destino guardado serviu outro arquivo ($visto bytes, esperado $esperado); de volta a ${original.host}")
                else Log.i(TAG, "destino guardado recusou (HTTP ${c.responseCode}); de volta a ${original.host}")
                desconectar(c)
            } catch (e: IOException) {
                if (foiCancelado(e)) throw cancelado(e)
                Log.i(TAG, "destino guardado falhou (${e.javaClass.simpleName}); de volta a ${original.host}")
            }
            destinos?.esquecer(chave)
        }
        val c = conectarComRetry(original, ini, ultimo, 0L)
        if (c.responseCode in 200..299) destinos?.lembrar(chave, c.url, totalDe(c))
        return c
    }

    // Tamanho do arquivo inteiro segundo a resposta: o total do Content-Range
    // num 206, o corpo num 200 (o servidor ignorou o Range). -1 se nao se sabe.
    private fun totalDe(c: HttpURLConnection): Long = when (c.responseCode) {
        206 -> (c.getHeaderField("Content-Range") ?: "").substringAfter('/', "").trim().toLongOrNull() ?: -1L
        200 -> c.contentLengthLong.takeIf { it > 0 } ?: -1L
        else -> -1L
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
        val pedido = dataSpec.length
        if (pedido == 0L) {
            // Janela vazia (DataSpec de 0 byte): abre vazio, sem pedir um Range
            // invalido (ultimo < primeiro) e sem gastar conexao.
            aberto = true
            transferStarted(dataSpec)
            return 0L
        }
        // A sonda cabe no que foi pedido, mas nunca passa de PRIMEIRO: um
        // buraco do cache maior que a janela (seek para tras sobre regiao
        // sem cache) entraria inteiro num ByteArray so, na thread de quem
        // chamou, antes do pool existir.
        val primeiroUltimo = pos + (if (pedido != C.LENGTH_UNSET.toLong()) minOf(pedido, PRIMEIRO.toLong()) else PRIMEIRO.toLong()) - 1
        // Primeiro pedaco pela URL original: segue os redirecionamentos do debrid
        // uma vez so; os pedacos seguintes vao direto ao endereco final. Uma
        // abertura seguinte da mesma sessao ja comeca no destino (DestinosAddon).
        val c = abrirSonda(URL(dataSpec.uri.toString()), pos, primeiroUltimo)
        val code = c.responseCode
        if (code == 206) {
            val cr = c.getHeaderField("Content-Range") ?: ""
            total = cr.substringAfter('/', "").trim().toLongOrNull() ?: C.LENGTH_UNSET.toLong()
        }
        // Janela que ESTA abertura pode servir: o tamanho que o ExoPlayer
        // pediu ou, na abertura progressiva comum (length UNSET), o resto do
        // arquivo — nunca o pedaco de sonda, senao nenhuma abertura comum
        // caberia no paralelo e tudo viraria conexao unica.
        val janela = when {
            pedido != C.LENGTH_UNSET.toLong() -> pedido
            total > 0 -> total - pos
            else -> C.LENGTH_UNSET.toLong()
        }
        val paralelo = code == 206 && total > 0 && janela > pedaco.toLong() * 2
        // FIM DO ARQUIVO: o indice do MKV (Cues) e o moov do MP4 ficam nos
        // ultimos MB, e o Media3 vai ate la ANTES do primeiro quadro. Pela
        // conexao unica, os 4 MB finais de um remux de 50 GB levaram 24 s no
        // Shield (Ant-Man, 16/10/2026: 170 KB/s naquele trecho do TorBox) e a
        // imagem esperou tudo; os 612 KB finais de outro remux, 5,3 s. Uma
        // janela que vai ate o fim e tem mais de dois pedacos minimos vai pelo
        // pool: a sonda fica com a sua parte e o resto se divide entre as
        // outras conexoes, todas ao mesmo tempo.
        val cauda = !paralelo && code == 206 && total > 0 &&
            janela > PEDACO_CAUDA_MIN.toLong() * 2 && pos + janela >= total
        if (!paralelo && !cauda) {
            Log.i(TAG, "conexao unica: HTTP $code, total=$total, pos=$pos (${dataSpec.uri.host})")
            // Sem Range (ou pequeno demais para valer a pena): conexao unica.
            if (code == 206 || code == 200) {
                // 206 so da sonda: reabre ate o fim da janela pedida (sem teto
                // se ela e o arquivo inteiro). Janela que cabe na sonda segue.
                if (code == 206 && (pedido == C.LENGTH_UNSET.toLong() || pedido > PRIMEIRO)) {
                    // A sonda acabou de resolver o destino: reabre nele.
                    val alvo = c.url
                    desconectar(c)
                    unicaCon = conectarComRetry(alvo, pos,
                        if (pedido == C.LENGTH_UNSET.toLong()) -1 else pos + pedido - 1)
                } else unicaCon = c
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
        fim = if (pedido != C.LENGTH_UNSET.toLong()) minOf(total, pos + pedido) else total
        // No fim do arquivo a sonda le so a sua parte (o pedido dela ja foi ate
        // PRIMEIRO; o resto daquela resposta e descartado ao desconectar).
        val parte = ((fim - pos + conexoes - 1) / conexoes.coerceAtLeast(1))
            .coerceIn(PEDACO_CAUDA_MIN.toLong(), PRIMEIRO.toLong())
        val sondaFim = minOf(primeiroUltimo + 1, fim, if (cauda) pos + parte else Long.MAX_VALUE)
        if (cauda) {
            val outras = (conexoes - 1).coerceAtLeast(1)
            val resto = fim - sondaFim
            pedacoAtual = ((resto + outras - 1) / outras)
                .coerceIn(PEDACO_CAUDA_MIN.toLong(), pedaco.toLong()).toInt()
            proximoPedido = sondaFim
            exec = Executors.newFixedThreadPool(conexoes)
            encher()
        }
        val primeiro = try { lerTudo(c, if (cauda) sondaFim - pos else -1L) } finally { desconectar(c) }
        if (cauda && primeiro.size.toLong() != sondaFim - pos)
            throw IOException("sonda curta: ${primeiro.size} de ${sondaFim - pos}")
        bufIni = pos
        buf = primeiro
        if (!cauda) {
            proximoPedido = pos + primeiro.size
            exec = Executors.newFixedThreadPool(conexoes)
            encher()
        }
        aberto = true
        transferStarted(dataSpec)
        if (cauda) Log.i(TAG, "paralelo no fim do arquivo: ${fim - pos} bytes, pedaco ${pedacoAtual shr 10} KB a partir de $pos")
        else Log.i(TAG, "paralelo: $conexoes conexoes, pedaco ${pedaco shr 20} MB, ${(fim - pos) shr 20} MB a partir de $pos")
        return fim - pos
    }

    // `limite` >= 0: para ali (o resto da resposta fica para quem desconectar).
    private fun lerTudo(c: HttpURLConnection, limite: Long = -1L): ByteArray {
        val esperado = c.contentLengthLong
        val cap = if (limite >= 0) limite else if (esperado > 0) esperado else pedaco.toLong()
        val out = java.io.ByteArrayOutputStream(cap.toInt())
        val origem = PassivoMedidor.origemDe(c.url)
        conferirCancelamento()
        c.inputStream.use { inp ->
            val tmp = ByteArray(64 * 1024)
            while (limite < 0 || out.size() < limite) {
                conferirCancelamento()
                val quero = if (limite < 0) tmp.size else minOf(tmp.size.toLong(), limite - out.size()).toInt()
                val n = inp.read(tmp, 0, quero)
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
            val ult = minOf(fim, ini + pedacoAtual) - 1
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
