package space.nuvio.nativelegacy

import java.io.File
import java.io.IOException

// F07 (1.8): the PURE part of the seek cache — sizes, folders, keys, cleanup
// and the write guard. No Android or Media3 import, so the JVM test
// (tests/cacheboost_android.py) runs this exact file. The Media3 side
// (SimpleCache + CacheDataSource + CacheDataSink) is CacheMidia.kt.
//
// Layout on disk: <app cache dir>/nv-seek/<boot>/<session>/. `boot` is one
// per process: at start every OTHER boot folder is a crash leftover and goes
// away, without racing the sessions this process creates meanwhile. Each
// player open gets its own session folder, deleted when the player closes.
object CacheSessao {
    // States reported to C (src/cacheboost.h, CB_CACHE_*): keep in sync.
    const val DESLIGADO = 0
    const val ATIVO = 1
    const val POUCO_ESPACO = 2
    const val DISCO_CHEIO = 3
    const val NAO_SE_APLICA = 4
    const val FALHOU = 5

    // Free space the cache never takes: the system, the app's own data and
    // other apps need room too. A 256 MB cache needs 768 MB free.
    const val RESERVA_BYTES = 512L shl 20
    private val OPCOES_MB = intArrayOf(1024, 512, 256)

    // The largest offered size <= `pedidoMb` that leaves RESERVA_BYTES free
    // (statvfs of the cache dir: File.usableSpace). Unknown/failed free space
    // (<= 0) means OFF: never guess that a disk has room.
    fun limiteMb(pedidoMb: Int, livreBytes: Long): Int {
        if (pedidoMb <= 0 || livreBytes <= 0L) return 0
        for (mb in OPCOES_MB) {
            if (mb > pedidoMb) continue
            if ((mb.toLong() shl 20) + RESERVA_BYTES <= livreBytes) return mb
        }
        return 0
    }

    // Cache key: the session first, so two sessions can never share a span even
    // if a folder survived; the URI without fragment (Media3's own default key).
    fun chave(sessao: String, uri: String): String {
        val i = uri.indexOf('#')
        return sessao + "|" + (if (i >= 0) uri.substring(0, i) else uri)
    }

    // Only names this file creates: [a-z0-9-], never "..", never a separator.
    fun nomeValido(nome: String): Boolean =
        nome.isNotEmpty() && nome.length <= 64 && nome.all { it in 'a'..'z' || it in '0'..'9' || it == '-' }

    fun pastaSessao(raiz: File, boot: String, sessao: String): File {
        require(nomeValido(boot) && nomeValido(sessao)) { "nome de pasta invalido" }
        return File(File(raiz, boot), sessao)
    }

    // App start: removes every boot folder except the current one. Returns
    // how many were removed (log). Missing root = nothing to do.
    fun limparSobras(raiz: File, bootAtual: String): Int {
        val filhos = raiz.listFiles() ?: return 0
        var n = 0
        for (f in filhos) {
            if (f.name == bootAtual) continue
            if (apagar(f)) n++
        }
        return n
    }

    // Recursive delete that never follows a symlink out of the folder.
    fun apagar(f: File): Boolean {
        if (!f.exists()) return false
        if (f.isDirectory && !ehLink(f)) f.listFiles()?.forEach { apagar(it) }
        return f.delete()
    }

    // A symlink's canonical path is not <canonical parent>/<name>. (The app
    // cache dir itself may sit behind a link, /data/user/0 -> /data/data, so
    // only the entry's own last hop is compared; Files.isSymbolicLink needs
    // API 26 and minSdk is 24.)
    private fun ehLink(f: File): Boolean {
        val pai = f.absoluteFile.parentFile ?: return false
        return try { f.canonicalFile != File(pai.canonicalFile, f.name) } catch (e: IOException) { true }
    }

    // WARM RESUME (#492): the cache of the LAST title survives the player. One
    // slot only; it goes away when another title opens, the setting turns off,
    // the profile changes, LIMITE_MS passes, or free space drops under the
    // reserve. Pure (no Android/Media3), so the JVM test drives it with a fake
    // cache and a fake clock; the caller owns what `T` is and how it dies.
    const val LIMITE_MS = 3L * 60L * 60L * 1000L   // same as the saved link (fontevolta)

    // Keeping needs the reserve still free: the kept bytes are already on disk,
    // so it is the free space that is left that matters.
    fun podeGuardar(livreBytes: Long): Boolean = livreBytes >= RESERVA_BYTES

    class Vaga<T : Any>(private val liberar: (T) -> Unit) {
        private class Item<T>(val c: T, val titulo: String, val chave: String, val pedidoMb: Int, val ate: Long)
        private var item: Item<T>? = null

        // Player closed: keep `c` for `titulo` (a non-empty id of the title/episode),
        // replacing any older one. Not kept (and released) without a title, or
        // without free space. Returns whether it was kept.
        @Synchronized fun guardar(c: T, titulo: String, chave: String, pedidoMb: Int, agora: Long, livreBytes: Long): Boolean {
            largar()
            if (titulo.isEmpty() || !podeGuardar(livreBytes)) { liberar(c); return false }
            item = Item(c, titulo, chave, pedidoMb, agora + LIMITE_MS)
            return true
        }

        // A VOD opens: the kept cache if it is the same title, source and limit
        // and still inside the time limit; otherwise it is released. An empty
        // title (the Home trailer) touches nothing.
        @Synchronized fun pegar(titulo: String, chave: String, pedidoMb: Int, agora: Long): T? {
            val i = item ?: return null
            if (titulo.isEmpty() && agora < i.ate) return null
            item = null
            if (agora < i.ate && i.titulo == titulo && i.chave == chave && i.pedidoMb == pedidoMb) return i.c
            liberar(i.c)
            return null
        }

        // Other title / setting off / profile switch.
        @Synchronized fun largar(): Boolean {
            val i = item ?: return false
            item = null
            liberar(i.c)
            return true
        }

        // Timer: drops it when the time limit passed or the disk got tight.
        @Synchronized fun vencer(agora: Long, livreBytes: Long): Boolean {
            val i = item ?: return false
            if (agora < i.ate && podeGuardar(livreBytes)) return false
            return largar()
        }

        @Synchronized fun titulo(): String? = item?.titulo
    }

    // ENOSPC as Android reports it: ErrnoException("write failed: ENOSPC (No
    // space left on device)") wrapped in IOException, sometimes twice.
    fun semEspaco(t: Throwable?): Boolean {
        var e = t
        var passos = 0
        while (e != null && passos++ < 8) {
            val m = e.message ?: ""
            if (m.contains("ENOSPC") || m.contains("No space left", ignoreCase = true)) return true
            e = e.cause
        }
        return false
    }

    // Shared by every sink of ONE session: the first write failure turns the
    // cache off for the rest of the session (reads of spans already written
    // continue; playback is never interrupted by the cache).
    class Estado(@Volatile var aoFalhar: (semEspaco: Boolean) -> Unit) {
        @Volatile var desligado = false
            private set
        fun falhou(e: Throwable) {
            synchronized(this) {
                if (desligado) return
                desligado = true
            }
            try { aoFalhar(semEspaco(e)) } catch (_: Throwable) { }
        }
    }

    // THE WRITE GUARD around one cache sink. The inner sink is Media3's
    // CacheDataSink built UNBUFFERED (bufferSize 0): this class buffers, so a
    // failed write never leaves bytes in a hidden buffer that a later flush
    // would write again at the wrong offset — the inner sink commits only the
    // bytes whose write returned. Any exception (ENOSPC, IO, a released cache)
    // turns the session's cache off and the data keeps flowing to the player.
    class Escrita(
        private val estado: Estado,
        tamanho: Int,
        private val abrirInterno: () -> Unit,
        private val escreverInterno: (ByteArray, Int, Int) -> Unit,
        private val fecharInterno: () -> Unit,
    ) {
        private val buf = ByteArray(tamanho.coerceAtLeast(4096))
        private var n = 0
        private var aberto = false
        private var quebrado = false

        fun abrir() {
            n = 0; quebrado = false; aberto = false
            if (estado.desligado) return
            try { abrirInterno(); aberto = true } catch (e: Exception) { estado.falhou(e) }
        }

        fun escrever(b: ByteArray, off: Int, len: Int) {
            if (!aberto || quebrado || estado.desligado || len <= 0) return
            var o = off
            var resta = len
            while (resta > 0) {
                val c = minOf(resta, buf.size - n)
                System.arraycopy(b, o, buf, n, c)
                n += c; o += c; resta -= c
                if (n == buf.size && !descarregar()) return
            }
        }

        private fun descarregar(): Boolean {
            if (n == 0) return true
            return try { escreverInterno(buf, 0, n); n = 0; true }
            catch (e: Exception) { quebrado = true; n = 0; estado.falhou(e); false }
        }

        fun fechar() {
            if (!aberto) { n = 0; return }
            aberto = false
            if (!quebrado && !estado.desligado) descarregar() else n = 0
            // Closing commits what the inner sink wrote successfully (also after
            // a failure: unbuffered, its count matches the file prefix).
            try { fecharInterno() } catch (e: Exception) { estado.falhou(e) }
        }
    }
}
