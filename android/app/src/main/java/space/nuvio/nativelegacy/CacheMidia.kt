package space.nuvio.nativelegacy

import android.util.Log
import androidx.media3.common.util.UnstableApi
import androidx.media3.datasource.DataSink
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.cache.CacheDataSink
import androidx.media3.datasource.cache.CacheDataSource
import androidx.media3.datasource.cache.CacheKeyFactory
import androidx.media3.datasource.cache.LeastRecentlyUsedCacheEvictor
import androidx.media3.datasource.cache.SimpleCache
import java.io.File
import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

// F07 (1.8): SEEK CACHE ON DISK FOR ONE PLAYER SESSION (Media3 side).
//
// CacheDataSource sits ABOVE the existing chain (NvPlayer: DefaultDataSource ->
// here -> ParaleloDataSource.Factory with the auth headers, Range and the
// parallel downloader). Spans already downloaded are read back from disk;
// gaps go upstream with the same DataSpec (position/length = Range), so
// headers, redirects and the 4 connections are untouched. Cache hits never
// reach ParaleloDataSource, so the StreamFit passive meter keeps counting
// socket bytes only.
//
// One SimpleCache per session in its own folder (CacheSessao), no database
// (the index lives in the folder and dies with it), LRU-bounded to the
// effective limit. Writes go through CacheSessao.Escrita: ENOSPC or any other
// write error turns the cache off for the session, playback continues.
@androidx.annotation.OptIn(UnstableApi::class)
class CacheMidia private constructor(
    private val pasta: File,
    private val cache: SimpleCache,
    private val sessao: String,
    val pedidoMb: Int,
    val limiteMb: Int,
    val estado: CacheSessao.Estado,
) {
    companion object {
        private const val TAG = "nuvio-cache"
        private const val ESCRITA_BUF = 128 * 1024
        // Canceled loaders unwind (interrupt -> close) before the cache goes.
        private const val ESPERA_LIBERAR_MS = 1500L
        private val boot = "b" + java.lang.Long.toString(System.currentTimeMillis(), 36)
        private var contador = 0
        private val fio: ScheduledExecutorService by lazy {
            Executors.newSingleThreadScheduledExecutor { r -> Thread(r, "nv-cache").apply { isDaemon = true } }
        }

        fun raiz(dirCache: File) = File(dirCache, "nv-seek")

        // WARM RESUME (#492): the kept cache of the last title (CacheSessao.Vaga).
        private const val VIGIA_MS = 5L * 60L * 1000L
        private var raizVaga: File? = null
        private val vaga = CacheSessao.Vaga<CacheMidia> { it.liberar() }

        // Player closed: keep this session's cache for `titulo` instead of
        // deleting it (when it is healthy, has bytes and the disk has room).
        fun guardar(c: CacheMidia, titulo: String, chave: String) {
            val sadio = !c.estado.desligado && c.cache.cacheSpace > 0
            val livre = try { c.pasta.usableSpace } catch (e: SecurityException) { 0L }
            val ficou = vaga.guardar(c, if (sadio) titulo else "", chave, c.pedidoMb, System.currentTimeMillis(), livre)
            if (ficou) { raizVaga = c.pasta.parentFile?.parentFile; Log.i(TAG, "kept for the next open: $titulo (${c.usadoMb()} MB)"); vigiar() }
        }

        // A VOD opens: the kept cache, rebound to this session, or null.
        fun pegar(titulo: String, chave: String, pedidoMb: Int, aoFalhar: (Boolean) -> Unit): CacheMidia? {
            val livre = try { raizVaga?.usableSpace ?: 0L } catch (_: SecurityException) { 0L }
            vaga.vencer(System.currentTimeMillis(), livre)
            val c = vaga.pegar(titulo, chave, pedidoMb, System.currentTimeMillis()) ?: return null
            c.estado.aoFalhar = aoFalhar
            Log.i(TAG, "reusing the kept cache: $titulo (${c.usadoMb()} MB)")
            return c
        }

        // Another title, setting off, profile switch.
        fun descartar(motivo: String) { if (vaga.largar()) Log.i(TAG, "kept cache dropped: $motivo") }

        private fun vigiar() {
            fio.schedule({
                val livre = try { raizVaga?.usableSpace ?: 0L } catch (e: SecurityException) { 0L }
                if (vaga.vencer(System.currentTimeMillis(), livre)) Log.i(TAG, "kept cache dropped: time or space")
                else if (vaga.titulo() != null) vigiar()
            }, VIGIA_MS, TimeUnit.MILLISECONDS)
        }

        // App start: crash leftovers of earlier processes, off the main thread.
        fun limparTudo(dirCache: File) {
            fio.schedule({ raiz(dirCache).deleteRecursively() },
                ESPERA_LIBERAR_MS + 100, TimeUnit.MILLISECONDS)
        }

        fun limparSobras(dirCache: File) {
            fio.execute {
                val n = CacheSessao.limparSobras(raiz(dirCache), boot)
                if (n > 0) Log.i(TAG, "removed $n leftover cache folder(s)")
            }
        }

        // Session cache or the reason there is none: (cache, state).
        fun criar(dirCache: File, pedidoMb: Int, aoFalhar: (Boolean) -> Unit): Pair<CacheMidia?, Int> {
            if (pedidoMb <= 0) return null to CacheSessao.DESLIGADO
            val r = raiz(dirCache)
            r.mkdirs()
            val livre = try { r.usableSpace } catch (e: SecurityException) { 0L }
            val limite = CacheSessao.limiteMb(pedidoMb, livre)
            if (limite <= 0) {
                Log.i(TAG, "off: ${livre shr 20} MB free for $pedidoMb MB + reserve")
                return null to CacheSessao.POUCO_ESPACO
            }
            val sessao = "s" + (++contador)
            val pasta = CacheSessao.pastaSessao(r, boot, sessao)
            return try {
                CacheSessao.apagar(pasta)
                pasta.mkdirs()
                @Suppress("DEPRECATION")
                val c = SimpleCache(pasta, LeastRecentlyUsedCacheEvictor(limite.toLong() shl 20))
                Log.i(TAG, "on: $limite MB (asked $pedidoMb, ${livre shr 20} MB free)")
                CacheMidia(pasta, c, sessao, pedidoMb, limite, CacheSessao.Estado(aoFalhar)) to CacheSessao.ATIVO
            } catch (e: Exception) {
                Log.w(TAG, "cache init failed: $e")
                fio.execute { CacheSessao.apagar(pasta) }
                null to CacheSessao.FALHOU
            }
        }
    }

    private class SinkProtegido(cache: SimpleCache, estado: CacheSessao.Estado) : DataSink {
        // bufferSize 0: Escrita buffers (see CacheSessao.Escrita).
        private val interno = CacheDataSink(cache, CacheDataSink.DEFAULT_FRAGMENT_SIZE, 0)
        private var spec: DataSpec? = null
        private val escrita = CacheSessao.Escrita(estado, ESCRITA_BUF,
            { interno.open(spec!!) }, { b, o, n -> interno.write(b, o, n) }, { interno.close() })
        override fun open(dataSpec: DataSpec) { spec = dataSpec; escrita.abrir() }
        override fun write(buffer: ByteArray, offset: Int, length: Int) = escrita.escrever(buffer, offset, length)
        override fun close() { escrita.fechar(); spec = null }
    }

    fun fabrica(upstream: DataSource.Factory): DataSource.Factory =
        CacheDataSource.Factory()
            .setCache(cache)
            .setUpstreamDataSourceFactory(upstream)
            .setCacheWriteDataSinkFactory { SinkProtegido(cache, estado) }
            .setCacheKeyFactory(CacheKeyFactory { spec -> CacheSessao.chave(sessao, spec.key ?: spec.uri.toString()) })
            // A cache read error bypasses the cache for that load instead of
            // failing playback.
            .setFlags(CacheDataSource.FLAG_IGNORE_CACHE_ON_ERROR)

    fun usadoMb(): Int = try { (cache.cacheSpace shr 20).toInt() } catch (e: Exception) { 0 }

    // Player closed / source changed: release and delete the folder, later and
    // off the main thread.
    fun liberar() {
        fio.schedule({
            try { cache.release() } catch (e: Exception) { Log.w(TAG, "release: $e") }
            CacheSessao.apagar(pasta)
        }, ESPERA_LIBERAR_MS, TimeUnit.MILLISECONDS)
    }
}
