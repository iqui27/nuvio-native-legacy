package space.nuvio.nativelegacy

import androidx.media3.common.C
import androidx.media3.datasource.DataSpec
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.IOException
import java.io.InputStream
import java.io.InterruptedIOException
import java.util.concurrent.atomic.AtomicReference
import java.net.HttpURLConnection
import java.net.SocketTimeoutException
import java.net.URL
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

class ParaleloDataSourceTest {
    private open class ConexaoFalsa(url: URL) : HttpURLConnection(url) {
        val desconectou = AtomicBoolean()
        override fun connect() = Unit
        override fun usingProxy(): Boolean = false
        override fun disconnect() { desconectou.set(true) }
        override fun getInputStream(): InputStream = ByteArrayInputStream(byteArrayOf(1))
        override fun getResponseCode(): Int = HTTP_OK
        override fun getResponseMessage(): String = "OK"
    }

    // Responde 206 honrando o Range pedido: devolve exatamente os bytes da
    // janela, como um servidor de Range de verdade.
    private open class ConexaoFaixa(url: URL, private val total: Long) : ConexaoFalsa(url) {
        private val pedidos = HashMap<String, String>()
        override fun setRequestProperty(key: String?, value: String?) {
            if (key != null && value != null) pedidos[key] = value
        }
        override fun getRequestProperty(key: String?): String? = pedidos[key]
        // Range invalido (ultimo < primeiro) e 416, como um servidor de verdade.
        override fun getResponseCode(): Int =
            if (fim() < ini()) HTTP_REQ_TOO_LONG else HTTP_PARTIAL
        override fun getContentLengthLong(): Long = fim() - ini() + 1
        override fun getHeaderField(name: String?): String? =
            if (name == "Content-Range") "bytes ${ini()}-${fim()}/$total" else super.getHeaderField(name)
        override fun getInputStream(): InputStream =
            ByteArrayInputStream(ByteArray((fim() - ini() + 1).toInt()))
        private fun ini(): Long = faixa().first
        private fun fim(): Long = faixa().second
        private fun faixa(): Pair<Long, Long> {
            val corpo = (pedidos["Range"] ?: "bytes=0-").removePrefix("bytes=")
            val ini = corpo.substringBefore('-').toLong()
            val fim = corpo.substringAfter('-', "")
            return if (fim.isEmpty()) ini to (total - 1) else ini to fim.toLong()
        }
    }

    @Test fun aberturaInicialRepeteFalhaDeConexaoComBackoffLimitado() {
        val aberturas = AtomicInteger()
        val conexoes = ArrayList<ConexaoFalsa>()
        val esperas = ArrayList<Long>()
        val fonte = ParaleloDataSource(null, emptyMap(),
            novaConexao = { url ->
                val tentativa = aberturas.incrementAndGet()
                val c = if (tentativa < 3) object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int = throw IOException("sem rota")
                } else ConexaoFalsa(url)
                conexoes += c
                c
            },
            dormir = { esperas += it })

        fonte.conectarComRetry(URL("https://example.invalid/video.mkv"), 0, 1023)
        fonte.close()

        assertEquals(3, aberturas.get())
        assertEquals(listOf(250L, 750L), esperas)
        assertTrue(conexoes.take(2).all { it.desconectou.get() })
    }

    @Test fun timeoutDeSocketTambemRecebeTresTentativas() {
        val aberturas = AtomicInteger()
        val esperas = ArrayList<Long>()
        val fonte = ParaleloDataSource(null, emptyMap(),
            novaConexao = { url ->
                object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int {
                        aberturas.incrementAndGet()
                        throw SocketTimeoutException("timeout")
                    }
                }
            },
            dormir = { esperas += it })

        try {
            fonte.conectarComRetry(URL("https://example.invalid/video.mkv"), 0, 1023)
        } catch (e: FalhaDownloadParalelo) {
            assertTrue(e.cause is SocketTimeoutException)
        }
        finally { fonte.close() }

        assertEquals(3, aberturas.get())
        assertEquals(listOf(250L, 750L), esperas)
    }

    @Test fun closeNaoDevolveCabecalhoObsoleto() {
        lateinit var fonte: ParaleloDataSource
        fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            object : ConexaoFalsa(url) {
                override fun getResponseCode(): Int { fonte.close(); return HTTP_OK }
            }
        }, dormir = { throw AssertionError("cancelamento nao deve repetir") })
        try {
            fonte.conectarComRetry(URL("https://example.invalid/video.mkv"), 0, 3)
            throw AssertionError("devolveu conexao depois de close")
        } catch (_: InterruptedIOException) {} finally { fonte.close() }
    }

    @Test fun closeCancelaLeituraUnicaMesmoSeSocketDevolverBytes() {
        val fonte = ParaleloDataSource(null, emptyMap())
        val entrada = ParaleloDataSource::class.java.getDeclaredField("unicaIn")
        entrada.isAccessible = true
        ParaleloDataSource.Estat.conexoes.incrementAndGet()
        entrada.set(fonte, object : InputStream() {
            override fun read(): Int { fonte.close(); return 1 }
        })
        try {
            fonte.read(ByteArray(1), 0, 1)
            throw AssertionError("entregou bytes depois de close")
        } catch (_: InterruptedIOException) {} finally { fonte.close() }
        assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
    }

    @Test fun pedacoRepeteConnectEDesconectaAntesDoBackoff() {
        val aberturas = AtomicInteger()
        val conexoes = ArrayList<ConexaoFalsa>()
        val esperas = ArrayList<Long>()
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            val tentativa = aberturas.incrementAndGet()
            val c = object : ConexaoFalsa(url) {
                override fun getResponseCode(): Int =
                    if (tentativa < 3) throw IOException("sem rota") else HTTP_PARTIAL
                override fun getInputStream(): InputStream = ByteArrayInputStream(byteArrayOf(1, 2, 3, 4))
                override fun getContentLengthLong(): Long = 4
            }
            conexoes += c
            c
        }, dormir = {
            assertTrue(conexoes.last().desconectou.get())
            assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
            esperas += it
        })
        // Os pedacos leem o destino da abertura (urlFinal), que renovarDestino
        // pode trocar; aqui ele e posto a mao, sem abertura.
        val urlFinal = ParaleloDataSource::class.java.getDeclaredField("urlFinal")
        urlFinal.isAccessible = true
        urlFinal.set(fonte, URL("https://example.invalid/video.mkv"))
        val baixar = ParaleloDataSource::class.java.getDeclaredMethod("baixar",
            Long::class.javaPrimitiveType, Long::class.javaPrimitiveType)
        baixar.isAccessible = true
        try {
            val dados = baixar.invoke(fonte, 0L, 3L) as ByteArray
            assertEquals(listOf<Byte>(1, 2, 3, 4), dados.toList())
        } finally { fonte.close() }
        assertEquals(3, aberturas.get())
        assertEquals(listOf(250L, 750L), esperas)
        assertTrue(conexoes.all { it.desconectou.get() })
        assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
    }

    @Test fun janelaPequenaUsaConexaoUnicaSemPool() {
        // Buraco pequeno do seek-cache: 2 bytes num arquivo de 24 GB. Sobra da
        // regressao do paralelo: o reopen em offset-2 com 0 MB restante.
        val aberturas = AtomicInteger()
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            aberturas.incrementAndGet()
            ConexaoFaixa(url, 24891962527L)
        }, dormir = { throw AssertionError("janela pequena nao deve esperar retry") })
        val exec = ParaleloDataSource::class.java.getDeclaredField("exec")
        exec.isAccessible = true
        val pos = 613230488L
        val len = fonte.open(DataSpec.Builder()
            .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
            .setPosition(pos)
            .setLength(2L)
            .build())
        assertEquals(2L, len)
        assertEquals(1, aberturas.get())      // uma conexao, nenhum reopen
        assertEquals(null, exec.get(fonte))   // sem pool
        val buf = ByteArray(2)
        assertEquals(2, fonte.read(buf, 0, 2))
        assertEquals(0, buf[0].toInt() and 0xFF)
        assertEquals(C.RESULT_END_OF_INPUT.toLong(), fonte.read(buf, 0, 2).toLong())
        fonte.close()
    }

    @Test fun janelaMediaMaiorQueASondaServeAJanelaInteira() {
        // 3 MB pedidos: acima da sonda (1 MB), abaixo do limiar do pool. A
        // conexao unica nao pode herdar o teto da sonda e acabar aos 1 MB.
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            ConexaoFaixa(url, 24891962527L)
        }, dormir = { throw AssertionError("conexao viva nao deve esperar retry") })
        val janela = 3L shl 20
        val len = fonte.open(DataSpec.Builder()
            .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
            .setPosition(1000L)
            .setLength(janela)
            .build())
        assertEquals(janela, len)
        val buf = ByteArray(1 shl 16)
        var lidos = 0L
        while (true) {
            val n = fonte.read(buf, 0, buf.size)
            if (n == C.RESULT_END_OF_INPUT) break
            lidos += n
        }
        fonte.close()
        assertEquals(janela, lidos)
    }

    @Test fun buracoGrandeNaoBaixaTudoNaSonda() {
        // Seek para tras sobre 200 MB sem cache: a sonda fica em PRIMEIRO, o
        // resto vai pelo pool, nunca um ByteArray de 200 MB na thread de quem abriu.
        val conexoes = java.util.Collections.synchronizedList(ArrayList<ConexaoFaixa>())
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            ConexaoFaixa(url, 24891962527L).also { conexoes += it }
        }, dormir = { throw AssertionError("conexao viva nao deve esperar retry") })
        try {
            val len = fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
                .setPosition(0L)
                .setLength(200L shl 20)
                .build())
            assertEquals(200L shl 20, len)
            assertEquals("bytes=0-${(1 shl 20) - 1}", conexoes[0].getRequestProperty("Range"))
        } finally {
            fonte.close()
        }
    }

    @Test fun aberturaComumUsaParalelo() {
        // Abertura progressiva comum (length UNSET, arquivo de 24 GB): tem que
        // criar o pool. A janela e o resto do arquivo, nao o pedaco de sonda.
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            ConexaoFaixa(url, 24891962527L)
        }, dormir = { throw AssertionError("conexao viva nao deve esperar retry") })
        val exec = ParaleloDataSource::class.java.getDeclaredField("exec")
        exec.isAccessible = true
        var pool: java.util.concurrent.ExecutorService? = null
        try {
            val len = fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
                .setPosition(0L)
                .setLength(C.LENGTH_UNSET.toLong())
                .build())
            assertEquals(24891962527L, len)
            pool = exec.get(fonte) as java.util.concurrent.ExecutorService
            assertTrue("abertura comum deve criar o pool", pool != null)
            val buf = ByteArray(16)
            assertEquals(16, fonte.read(buf, 0, 16))
        } finally {
            fonte.close()
            // Os fios do pool contam +1/-1 em Estat.conexoes: sem espera-los,
            // o saldo vaza para o assert do teste seguinte.
            pool?.let { assertTrue(it.awaitTermination(10, TimeUnit.SECONDS)) }
            assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
        }
    }

    @Test fun fimDoArquivoDivideOIndiceEntreAsConexoes() {
        // Ant-Man (Shield, 16/10/2026): o Media3 abriu os 4 002 478 bytes finais
        // de um arquivo de 50 881 735 023 para ler o indice (Cues). Pela conexao
        // unica, 24 s. Agora: sonda de 1 MB + o resto em pedacos pelo pool, ao
        // mesmo tempo, e os bytes chegam inteiros e em ordem.
        val total = 50881735023L
        val pos = 50877732545L
        val conexoes = java.util.Collections.synchronizedList(ArrayList<ConexaoFaixa>())
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            object : ConexaoFaixa(url, total) {
                // Cada byte carrega o proprio offset: a ordem e conferivel.
                override fun getInputStream(): InputStream {
                    val r = getRequestProperty("Range")!!.removePrefix("bytes=")
                    val ini = r.substringBefore('-').toLong()
                    val fim = r.substringAfter('-').let { if (it.isEmpty()) total - 1 else it.toLong() }
                    return ByteArrayInputStream(ByteArray((fim - ini + 1).toInt()) { ((ini + it) % 251).toByte() })
                }
            }.also { conexoes += it }
        }, dormir = { throw AssertionError("conexao viva nao deve esperar retry") })
        val exec = ParaleloDataSource::class.java.getDeclaredField("exec")
        exec.isAccessible = true
        var pool: java.util.concurrent.ExecutorService? = null
        try {
            val len = fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
                .setPosition(pos)
                .setLength(C.LENGTH_UNSET.toLong())
                .build())
            assertEquals(total - pos, len)
            pool = exec.get(fonte) as java.util.concurrent.ExecutorService?
            assertTrue("o fim do arquivo deve usar o pool", pool != null)
            val buf = ByteArray(1 shl 16)
            var lidos = 0L
            while (true) {
                val n = fonte.read(buf, 0, buf.size)
                if (n == C.RESULT_END_OF_INPUT) break
                for (i in 0 until n) assertEquals(((pos + lidos + i) % 251).toByte(), buf[i])
                lidos += n
            }
            assertEquals(total - pos, lidos)
            val faixas = conexoes.map { it.getRequestProperty("Range") }
            assertEquals("bytes=$pos-${pos + (1 shl 20) - 1}", faixas[0])
            // A sonda fica com 1/4 da janela; o resto vai em 3 pedacos contiguos
            // ate o ultimo byte (a ordem de criacao depende dos fios).
            assertEquals(4, faixas.size)
            val pedacos = faixas.drop(1).map { f ->
                val r = f!!.removePrefix("bytes=")
                r.substringBefore('-').toLong() to r.substringAfter('-').toLong()
            }.sortedBy { it.first }
            assertEquals(pos + (total - pos + 3) / 4, pedacos.first().first)
            assertEquals(total - 1, pedacos.last().second)
            for (i in 1 until pedacos.size) assertEquals(pedacos[i - 1].second + 1, pedacos[i].first)
        } finally {
            fonte.close()
            pool?.let { assertTrue(it.awaitTermination(10, TimeUnit.SECONDS)) }
            assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
        }
    }

    // Le a janela inteira e devolve os Ranges pedidos.
    private fun lerFim(total: Long, janela: Long): Pair<List<String>, Boolean> {
        val conexoes = java.util.Collections.synchronizedList(ArrayList<ConexaoFaixa>())
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            ConexaoFaixa(url, total).also { conexoes += it }
        }, dormir = { throw AssertionError("conexao viva nao deve esperar retry") })
        val exec = ParaleloDataSource::class.java.getDeclaredField("exec")
        exec.isAccessible = true
        var pool: java.util.concurrent.ExecutorService? = null
        try {
            val len = fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
                .setPosition(total - janela)
                .setLength(C.LENGTH_UNSET.toLong())
                .build())
            assertEquals(janela, len)
            pool = exec.get(fonte) as java.util.concurrent.ExecutorService?
            val buf = ByteArray(1 shl 16)
            var lidos = 0L
            while (true) {
                val n = fonte.read(buf, 0, buf.size)
                if (n == C.RESULT_END_OF_INPUT) break
                lidos += n
            }
            assertEquals(janela, lidos)
        } finally {
            fonte.close()
            pool?.let { assertTrue(it.awaitTermination(10, TimeUnit.SECONDS)) }
        }
        return conexoes.map { it.getRequestProperty("Range")!! } to (pool != null)
    }

    @Test fun indiceMenorQueASondaTambemSeDivide() {
        // Outro remux (Shield, 17/10/2026): 612 132 bytes finais, 5,3 s pela
        // conexao unica. A sonda ja cobria a janela; agora fica com 1/4 dela.
        val (faixas, pool) = lerFim(54212259960L, 612132L)
        assertTrue(pool)
        assertEquals(4, faixas.size)
    }

    @Test fun fimDoArquivoMinusculoSegueNaConexaoUnica() {
        // 200 KB finais: menos de dois pedacos minimos, nada de pool.
        val (faixas, pool) = lerFim(24891962527L, 200L shl 10)
        assertFalse(pool)
        assertEquals(2, faixas.size)   // sonda + reabertura da conexao unica
    }

    // Link do addon que redireciona (307) para o CDN do debrid; o host em
    // `vencido` responde 403 (link vencido).
    private class Rede(val total: Long) {
        val noAddon = AtomicInteger()
        val noCdn = AtomicInteger()
        @Volatile var cdn = "https://cdn1.invalid/f?token=a"
        @Volatile var vencido = ""
        // Host que responde 200 com um "clipe de erro" de 5000 bytes.
        @Volatile var clipe = ""
        // O addon fora do ar (o link reserva falha).
        @Volatile var addonCai = false
        val doAddon = java.util.concurrent.CopyOnWriteArrayList<HttpURLConnection>()
        val porHost = java.util.concurrent.ConcurrentHashMap<String, AtomicInteger>()
        fun conexao(url: URL): HttpURLConnection =
            if (url.host == "addon.invalid") {
                noAddon.incrementAndGet()
                val destino = cdn
                val cai = addonCai
                object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int = if (cai) throw SocketTimeoutException("lento") else 307
                    override fun getHeaderField(name: String?): String? =
                        if (name == "Location") destino else null
                }.also { doAddon.add(it) }
            } else {
                noCdn.incrementAndGet()
                porHost.getOrPut(url.host) { AtomicInteger() }.incrementAndGet()
                val recusa = url.host == vencido
                if (url.host == clipe) object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int = HTTP_OK
                    override fun getContentLengthLong(): Long = 5000
                    override fun getInputStream(): InputStream = ByteArrayInputStream(ByteArray(5000))
                    override fun getHeaderField(name: String?): String? = null
                } else object : ConexaoFaixa(url, total) {
                    override fun getResponseCode(): Int = if (recusa) HTTP_FORBIDDEN else super.getResponseCode()
                }
            }
    }

    private fun abrirDois(fonte: ParaleloDataSource, pos: Long) {
        try {
            fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://addon.invalid/play/abc"))
                .setPosition(pos).setLength(2L).build())
        } finally { fonte.close() }
    }

    @Test fun aberturaSeguinteVaiDiretoAoDestinoDoAddon() {
        val rede = Rede(24891962527L)
        val destinos = DestinosAddon(agora = { 1000L })
        val nova = { ParaleloDataSource(null, emptyMap(), destinos = destinos, novaConexao = rede::conexao,
            dormir = { throw AssertionError("nao deve esperar retry") }) }
        abrirDois(nova(), 0L)
        assertEquals(1, rede.noAddon.get())
        // Indice no fim, volta ao inicio, seek: nenhuma passa de novo pelo addon.
        abrirDois(nova(), 24891962000L)
        abrirDois(nova(), 5000L)
        assertEquals(1, rede.noAddon.get())
        assertEquals(3, rede.noCdn.get())
    }

    @Test fun destinoRecusadoVoltaAoAddonEGuardaONovo() {
        val rede = Rede(24891962527L)
        val destinos = DestinosAddon(agora = { 1000L })
        val nova = { ParaleloDataSource(null, emptyMap(), destinos = destinos, novaConexao = rede::conexao,
            dormir = { throw AssertionError("nao deve esperar retry") }) }
        abrirDois(nova(), 0L)
        // O link do CDN venceu: 403 no destino guardado, o addon da um novo.
        rede.vencido = "cdn1.invalid"
        rede.cdn = "https://cdn2.invalid/f?token=b"
        val cdnAntes = rede.noCdn.get()
        abrirDois(nova(), 7000L)
        assertEquals(2, rede.noAddon.get())
        assertEquals(cdnAntes + 2, rede.noCdn.get())   // cdn1 recusou, cdn2 serviu
        assertEquals("cdn2.invalid", destinos.buscar("https://addon.invalid/play/abc")?.host)
        // E a abertura seguinte ja vai direto ao cdn2.
        abrirDois(nova(), 9000L)
        assertEquals(2, rede.noAddon.get())
    }

    @Test fun destinoQueServeOutroArquivoVoltaAoAddon() {
        // O CDN vencido responde 200 com um clipe de erro de 5000 bytes em vez
        // de 403: o tamanho nao bate com o do arquivo, entao nao e ele.
        val rede = Rede(24891962527L)
        val destinos = DestinosAddon(agora = { 1000L })
        val nova = { ParaleloDataSource(null, emptyMap(), destinos = destinos, novaConexao = rede::conexao,
            dormir = { throw AssertionError("nao deve esperar retry") }) }
        abrirDois(nova(), 0L)
        assertEquals(24891962527L, destinos.tamanhoDe("https://addon.invalid/play/abc"))
        rede.clipe = "cdn1.invalid"
        rede.cdn = "https://cdn2.invalid/f?token=b"
        val fonte = nova()
        val len = try {
            fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://addon.invalid/play/abc"))
                .setPosition(7000L).setLength(2L).build())
        } finally { fonte.close() }
        assertEquals(2L, len)
        assertEquals(2, rede.noAddon.get())
        assertEquals("cdn2.invalid", destinos.buscar("https://addon.invalid/play/abc")?.host)
    }

    private val chave = "https://addon.invalid/play/abc"
    private val minuto = 60L * 1000

    private fun comReserva(rede: Rede, destinos: DestinosAddon) = {
        ParaleloDataSource(null, emptyMap(), destinos = destinos, novaConexao = rede::conexao,
            dormir = { throw AssertionError("nao deve esperar retry") }, fundo = { it.run() })
    }

    // Comentario do #572: com 2,5 h o link reserva e pedido em segundo plano,
    // e so entra numa pausa/seek feitos 10 min ou mais depois de chegar.
    @Test fun linkReservaSoEntraNumaPausaOuSeekDepoisDaCarencia() {
        DestinosAddon.pausaOuBusca(Long.MIN_VALUE)
        val rede = Rede(24891962527L)
        var agora = 0L
        val destinos = DestinosAddon(agora = { agora })
        val nova = comReserva(rede, destinos)
        abrirDois(nova(), 0L)
        // Um seek 2 h depois: direto ao cdn1, nada pedido ao addon.
        agora = 120 * minuto
        DestinosAddon.pausaOuBusca(agora)
        abrirDois(nova(), 0L)
        assertEquals(1, rede.noAddon.get())
        // 2,5 h: a abertura segue no cdn1 e o link reserva vem do addon.
        rede.cdn = "https://cdn2.invalid/f?token=b"
        agora = DestinosAddon.VALIDADE_MS
        abrirDois(nova(), 0L)
        assertEquals(2, rede.noAddon.get())
        assertEquals("cdn1.invalid", destinos.buscar(chave)?.host)
        // Ate 20 s de espera pelo addon, na conexao e na leitura.
        for (c in rede.doAddon) {
            assertEquals(ParaleloDataSource.ESPERA_FONTE_MS, c.connectTimeout)
            assertEquals(ParaleloDataSource.ESPERA_FONTE_MS, c.readTimeout)
        }
        // Seek 5 min depois do link reserva: ainda o cdn1.
        agora += 5 * minuto
        DestinosAddon.pausaOuBusca(agora)
        abrirDois(nova(), 0L)
        assertEquals(1, rede.porHost["cdn2.invalid"]?.get())   // so a conferencia da reserva
        // Passada a carencia, sem pausa nem seek novo: ainda o cdn1.
        agora = DestinosAddon.VALIDADE_MS + 11 * minuto
        assertEquals("cdn1.invalid", destinos.buscar(chave)?.host)
        // A pausa depois da carencia: o cdn2 entra, sem perguntar ao addon.
        DestinosAddon.pausaOuBusca(agora)
        assertEquals("cdn2.invalid", destinos.buscar(chave)?.host)
        abrirDois(nova(), 0L)
        assertEquals(2, rede.porHost["cdn2.invalid"]?.get())
        assertEquals(2, rede.noAddon.get())
    }

    // Reproducao em curso: a pausa depois da carencia troca os pedacos
    // seguintes para o link reserva, na mesma abertura e sem erro.
    @Test fun pausaDepoisDaCarenciaTrocaOsPedacosSeguintes() {
        DestinosAddon.pausaOuBusca(Long.MIN_VALUE)
        val total = 8L shl 20
        val porHost = java.util.concurrent.ConcurrentHashMap<String, AtomicInteger>()
        val cdn = AtomicReference("https://cdn1.invalid/f?token=a")
        var agora = 0L
        val destinos = DestinosAddon(agora = { agora })
        val conexao: (URL) -> HttpURLConnection = { url ->
            porHost.getOrPut(url.host) { AtomicInteger() }.incrementAndGet()
            if (url.host == "addon.invalid") {
                val d = cdn.get()
                object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int = 307
                    override fun getHeaderField(name: String?): String? = if (name == "Location") d else null
                }
            } else ConexaoPadrao(url, total)
        }
        val nova = { ParaleloDataSource(null, emptyMap(), conexoes = 1, pedaco = 256 shl 10,
            destinos = destinos, novaConexao = conexao, dormir = { }, fundo = { it.run() }) }
        abrirDois(nova(), 0L)
        cdn.set("https://cdn2.invalid/f?token=b")
        agora = DestinosAddon.VALIDADE_MS
        abrirDois(nova(), 0L)                                    // pede a reserva
        agora += 11 * minuto
        val fonte = nova()
        val buf = ByteArray(1 shl 16)
        var lidos = 0L
        var pausou = false
        try {
            assertEquals(total, fonte.open(DataSpec.Builder().setUri(android.net.Uri.parse(chave)).build()))
            while (true) {
                if (!pausou && lidos >= total / 2) { DestinosAddon.pausaOuBusca(agora); pausou = true }
                val n = fonte.read(buf, 0, buf.size)
                if (n == C.RESULT_END_OF_INPUT) break
                for (i in 0 until n) assertEquals(((lidos + i) % 251).toByte(), buf[i])
                lidos += n
            }
        } finally { fonte.close() }
        assertEquals(total, lidos)
        assertEquals(2, porHost["addon.invalid"]?.get())
        // A conferencia da reserva e pedacos da segunda metade.
        assertTrue((porHost["cdn2.invalid"]?.get() ?: 0) > 1)
    }

    // O cdn1 venceu antes da carencia: a reserva entra no lugar dele, sem
    // pedir outro link ao addon.
    @Test fun destinoRecusadoUsaOLinkReservaSemPerguntarAoAddon() {
        DestinosAddon.pausaOuBusca(Long.MIN_VALUE)
        val rede = Rede(24891962527L)
        var agora = 0L
        val destinos = DestinosAddon(agora = { agora })
        val nova = comReserva(rede, destinos)
        abrirDois(nova(), 0L)
        rede.cdn = "https://cdn2.invalid/f?token=b"
        agora = DestinosAddon.VALIDADE_MS
        abrirDois(nova(), 0L)
        assertEquals(2, rede.noAddon.get())
        agora += minuto
        rede.vencido = "cdn1.invalid"
        abrirDois(nova(), 7000L)
        assertEquals(2, rede.noAddon.get())
        assertEquals("cdn2.invalid", destinos.buscar(chave)?.host)
    }

    // A fonte nao deu o link reserva (lenta, fora do ar): a reproducao segue
    // no cdn1 e o pedido so se repete 1 min depois.
    @Test fun linkReservaQueFalhaNaoAtrapalhaETentaDeNovo() {
        DestinosAddon.pausaOuBusca(Long.MIN_VALUE)
        val rede = Rede(24891962527L)
        var agora = 0L
        val destinos = DestinosAddon(agora = { agora })
        val nova = comReserva(rede, destinos)
        abrirDois(nova(), 0L)
        rede.addonCai = true
        agora = DestinosAddon.VALIDADE_MS
        abrirDois(nova(), 0L)
        assertEquals(2, rede.noAddon.get())
        assertEquals("cdn1.invalid", destinos.buscar(chave)?.host)
        agora += 30 * 1000
        abrirDois(nova(), 0L)
        assertEquals(2, rede.noAddon.get())
        rede.addonCai = false
        rede.cdn = "https://cdn2.invalid/f?token=b"
        agora += 31 * 1000
        abrirDois(nova(), 0L)
        assertEquals(3, rede.noAddon.get())
        agora += 10 * minuto
        DestinosAddon.pausaOuBusca(agora)
        assertEquals("cdn2.invalid", destinos.buscar(chave)?.host)
    }

    // 206 com bytes conferiveis ((pos % 251)) e o total que o teste escolhe.
    private class ConexaoPadrao(url: URL, private val total: Long) : ConexaoFalsa(url) {
        private var range = "bytes=0-"
        override fun setRequestProperty(key: String?, value: String?) { if (key == "Range" && value != null) range = value }
        private fun ini() = range.removePrefix("bytes=").substringBefore('-').toLong()
        private fun fim() = range.substringAfter('-', "").let { if (it.isEmpty()) total - 1 else it.toLong() }
        override fun getResponseCode(): Int = HTTP_PARTIAL
        override fun getContentLengthLong(): Long = fim() - ini() + 1
        override fun getHeaderField(name: String?): String? =
            if (name == "Content-Range") "bytes ${ini()}-${fim()}/$total" else null
        override fun getInputStream(): InputStream {
            val i = ini()
            return ByteArrayInputStream(ByteArray((fim() - i + 1).toInt()) { ((i + it) % 251).toByte() })
        }
    }

    private fun lerTudoDe(fonte: ParaleloDataSource, total: Long) {
        val buf = ByteArray(1 shl 16)
        var lidos = 0L
        while (true) {
            val n = fonte.read(buf, 0, buf.size)
            if (n == C.RESULT_END_OF_INPUT) break
            for (i in 0 until n) assertEquals(((lidos + i) % 251).toByte(), buf[i])
            lidos += n
        }
        assertEquals(total, lidos)
    }

    // Reproducao continua (ou retomada apos pausa) com o link do CDN vencendo
    // no meio: os pedacos recusados pedem UM destino novo ao addon e a mesma
    // abertura segue, sem erro e sem gastar as tentativas.
    @Test fun linkVencidoNoMeioDaAberturaTrocaDeDestinoSemCair() {
        val total = 40L shl 20
        val noAddon = AtomicInteger()
        val noCdn1 = AtomicInteger()
        val cdn = AtomicReference("https://cdn1.invalid/f?token=a")
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            when (url.host) {
                "addon.invalid" -> {
                    noAddon.incrementAndGet()
                    val d = cdn.get()
                    object : ConexaoFalsa(url) {
                        override fun getResponseCode(): Int = 307
                        override fun getHeaderField(name: String?): String? = if (name == "Location") d else null
                    }
                }
                // A sonda e dois pedacos passam; dai em diante o link venceu.
                "cdn1.invalid" -> if (noCdn1.incrementAndGet() <= 3) ConexaoPadrao(url, total) else {
                    cdn.set("https://cdn2.invalid/f?token=b")
                    object : ConexaoFalsa(url) { override fun getResponseCode(): Int = HTTP_FORBIDDEN }
                }
                else -> ConexaoPadrao(url, total)
            }
        }, dormir = { throw AssertionError("a troca de destino nao deve esperar retry") })
        try {
            assertEquals(total, fonte.open(DataSpec.Builder()
                .setUri(android.net.Uri.parse("https://addon.invalid/play/abc")).build()))
            lerTudoDe(fonte, total)
        } finally { fonte.close() }
        assertEquals(2, noAddon.get())   // a abertura e UMA renovacao
    }

    // O addon devolve outro arquivo (outro total): o destino nao e trocado e
    // o pedaco falha como antes, pelas tentativas.
    @Test fun renovacaoQueTrazOutroArquivoNaoTrocaODestino() {
        val total = 40L shl 20
        val noCdn1 = AtomicInteger()
        val cdn = AtomicReference("https://cdn1.invalid/f?token=a")
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            when (url.host) {
                "addon.invalid" -> {
                    val d = cdn.get()
                    object : ConexaoFalsa(url) {
                        override fun getResponseCode(): Int = 307
                        override fun getHeaderField(name: String?): String? = if (name == "Location") d else null
                    }
                }
                "cdn1.invalid" -> if (noCdn1.incrementAndGet() <= 3) ConexaoPadrao(url, total) else {
                    cdn.set("https://outro.invalid/g")
                    object : ConexaoFalsa(url) { override fun getResponseCode(): Int = HTTP_FORBIDDEN }
                }
                else -> ConexaoPadrao(url, total + 1)
            }
        }, dormir = { })
        var falhou = false
        try {
            fonte.open(DataSpec.Builder().setUri(android.net.Uri.parse("https://addon.invalid/play/abc")).build())
            try { lerTudoDe(fonte, total) } catch (e: IOException) { falhou = true }
        } finally { fonte.close() }
        assertTrue(falhou)
    }

    @Test fun semRedirecionamentoNadaEGuardado() {
        val destinos = DestinosAddon(agora = { 0L })
        destinos.lembrar("https://cdn.invalid/f", URL("https://cdn.invalid/f"))
        assertEquals(null, destinos.buscar("https://cdn.invalid/f"))
    }

    @Test fun janelaVaziaAbreSemConectar() {
        // DataSpec de 0 byte: abre vazio, sem Range invalido (pos-1) nem 416.
        val aberturas = AtomicInteger()
        val fonte = ParaleloDataSource(null, emptyMap(), novaConexao = { url ->
            aberturas.incrementAndGet()
            ConexaoFaixa(url, 24891962527L)
        }, dormir = { throw AssertionError("janela vazia nao deve esperar retry") })
        val len = fonte.open(DataSpec.Builder()
            .setUri(android.net.Uri.parse("https://example.invalid/video.mkv"))
            .setPosition(613230448L)
            .setLength(1L)
            .build().also { d ->
                // O Builder do media3 recusa length=0, mas o campo e public: abrir()
                // tem que abrir vazio em vez de pedir o Range invalido (pos-1).
                val campo = DataSpec::class.java.getDeclaredField("length")
                campo.isAccessible = true
                campo.set(d, 0L)
            })
        assertEquals(0L, len)
        assertEquals(0, aberturas.get())
        assertEquals(C.RESULT_END_OF_INPUT.toLong(), fonte.read(ByteArray(2), 0, 2).toLong())
        fonte.close()
    }

    @Test fun aberturaInicialRepeteSemEsperarNaPrimeiraFalha() {
        val aberturas = AtomicInteger()
        val esperas = ArrayList<Long>()
        val fonte = ParaleloDataSource(null, emptyMap(),
            novaConexao = { url ->
                val tentativa = aberturas.incrementAndGet()
                if (tentativa == 1) object : ConexaoFalsa(url) {
                    override fun getResponseCode(): Int = throw IOException("sem rota")
                } else ConexaoFalsa(url)
            },
            dormir = { esperas += it })
        fonte.conectarComRetry(URL("https://example.invalid/video.mkv"), 0, 1023, 0L)
        fonte.close()
        assertEquals(2, aberturas.get())
        assertEquals(emptyList<Long>(), esperas)
    }

    @Test fun closeDesconectaAberturaEmVoo() {
        val entrou = CountDownLatch(1)
        val saiu = CountDownLatch(1)
        val conexao = object : ConexaoFalsa(URL("https://example.invalid/video.mkv")) {
            override fun getResponseCode(): Int {
                entrou.countDown()
                while (!desconectou.get()) Thread.sleep(5)
                throw IOException("desconectada")
            }
        }
        val aberturas = AtomicInteger()
        val fonte = ParaleloDataSource(null, emptyMap(),
            novaConexao = { aberturas.incrementAndGet(); conexao },
            dormir = { throw AssertionError("cancelamento nao deve repetir") })
        val erro = AtomicReference<IOException?>()
        val t = Thread {
            try { fonte.conectarComRetry(conexao.url, 0, 1023) }
            catch (e: IOException) { erro.set(e) }
            finally { saiu.countDown() }
        }
        t.start()
        assertTrue(entrou.await(2, TimeUnit.SECONDS))

        fonte.close()

        assertTrue(saiu.await(2, TimeUnit.SECONDS))
        assertTrue(conexao.desconectou.get())
        assertEquals(1, aberturas.get())
        assertTrue("close deve sair como InterruptedIOException, recebeu ${erro.get()}", erro.get() is InterruptedIOException)
        // O finally libera `saiu` imediatamente antes de Thread.run retornar.
        t.join(200)
        assertFalse(t.isAlive)
    }
}
