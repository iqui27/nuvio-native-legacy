package space.nuvio.nativelegacy

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
        val baixar = ParaleloDataSource::class.java.getDeclaredMethod("baixar",
            URL::class.java, Long::class.javaPrimitiveType, Long::class.javaPrimitiveType)
        baixar.isAccessible = true
        try {
            val dados = baixar.invoke(fonte, URL("https://example.invalid/video.mkv"), 0L, 3L) as ByteArray
            assertEquals(listOf<Byte>(1, 2, 3, 4), dados.toList())
        } finally { fonte.close() }
        assertEquals(3, aberturas.get())
        assertEquals(listOf(250L, 750L), esperas)
        assertTrue(conexoes.all { it.desconectou.get() })
        assertEquals(0, ParaleloDataSource.Estat.conexoes.get())
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
