package space.nuvio.nativelegacy

import android.graphics.Bitmap
import android.media.Image
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaDataSource
import android.media.MediaExtractor
import android.media.MediaFormat
import android.util.Log
import java.io.ByteArrayOutputStream
import java.nio.ByteBuffer
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.ConcurrentHashMap
import kotlin.math.pow

// SEEK-BAR THUMBNAILS MADE ON THIS DEVICE (src/miniatura_android.c is the C side).
//
// One keyframe at a time: seek the container index to the nearest keyframe, hand
// that one sample to a decoder, shrink the frame to 480x270 (JPEG for the bar)
// and to a 32x18 grey signature (OpenSeek's, for checking that a smaller copy of
// the same title lines up with what is playing). Reads go through our own range
// reader (256 KB blocks, small LRU), so nothing is downloaded ahead and the bytes
// are counted. The device's own decoder is tried first (a second session next to
// the player; the Shield allows it), Android's software decoder otherwise.
//
// Called from ONE C worker thread per title, through JNI; handles are ids.
object MiniaturaMotor {
    private const val TAG = "nuvio-miniatura"
    private const val BLK = 256 * 1024
    private const val NBLK = 24
    const val TW = 480   // shown at 576x324 in the seek preview (player.c); was 320x180
    const val TH = 270
    const val SW = 32
    const val SH = 18

    // `manter` = keep the socket alive between requests (the copy, read all the time). The PLAYING file is read
    // only for the check: each of its requests closes its socket, so no idle connection to the player's own link
    // stays open next to the player's (debrid CDNs limit connections per user; measured 22/10 on TorBox: past the
    // limit, the player's reconnects are refused and playback stops).
    private class Fonte(url: String, cabecalhos: String, val manter: Boolean) : MediaDataSource() {
        var alvo = url
        private val httpOwner = "thumbnail-${System.identityHashCode(this)}"
        @Volatile private var closed = false
        fun abortHttp() { MediaHttpBudget.cancelOwner(httpOwner) }
        private fun checkHttpCancelled() {
            if (closed || Thread.currentThread().isInterrupted) throw java.io.InterruptedIOException("thumbnail source cancelled")
        }
        private val playbackGate = ThumbnailPlaybackGate()
        private fun awaitPlayback() {
            var logged = false
            while (direto && playbackGate.shouldWait(NvPlayer.miniaturaPodeCriar(),
                    NvPlayer.thumbnailState, NvPlayer.thumbnailBufferMs,
                    android.os.SystemClock.elapsedRealtime() - NvPlayer.thumbnailHealthAtMs)) {
                checkHttpCancelled()
                if (!logged) { Log.i(TAG, "[playback-gate] holding HTTP bufferMs=${NvPlayer.thumbnailBufferMs} state=${NvPlayer.thumbnailState}"); logged = true }
                ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PLAYBACK_GATE).use { Thread.sleep(100) }
            }
            if (logged) Log.i(TAG, "[playback-gate] resumed HTTP bufferMs=${NvPlayer.thumbnailBufferMs}")
        }
        private fun openHttp(url: URL): HttpURLConnection {
            awaitPlayback() // Before acquiring a permit; leaves all four available for playback recovery.
            return MediaHttpBudget.open(url, httpOwner, factory = {
                if ((perfilBits and 32768) != 0) ThumbnailTransport.open(it)
                else it.openConnection() as HttpURLConnection
            }, checkCancelled = ::checkHttpCancelled).also { ThumbnailPayloadExchange.track(it) }
        }
        @Volatile var recusado = false    // explicit refusal or exhausted timeout retry: stop this title
        private val probeRecovery = ThumbnailProbeRecovery()
        private var optionalProbe = false
        val cab = cabecalhos.split('\n').mapNotNull { l ->
            val i = l.indexOf(':'); if (i <= 0) null else l.substring(0, i).trim() to l.substring(i + 1).trim()
        }
        var tamanho = -1L
        var lidos = 0L
        // Reads start where the extractor asks (the keyframe), not at a block boundary: a 720p keyframe (~100-150 KB)
        // is ONE request of 384 KB instead of two aligned 256 KB blocks half the time (measured 1.46 requests per
        // thumbnail with blocks). Big files (4K remuxes, keyframes of 1.5-3 MB): 2 MB windows.
        private var janela = 384 * 1024
        private var limite = 6L shl 20
        private val segs = ArrayList<Pair<Long, ByteArray>>()   // most recently used last
        private var guardados = 0L


        // The copy's window follows its keyframes: ~1.6x the largest of the recent ones (+32 KB for the container's
        // block headers), 128-384 KB. Measured 22/10: a fixed 384 KB read ~0.43 MB per 720p thumbnail, about 3x the
        // keyframe. A keyframe bigger than the window simply costs a second request, never a wrong frame.
        // Sized from the TYPICAL keyframe (running mean x 1.3 + 32 KB, 96-384 KB), not 1.6x the largest: measured
        // 22/10, an H.264 copy read ~0.5 MB per thumbnail for ~0.2 MB keyframes. A bigger keyframe costs one more
        // small request, never a wrong frame. +64 KB of margin for the block headers around it.
        @Volatile private var media = 0
        var direto = false    // the PLAYING file read for direct thumbnails: batches allowed, socket closed after each
            private set
        fun modoDireto() { direto = true; janela = 1 shl 20; limite = 16L shl 20; Log.i(TAG, "[fetch-policy] uniform intervalMs=1000 bytesPerSecond=5000000") }   // a ~4 MB batch must fit whole
        // a small read just past a cached range in direct mode (a block header, the rest of a keyframe): 256 KB, not a
        // whole keyframe window
        var fimAtual = -1L                              // block end of the keyframe being decoded (direct mode), -1 unknown
        private fun janelaFalta(p: Long, len: Int): Int {
            if (direto && fase != F_EXATO && fimAtual > p) return (fimAtual + margem - p).coerceIn(16L * 1024, 8L shl 20).toInt()
            return if (direto && fase != F_EXATO && len <= 64 * 1024 && segs.any { it.first + it.second.size == p || p - (it.first + it.second.size) in 0..65536 }) 256 * 1024 else janela
        }   // batches of ~8 MB must fit whole
        fun ajustarJanela(amostra: Int) {
            somaKf += amostra; nKf++
            if (direto) {
                // The window must hold the keyframe AND the next block's header: Android's Matroska parser reads it
                // right after the keyframe (measured 23/10, 4K remux: a 2 KB read just past a 512 KB window on most
                // thumbnails, one extra request each). The typical keyframe x 1.4 + 256 KB, 768 KB-4 MB.
                media = if (media == 0) amostra else (media * 7 + amostra) / 8
                janela = (media * 14 / 10 + 256 * 1024).coerceIn(768 * 1024, 4 shl 20)
                return
            }
            if (!manter || tamanho > (8L shl 30)) return
            media = if (media == 0) amostra else (media * 7 + amostra) / 8
            janela = (media * 13 / 10 + 64 * 1024).coerceIn(128 * 1024, 384 * 1024)
        }
        // keyframes per multi-range request: as many as ~4 MB of windows (4-32)
        // direct mode with exact block ranges: ~6 MB of keyframes per request (round trips were the limit once only the
        // keyframes were fetched: 2.8 keyframes per request, measured 23/10)
        fun loteIdeal(): Int = if (direto && nalTam > 0 && media > 0) ((8 shl 20) / media).coerceIn(4, 16)
            else if (direto) ((4 shl 20) / janela).coerceIn(2, 5) else ((4 shl 20) / janela).coerceIn(4, 32)

        // METADATA IS NEVER EVICTED: the header read when the file opens and the Cues (the keyframe index, usually at
        // the END of the file). Android's Matroska parser loads cue points lazily, a little further on each seek;
        // measured 23/10 (Bond copy): with the index evicted by the LRU, every seek re-read it, one extra request each.
        // Pinned: what the extractor reads while opening, anything inside the Cues element once it is known, and what
        // continues a pinned range in the last 10 % of the file (the index read on). At most 8 MB.
        private val fixos = ArrayList<Pair<Long, ByteArray>>()
        private var fixosBytes = 0L
        private var cuesIni = -1L; private var cuesFim = -1L
        private fun fixar(a: Long, n: Int): Boolean {
            if (fixosBytes + n > (8L shl 20)) return false
            if (fase == F_META) return true
            if (cuesIni >= 0 && a >= cuesIni && a < cuesFim) return true
            return tamanho > 0 && a >= tamanho / 10 * 9 && fixos.any { it.first + it.second.size == a }
        }
        private fun guardar(a: Long, d: ByteArray) {
            if (fixar(a, d.size)) { fixos.add(a to d); fixosBytes += d.size; diagCache(this); return }
            segs.add(a to d); guardados += d.size
            while (guardados > limite && segs.size > 1) guardados -= segs.removeAt(0).second.size
            diagCache(this)
        }
        private fun temByte(p: Long) = segs.any { p >= it.first && p < it.first + it.second.size } ||
            fixos.any { p >= it.first && p < it.first + it.second.size }

        // ---- READ DIAGNOSTICS (counters in D, bounded detail lines): what each network fetch was for ----
        @Volatile var fase = F_META           // set by quadro/quadroExato around the extractor's calls
        @Volatile var alvoMs = -1L            // the time the current seek asked for
        var cacheBytes = 0L                   // what this source holds (the engine's peak is summed over sources)
        private val baixados = ArrayList<LongArray>()   // every byte range fetched, merged [a, b): unique vs repeated
        fun registrarBaixa(a: Long, n: Int) {
            var b = a + n; var novo = n.toLong(); var i = 0
            var ini = a
            while (i < baixados.size) {
                val r = baixados[i]
                if (r[1] < ini) { i++; continue }
                if (r[0] > b) break
                novo -= maxOf(0L, minOf(r[1], a + n) - maxOf(r[0], a))
                ini = minOf(ini, r[0]); b = maxOf(b, r[1])
                baixados.removeAt(i)
            }
            baixados.add(i, longArrayOf(ini, b))
            D.addAndGet(D_UNICOS, novo); D.addAndGet(D_REPETIDOS, n - novo)
        }
        private fun jaBaixado(p: Long) = baixados.any { p >= it[0] && p < it[1] }
        // Index of the cue whose cluster holds byte p (largest cluster start <= p); -1 without an index.
        private fun cueDe(p: Long): Int {
            val c = cues ?: return -1
            var k = -1
            for (i in 0 until c.size / 3) if (c[i * 3 + 1] <= p && (k < 0 || c[i * 3 + 1] >= c[k * 3 + 1])) k = i
            return k
        }
        // Cue picked for time tMs: at-or-before (what Android's MatroskaExtractor seeks to, whatever the mode) or closest.
        fun cueParaTempo(tMs: Long, maisPerto: Boolean): Int {
            val c = cues ?: return -1
            val n = c.size / 3
            val tNs = tMs * 1_000_000L
            var lo = 0; var hi = n - 1
            while (lo < hi) { val mid = (lo + hi + 1) / 2; if (c[mid * 3] <= tNs) lo = mid else hi = mid - 1 }
            if (!maisPerto) return lo
            if (c[lo * 3] > tNs) return lo
            return if (lo + 1 < n && c[(lo + 1) * 3] - tNs < tNs - c[lo * 3]) lo + 1 else lo
        }
        // A network fetch for an extractor read (not a batch): counted by purpose, the first few per title in detail.
        private fun diagFalta(p: Long, pos: Long, len: Int) {
            val parcial = p != pos
            D.incrementAndGet(D_FASE0 + fase)
            if (jaBaixado(p)) D.incrementAndGet(D_EVICTED)
            val k = cueDe(p)
            val c = cues
            var cueTxt = "no-index"
            if (c != null && k >= 0) {
                val antes = if (alvoMs >= 0) cueParaTempo(alvoMs, false) else -1
                val perto = if (alvoMs >= 0) cueParaTempo(alvoMs, true) else -1
                if ((fase == F_SEEK || fase == F_AMOSTRA) && !parcial && k == antes && antes != perto) D.incrementAndGet(D_CUE_ANTES)
                cueTxt = "cue#$k t=${c[k * 3] / 1_000_000}ms cluster=${c[k * 3 + 1]} rel=${c[k * 3 + 2]} at+${p - c[k * 3 + 1]}" +
                    " | wanted ${alvoMs}ms: previous cue#$antes closest cue#$perto"
            }
            if (D.incrementAndGet(D_DETALHES) > 40) return
            var dist = Long.MAX_VALUE; var perto = "none"
            for (s in segs + fixos) {
                val a = s.first; val b = a + s.second.size
                val d = if (p < a) a - p else if (p >= b) p - b + 1 else 0
                if (d < dist) { dist = d; perto = "$a-$b" }
            }
            Log.i(TAG, "fetch[${D.get(D_DETALHES)}] ${FASES[fase]} ${if (parcial) "partial" else "miss"} read $pos+$len, missing from $p" +
                " | nearest cached $perto (${if (dist == Long.MAX_VALUE) "-" else "${dist / 1024} KB away"})" +
                " | fetched before: ${jaBaixado(p)} | window ${janela / 1024} KB | $cueTxt")
        }
        fun soltarCache() { segs.clear(); guardados = 0; fixos.clear(); fixosBytes = 0; diagCache(this); fecharDisco() }
        fun guardadosAgora() = guardados + fixosBytes

        // ---- MULTI-RANGE BATCHES (measured 22/10 on TorBox's Nexus servers: one GET with 8 ranges came back as a
        // 206 multipart/byteranges with all 8 parts in 373 ms, vs ~190 ms for EACH single range) ----
        // The keyframes' byte positions come from the Matroska index (Cues); a batch of thumbnails is ONE request on
        // the same single connection, stored here, and the extractor's own seek then finds the bytes in memory.
        @Volatile var multiOk = true          // false after one answer that was not multipart: single ranges from then on
        @Volatile private var cues: LongArray? = null   // [time ms, absolute cluster position, relative block position] x n
        private var cuesTentado = false
        private var mp4Index: ThumbnailMp4Index? = null
        private var prefetchUnsupportedLogged = false

        private fun ler(p: Long, n: Int): ByteArray? {
            if (p < 0 || p >= tamanho) return null
            val b = ByteArray(minOf(n.toLong(), tamanho - p).toInt())
            var f = 0
            while (f < b.size) { val r = readAt(p + f, b, f, b.size - f); if (r <= 0) return null; f += r }
            return b
        }
        private fun vint(b: ByteArray, i: Int, id: Boolean): Pair<Long, Int>? {   // (value, length)
            if (i >= b.size) return null
            val first = b[i].toInt() and 0xff
            var len = 1; var mask = 0x80
            while (len <= 8 && first and mask == 0) { len++; mask = mask shr 1 }
            if (len > 8 || i + len > b.size) return null
            var v = (if (id) first else first and (mask - 1)).toLong()
            for (k in 1 until len) v = (v shl 8) or (b[i + k].toLong() and 0xff)
            if (!id && v == (1L shl (7 * len)) - 1) v = -1   // unknown size
            return v to len
        }
        private fun uint(b: ByteArray, i: Int, n: Int): Long { var v = 0L; for (k in 0 until n) v = (v shl 8) or (b[i + k].toLong() and 0xff); return v }

        // Segment -> SeekHead -> Cues (+ Info's TimestampScale, Tracks' video track number). Null when not Matroska
        // or anything is unexpected: the batches are then simply not used.
        // Time (ms) of the keyframe the extractor lands on for tMs (the cue at or before it); -1 without an index.
        fun keyframeMs(tMs: Long): Long {
            val c = carregarCues() ?: return -1
            return c[cueParaTempo(tMs, false) * 3] / 1_000_000L
        }

        // Dolby Vision configuration in the Matroska track header (BlockAdditionMapping dvcC/dvvC): profile x 16 +
        // base-layer compatibility id, or -1 when there is none. Android 11's extractor does not report it (the track is
        // plain video/hevc), so a profile 5 file (IPT, no HDR10 base) would decode to green/purple pictures unnoticed.
        fun perfilDv(): Int {
            val header = ler(0, 8) ?: return -1
            if (header[0] != 0x1a.toByte() || header[1] != 0x45.toByte() ||
                header[2] != 0xdf.toByte() || header[3] != 0xa3.toByte()) {
                return ThumbnailDolbyVision.mp4(tamanho) { pos, count -> ler(pos, count) }
            }
            val h = ler(0, 256 * 1024) ?: return -1
            for (i in 0 until h.size - 12) {
                if (h[i] != 'd'.code.toByte() || h[i + 1] != 'v'.code.toByte() || (h[i + 2] != 'c'.code.toByte() && h[i + 2] != 'v'.code.toByte()) ||
                    h[i + 3] != 'C'.code.toByte()) continue
                for (k in i + 4 until minOf(h.size - 8, i + 48)) {
                    if (h[k] != 0x41.toByte() || h[k + 1] != 0xED.toByte()) continue
                    val (n, l) = vint(h, k + 2, false) ?: break
                    val d = k + 2 + l
                    if (n < 5 || d + 5 > h.size) break
                    val perfil = (h[d + 2].toInt() and 0xff) shr 1
                    val compat = (h[d + 4].toInt() and 0xff) shr 4
                    return perfil * 16 + compat
                }
            }
            return -1
        }

        private fun carregarCues(): LongArray? {
            if (cuesTentado) return cues
            cuesTentado = true
            try {
                val h = ler(0, 64 * 1024) ?: return null
                if (h.size < 4) return null
                if (h[0] != 0x1a.toByte() || h[1] != 0x45.toByte() || h[2] != 0xdf.toByte() || h[3] != 0xa3.toByte()) {
                    val index = ThumbnailMp4Index.read(tamanho) { pos, n -> ler(pos, n) }
                    if (index != null) {
                        mp4Index = index; nalTam = index.nalLength; cues = index.cues
                        Log.i(TAG, "[packet-index] container=mp4 syncSamples=${index.sizes.size} nalLength=$nalTam timestamps=edited-presentation")
                    }
                    if (index == null) Log.w(TAG, "[packet-index] MP4 packet fallback unavailable: unsupported or invalid sample tables; no downloaded samples claimed")
                    return cues
                }
                val (_, l0) = vint(h, 0, true) ?: return null
                val (ebmlSz, l1) = vint(h, l0, false) ?: return null
                var i = (l0 + l1 + ebmlSz).toInt()
                val (segId, s0) = vint(h, i, true) ?: return null
                if (segId != 0x18538067L) return null
                val (_, s1) = vint(h, i + s0, false) ?: return null
                val segIni = (i + s0 + s1).toLong()
                i = segIni.toInt()
                var cuesPos = -1L; var infoPos = -1L; var tracksPos = -1L
                // SeekHead is the first child of Segment in practice (mkvmerge, ffmpeg)
                val (shId, h0) = vint(h, i, true) ?: return null
                val (shSz, h1) = vint(h, i + h0, false) ?: return null
                if (shId != 0x114D9B74L) return null
                var j = i + h0 + h1
                val fim = j + shSz.toInt()
                while (j < fim && j < h.size) {
                    val (sid, a0) = vint(h, j, true) ?: break
                    val (ssz, a1) = vint(h, j + a0, false) ?: break
                    var k = j + a0 + a1
                    val kfim = k + ssz.toInt()
                    var qual = 0L; var onde = -1L
                    if (sid == 0x4DBBL) while (k < kfim) {
                        val (eid, e0) = vint(h, k, true) ?: break
                        val (esz, e1) = vint(h, k + e0, false) ?: break
                        if (eid == 0x53ABL) qual = uint(h, k + e0 + e1, esz.toInt())
                        if (eid == 0x53ACL) onde = uint(h, k + e0 + e1, esz.toInt())
                        k += e0 + e1 + esz.toInt()
                    }
                    when (qual) { 0x1C53BB6BL -> cuesPos = segIni + onde; 0x1549A966L -> infoPos = segIni + onde; 0x1654AE6BL -> tracksPos = segIni + onde }
                    j = kfim
                }
                if (cuesPos < 0) return null
                var escala = 1_000_000L   // ns per Matroska tick
                if (infoPos >= 0) ler(infoPos, 4096)?.let { b ->
                    val (_, i0) = vint(b, 0, true) ?: return@let
                    val (isz, i1) = vint(b, i0, false) ?: return@let
                    var k = i0 + i1
                    while (k < minOf(b.size, i0 + i1 + isz.toInt())) {
                        val (eid, e0) = vint(b, k, true) ?: break
                        val (esz, e1) = vint(b, k + e0, false) ?: break
                        if (eid == 0x2AD7B1L) escala = uint(b, k + e0 + e1, esz.toInt())
                        k += e0 + e1 + esz.toInt()
                    }
                }
                var trilhaVideo = -1L
                if (tracksPos >= 0) ler(tracksPos, 64 * 1024)?.let { b ->
                    val (_, t0) = vint(b, 0, true) ?: return@let
                    val (tsz, t1) = vint(b, t0, false) ?: return@let
                    var k = t0 + t1
                    while (k < minOf(b.size, t0 + t1 + tsz.toInt()) && trilhaVideo < 0) {
                        val (eid, e0) = vint(b, k, true) ?: break
                        val (esz, e1) = vint(b, k + e0, false) ?: break
                        if (eid == 0xAEL) {
                            var m = k + e0 + e1; var num = -1L; var tipo = -1L
                            var codec = ""; var priv: ByteArray? = null; var cod = false
                            while (m < k + e0 + e1 + esz.toInt() && m < b.size) {
                                val (fid, f0) = vint(b, m, true) ?: break
                                val (fsz, f1) = vint(b, m + f0, false) ?: break
                                val d0 = m + f0 + f1
                                if (fid == 0xD7L) num = uint(b, d0, fsz.toInt())
                                if (fid == 0x83L) tipo = uint(b, d0, fsz.toInt())
                                if (fid == 0x86L && d0 + fsz <= b.size) codec = String(b, d0, fsz.toInt(), Charsets.ISO_8859_1)
                                if (fid == 0x63A2L && d0 + fsz <= b.size) priv = b.copyOfRange(d0, d0 + fsz.toInt())
                                if (fid == 0x6D80L) cod = true          // ContentEncodings: header stripping/compression
                                m += d0 - m + fsz.toInt()
                            }
                            if (tipo == 1L) {
                                trilhaVideo = num
                                trilhaV = num
                                val pv = priv
                                nalTam = when {
                                    cod || pv == null -> 0
                                    codec.startsWith("V_MPEGH/ISO/HEVC") && pv.size > 22 -> (pv[21].toInt() and 3) + 1
                                    codec.startsWith("V_MPEG4/ISO/AVC") && pv.size > 5 -> (pv[4].toInt() and 3) + 1
                                    else -> 0
                                }
                                Log.i(TAG, "video track $num: $codec, ${if (nalTam > 0) "NAL length $nalTam: keyframes decoded straight from their blocks" else "blocks not usable directly${if (cod) " (content encoding)" else ""}"}")
                            }
                        }
                        k += e0 + e1 + esz.toInt()
                    }
                }
                val ch = ler(cuesPos, 16) ?: return null
                val (cid, c0) = vint(ch, 0, true) ?: return null
                val (csz, c1) = vint(ch, c0, false) ?: return null
                if (cid != 0x1C53BB6BL || csz <= 0 || csz > (16L shl 20)) return null
                cuesIni = cuesPos; cuesFim = cuesPos + c0 + c1 + csz
                val b = ler(cuesPos + c0 + c1, csz.toInt()) ?: return null
                val out = ArrayList<Long>()
                var k = 0
                while (k < b.size) {
                    val (pid, p0) = vint(b, k, true) ?: break
                    val (psz, p1) = vint(b, k + p0, false) ?: break
                    if (pid == 0xBBL) {
                        var m = k + p0 + p1; var t = -1L
                        val mfim = m + psz.toInt()
                        while (m < mfim && m < b.size) {
                            val (eid, e0) = vint(b, m, true) ?: break
                            val (esz, e1) = vint(b, m + e0, false) ?: break
                            if (eid == 0xB3L) t = uint(b, m + e0 + e1, esz.toInt())
                            if (eid == 0xB7L) {
                                var q = m + e0 + e1; var trilha = -1L; var cl = -1L; var rel = 0L
                                while (q < m + e0 + e1 + esz.toInt()) {
                                    val (gid, g0) = vint(b, q, true) ?: break
                                    val (gsz, g1) = vint(b, q + g0, false) ?: break
                                    when (gid) {
                                        0xF7L -> trilha = uint(b, q + g0 + g1, gsz.toInt())
                                        0xF1L -> cl = uint(b, q + g0 + g1, gsz.toInt())
                                        0xF0L -> rel = uint(b, q + g0 + g1, gsz.toInt())
                                    }
                                    q += g0 + g1 + gsz.toInt()
                                }
                                if (t >= 0 && cl >= 0 && (trilhaVideo < 0 || trilha == trilhaVideo)) {
                                    out += t * escala; out += segIni + cl; out += rel   // time in ns, as the extractor compares it
                                }
                            }
                            m += e0 + e1 + esz.toInt()
                        }
                    }
                    k += p0 + p1 + psz.toInt()
                }
                if (out.size < 30) return null
                cues = out.toLongArray()
                var comRel = 0
                for (q in 2 until out.size step 3) if (out[q] > 0) comRel++
                Log.i(TAG, "keyframe index: ${out.size / 3} entries (Matroska Cues), $comRel with CueRelativePosition > 0, multi-range batches on")
                return cues
            } catch (e: Exception) {
                return null
            }
        }

        // Prefetch the keyframes the extractor will land on for these times in ONE multi-range request. Returns how many
        // were fetched, 0 nothing to do, -1 batches not possible for this file.
        // The cue AT OR BEFORE each time, never the closest: Android's MatroskaExtractor (BlockIterator::seek, AOSP
        // frameworks/av) ignores the seek mode and always starts from the last cue <= the time. Picking the closest
        // fetched the NEXT keyframe about half the time, and the extractor then missed and made its own request.
        // Fonte is used by two threads in direct mode (the download loop and the CPU thumbnail thread): its entry
        // points take the Fonte's lock (order: Fonte, then VOO, everywhere).
        private var speculativeLimit = Long.MAX_VALUE
        fun prefetchBounded(times: LongArray, further: LongArray = longArrayOf()): Int = synchronized(this) {
            checkHttpCancelled()
            speculativeLimit = 8L shl 20
            try { prefetch0(times, further) } finally { speculativeLimit = Long.MAX_VALUE }
        }
        private fun boundedRanges(ranges: List<LongArray>): List<LongArray> {
            if (speculativeLimit == Long.MAX_VALUE) return ranges
            var remaining = speculativeLimit
            return ranges.filter { range ->
                val n = range[1] - range[0] + 1
                if (n <= remaining) { remaining -= n; true } else false
            }
        }

        fun completedOnDisk(times: LongArray): Boolean = synchronized(discoMu) {
            val c = cues ?: return@synchronized false
            times.isNotEmpty() && nalTam > 0 && times.all {
                val k = cueParaTempo(it, false)
                k >= 0 && k < c.size / 3 && k in noDisco
            }
        }
        fun prefetch(tempos: LongArray, proximos: LongArray = longArrayOf()): Int = synchronized(this) { prefetch0(tempos, proximos) }
        private fun prefetch0(tempos: LongArray, proximos: LongArray): Int {
            if (!multiOk || (!manter && !direto)) return -1
            val c = carregarCues() ?: run {
                if (!prefetchUnsupportedLogged) {
                    prefetchUnsupportedLogged = true
                    Log.i(TAG, "[prefetch] unsupported index; queued targets are not downloaded samples")
                }
                return -1
            }
            if (direto && mp4Index != null) return prefetchMp4(c, tempos)
            if (direto) return prefetchExato(c, tempos, proximos)
            val faixas = ArrayList<LongArray>()
            for (t in tempos) {
                val antes = cueParaTempo(t, false)
                val k = if (cueMaisPerto) cueParaTempo(t, true) else antes
                if (antes != cueParaTempo(t, true)) D.incrementAndGet(D_PERTO_DIFERE)
                D.incrementAndGet(D_PEDIDOS_LOTE)
                val ini = c[k * 3 + 1]
                if (temByte(ini)) continue
                val fim = minOf(tamanho - 1, ini + c[k * 3 + 2] + janela - 1)
                if (faixas.none { it[0] == ini }) faixas += longArrayOf(ini, fim)
            }
            if (faixas.isEmpty()) return 0
            faixas.sortBy { it[0] }
            val partes = synchronized(VOO) { pedirVarios(faixas) } ?: return -1
            for ((a, d) in partes) guardar(a, d)
            return partes.size
        }

        // ---- DIRECT MODE: EXACT KEYFRAME RANGES ----
        // A window sized from the typical keyframe wasted ~half the bytes on a 4K remux (measured 23/10: 343 MB for 297
        // thumbnails of 606 KB keyframes) and still missed the big ones (86 extra requests). Instead: a tiny multi-range
        // probe reads each keyframe's block header (16 bytes at cluster + CueRelativePosition: element ID + size), then
        // ONE batch fetches exactly [cluster start, block end + margin]. The margin covers what Android's parser reads
        // after the keyframe (it walks on to the next video block); it is learnt from the reads actually seen
        // (largest overshoot of the last 16 frames + 16 KB, 16-512 KB).
        private val fimBloco = ConcurrentHashMap<Int, Long>()     // cue index -> end of its keyframe's block (absolute)
        private val iniBloco = ConcurrentHashMap<Int, Long>()     // cue index -> start of its keyframe's block (absolute)
        var trilhaV = -1L; var nalTam = 0               // video track number; NAL length size (0 = blocks not usable)
        // KEYFRAME BLOCKS ON DISK (direct mode): fetched while the film plays, decoded later in a burst once the
        // decoder may exist (pause/scrub, see trabalharDireto). One file per source in the app's cache, at most 900 MB
        // and never below 1 GB free; deleted when the source closes.
        private var disco: java.io.RandomAccessFile? = null
        private val discoMu = Any()  // completed blocks can be read while Fonte waits for the next HTTP batch
        private var discoArq: java.io.File? = null
        private val noDisco = HashMap<Int, LongArray>()   // cue index -> [file offset, length]
        private var discoFim = 0L
        private var discoFalhou = false
        private fun guardarNoDisco(k: Int, d: ByteArray, de: Int, n: Int) =
            ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.DISK).use { guardarNoDisco0(k,d,de,n) }
        private fun guardarNoDisco0(k: Int, d: ByteArray, de: Int, n: Int) {
          synchronized(discoMu) {
            if (discoFalhou || k in noDisco || discoFim + n > (900L shl 20)) return
            try {
                if (disco == null) {
                    val dir = NvPlayer.contexto()?.cacheDir ?: run { discoFalhou = true; return }
                    if (dir.usableSpace < (1L shl 30)) { discoFalhou = true; return }
                    dir.listFiles { f -> f.name.startsWith("miniatura-blocos-") }?.forEach { it.delete() }   // leftovers
                    val f = java.io.File(dir, "miniatura-blocos-${System.nanoTime()}.bin")
                    discoArq = f; disco = java.io.RandomAccessFile(f, "rw")
                }
                val r = disco ?: return
                r.seek(discoFim); r.write(d, de, n)
                noDisco[k] = longArrayOf(discoFim, n.toLong()); discoFim += n
            } catch (e: Exception) { discoFalhou = true }
          }
        }
        private fun lerDoDisco(k: Int): ByteArray? =
            ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.DISK).use { lerDoDisco0(k) }
        private fun lerDoDisco0(k: Int): ByteArray? {
            val e = noDisco[k] ?: return null
            return try { val b = ByteArray(e[1].toInt()); disco?.let { it.seek(e[0]); it.readFully(b) } ?: return null; b } catch (x: Exception) { null }
        }
        fun fecharDisco() {
          synchronized(discoMu) {
            try { disco?.close() } catch (_: Exception) {}
            disco = null; discoArq?.delete(); discoArq = null; noDisco.clear(); discoFim = 0
          }
        }
        private val excessos = ArrayDeque<Long>()
        @Volatile var margem = 192L * 1024; private set
        var maxLido = 0L                                // furthest byte the extractor read during the current frame
        var nSondas = 0L; var nExatos = 0L; var nChutes = 0L
        fun fimBlocoPara(tMs: Long): Long { if (cues == null) return -1; return fimBloco[cueParaTempo(tMs, false)] ?: -1L }
        fun registrarExcesso(fim: Long) {
            if (fim <= 0 || maxLido <= 0) return
            excessos.addLast(maxOf(0L, maxLido - fim)); while (excessos.size > 16) excessos.removeFirst()
            margem = ((excessos.maxOrNull() ?: 0L) + 16 * 1024).coerceIn(16L * 1024, 512L * 1024)
        }
        private fun prefetchMp4(c: LongArray, tempos: LongArray): Int {
            val index = mp4Index ?: return -1
            val ks = tempos.map { cueParaTempo(it, false) }.distinct()
            // MP4 already provides exact sizes. CPU packets bypass ajustarJanela, so without this
            // feedback the scheduler stays at its four-frame bootstrap batch forever.
            val average = ks.sumOf { index.sizes[it].toLong() } / ks.size.coerceAtLeast(1)
            if (average > 0) media = if (media == 0) average.toInt() else ((media * 7L + average) / 8).toInt()
            Log.i(TAG, "[batch-plan] container=mp4 requestedFrames=${ks.size} averageKeyframeBytes=$average nextBatchFrames=${loteIdeal()} payloadBudgetBytes=${8 shl 20}")
            for (k in ks) {
                iniBloco[k] = c[k * 3 + 1]
                fimBloco[k] = c[k * 3 + 1] + index.sizes[k]
            }
            val missing = ks.filter { k ->
                val a = c[k * 3 + 1]; val end = a + index.sizes[k]
                val inRam = (fixos.asSequence() + segs.asSequence()).any { a >= it.first && end <= it.first + it.second.size }
                !inRam && synchronized(discoMu) { k !in noDisco }
            }
            val ranges = boundedRanges(missing.map { k -> longArrayOf(c[k * 3 + 1], c[k * 3 + 1] + index.sizes[k] - 1) })
            if (ranges.isEmpty()) {
                Log.i(TAG, "[prefetch] container=mp4 cachedSamples=${ks.size} fetchedSamples=0")
                return 0
            }
            // Preserve the existing ~8 MiB batch budget, even for unusual very large sync samples.
            var fetched = 0
            var batch = ArrayList<LongArray>(); var batchBytes = 0L
            fun download(): Boolean {
                if (batch.isEmpty()) return true
                val parts = synchronized(VOO) { pedirVarios(batch) } ?: return false
                for ((a, data) in parts) guardar(a, data)
                for (k in missing) {
                    val a = c[k * 3 + 1]; val n = index.sizes[k]
                    val part = parts.firstOrNull { a >= it.first && a + n <= it.first + it.second.size } ?: continue
                    synchronized(discoMu) { guardarNoDisco(k, part.second, (a - part.first).toInt(), n) }
                    iniBloco[k] = a; fimBloco[k] = a + n
                    fetched++
                }
                batch = ArrayList(); batchBytes = 0
                return true
            }
            for (range in ranges) {
                val n = range[1] - range[0] + 1
                if (batchBytes + n > (8L shl 20) && !download()) return -1
                batch.add(range); batchBytes += n
            }
            if (!download()) return -1
            Log.i(TAG, "[prefetch] container=mp4 cachedSamples=${ks.size - missing.size} fetchedSamples=$fetched")
            return fetched
        }
        private fun prefetchExato(c: LongArray, tempos: LongArray, proximos: LongArray): Int {
            val ks = tempos.map { cueParaTempo(it, false) }.distinct()
            D.addAndGet(D_PEDIDOS_LOTE, ks.size.toLong())
            // Find each keyframe's block with small probes. Round 1 reads the cluster's header plus CueRelativePosition
            // (counted from the cluster's DATA) + 64 bytes; when the index has no relative position (measured 23/10: a
            // remux with rel = 0 everywhere) or other elements come first (Timecode, audio blocks), the walk goes on
            // element by element, each further round reading 64 bytes where the previous one stopped (at most 6).
            val required = ks.toSet()
            val missing = ks.filter { !fimBloco.containsKey(it) }
            // Amortize tiny HTTP probes, not frame payload. Never add a request
            // solely for future work; leave the first batch and scrubs prompt.
            val mixed = (perfilBits and 8192) != 0 && nalTam > 0 && nExatos > 0
            val knownPayload = if (mixed) ks.mapNotNull { k ->
                val a = iniBloco[k] ?: return@mapNotNull null
                val b = fimBloco[k] ?: return@mapNotNull null
                if (b <= a || (temByte(a) && temByte(b - 1)) || synchronized(discoMu) { k in noDisco }) null
                else longArrayOf(a, b - 1)
            } else emptyList()
            val ahead = if (probeRecovery.allowed(android.os.SystemClock.elapsedRealtime()) && nExatos > 0 && (missing.isNotEmpty() || knownPayload.isNotEmpty())) proximos
                .map { cueParaTempo(it, false) }.distinct()
                .filter { it !in required && !fimBloco.containsKey(it) }
                .take((32 - missing.size).coerceAtLeast(0)) else emptyList()
            val pos = HashMap<Int, Long>()
            for (k in missing + ahead) pos[k] = -1L
            Log.i(TAG, "[fetch-audit] probe-plan source=${System.identityHashCode(this)} required=${missing.size} ahead=${ahead.size}")
            var ronda = 0
            while (pos.isNotEmpty() && ronda < 6) {
                ronda++
                val alvos = pos.entries.map { (k, p) ->
                    val a0 = if (p < 0) c[k * 3 + 1] else p
                    // later rounds read 64 KB: enough to walk past the audio blocks before the keyframe in one round
                    // (measured 23/10: 64-byte rounds, each a request >= 1 s apart, stalled a remux's batches)
                    val n = if (p < 0) 12 + minOf(c[k * 3 + 2], 65536L) + 64 else 65536L
                    longArrayOf(a0, minOf(tamanho - 1, a0 + n - 1), k.toLong(), if (p < 0) 1 else 0)
                }
                val probes = alvos.map { longArrayOf(it[0], it[1]) }.sortedBy { it[0] }.distinctBy { it[0] }
                val pedido = if (mixed && ronda == 1) thumbnailMixedRanges(probes, knownPayload) else probes
                val fused = pedido.size > probes.size
                if (fused) Log.i(TAG, "[mixed-batch] probes=${probes.size} payloads=${pedido.size - probes.size} bytes=${pedido.sumOf { it[1] - it[0] + 1 }}")
                val antes = multiOk
                semPausa = true
                optionalProbe = pos.keys.any { it !in required }
                val partes = try { synchronized(VOO) { pedirVarios(pedido) } } finally {
                    semPausa = false; optionalProbe = false; multiOk = antes
                }
                nSondas++
                if (partes == null) {
                    // A server may reject the larger range list. Retry only the
                    // current frames, with the normal pacing/permit rules.
                    if (pos.keys.any { it !in required }) {
                        pos.keys.retainAll(required)
                        Log.i(TAG, "[fetch-audit] probe-plan fallback=current-only")
                        continue
                    }
                    break
                }
                if (fused) {
                    for ((a, d) in partes) guardar(a, d)
                    var fusedPayloadBytes = 0L
                    for (k in ks) {
                        val a = iniBloco[k] ?: continue
                        val b = fimBloco[k] ?: continue
                        val part = partes.firstOrNull { a >= it.first && b <= it.first + it.second.size } ?: continue
                        guardarNoDisco(k, part.second, (a - part.first).toInt(), (b - a).toInt())
                        fusedPayloadBytes += b - a
                    }
                    if (speculativeLimit != Long.MAX_VALUE)
                        speculativeLimit = (speculativeLimit - fusedPayloadBytes).coerceAtLeast(0)
                }
                val prox = HashMap<Int, Long>()
                for (f in alvos) {
                    val k = f[2].toInt()
                    val parte = partes.firstOrNull { f[0] >= it.first && f[0] < it.first + it.second.size }
                    if (parte == null) {
                        if (k in required) prox[k] = if (f[3] == 1L) -1L else f[0]
                        continue
                    }
                    val bb = parte.second; val base = parte.first
                    var p = f[0]
                    if (f[3] == 1L) {                              // the cluster's own header first
                        val i0 = (p - base).toInt()
                        val (cid, h0) = vint(bb, i0, true) ?: continue
                        if (cid != 0x1F43B675L) continue
                        val (_, h1) = vint(bb, i0 + h0, false) ?: continue
                        p += h0 + h1 + c[k * 3 + 2]
                    }
                    var passos = 0
                    while (passos++ < 64) {
                        val i = (p - base).toInt()
                        if (i < 0 || i + 12 > bb.size) { prox[k] = p; break }   // continue in the next round
                        val (id, l0) = vint(bb, i, true) ?: break
                        val (tam, l1) = vint(bb, i + l0, false) ?: break
                        if (tam < 0 || tam > (64L shl 20) || id == 0x1F43B675L) break   // unknown size / next cluster
                        if (id == 0xA3L || id == 0xA0L) {
                            var t = i + l0 + l1
                            if (id == 0xA0L) {                     // BlockGroup: the track number is inside its Block
                                val (bid, b0) = vint(bb, t, true) ?: break
                                val (_, b1) = vint(bb, t + b0, false) ?: break
                                if (bid != 0xA1L) break
                                t += b0 + b1
                            }
                            if (t + 4 > bb.size) { prox[k] = p; break }
                            val trk = vint(bb, t, false)?.first ?: break
                            if (trilhaV < 0 || trk == trilhaV) {
                                iniBloco[k] = p; fimBloco[k] = p + l0 + l1 + tam
                                // the typical keyframe for the batch size (loteIdeal): the CPU route never passes through
                                // ajustarJanela, so without this the batches stayed at 4 (measured 23/10)
                                val t = (l0 + l1 + tam).toInt()
                                media = if (media == 0) t else (media * 7 + t) / 8
                                break
                            }
                        }
                        p += l0 + l1 + tam                         // Timecode, CRC, an audio block...: skip it
                    }
                }
                pos.clear(); pos.putAll(prox.filterKeys { it in required })
            }
            val faixas = ArrayList<LongArray>()
            for (k in ks) {
                val ini = c[k * 3 + 1]
                val fb = fimBloco[k]
                if (fb != null) nExatos++ else nChutes++
                val ib = iniBloco[k]
                val a0 = if (fb != null && ib != null && nalTam > 0) ib else ini
                val fim = minOf(tamanho - 1, when {
                    fb != null && nalTam > 0 -> fb - 1                 // the block alone: decoded without the extractor
                    fb != null -> fb + margem - 1
                    else -> ini + c[k * 3 + 2] + janela - 1
                })
                if ((temByte(a0) && temByte(fim)) || synchronized(discoMu) { k in noDisco }) continue
                faixas += longArrayOf(a0, fim)
            }
            if (faixas.isEmpty()) return 0
            faixas.sortBy { it[0] }
            val payloadRanges = boundedRanges(faixas)
            if (payloadRanges.isEmpty()) return 0
            val partes = synchronized(VOO) { pedirVarios(payloadRanges) } ?: return -1
            for ((a, d) in partes) guardar(a, d)
            if (nalTam > 0) for (k in ks) {
                val ib = iniBloco[k] ?: continue; val fb = fimBloco[k] ?: continue
                val pt = partes.firstOrNull { ib >= it.first && fb <= it.first + it.second.size } ?: continue
                guardarNoDisco(k, pt.second, (ib - pt.first).toInt(), (fb - ib).toInt())
            }
            return partes.size
        }
        @Volatile private var semPausa = false
        // DIRECT MODE PACE: >= 0.25 s between requests, <= 5 MB/s averaged, and (comVaga) never more connections to the
        // link than the player's own 4. Measured 23/10 (TorBox): a 5th connection next to the player's 4 got the CDN to
        // refuse connections twice (08:59 with a new socket per request, 12:27 with one kept-alive socket).
        private val speedPacing = ThumbnailPacing()
        private var lastSpeedPolicy = ThumbnailPacing.NORMAL
        private var lastPlaybackIncident = -1L
        private fun speedPolicy(): ThumbnailPacing.Policy {
            val now = android.os.SystemClock.elapsedRealtime()
            val incident = NvPlayer.thumbnailIncidentAtMs
            if (incident > lastPlaybackIncident) {
                lastPlaybackIncident = incident
                speedPacing.failure(incident, 15_000)
            }
            val policy = speedPacing.policy(now, (perfilBits and 4096) != 0,
                NvPlayer.thumbnailState == 3, NvPlayer.thumbnailPlaying,
                NvPlayer.thumbnailBufferMs, now - NvPlayer.thumbnailHealthAtMs,
                NvPlayer.thumbnailAllocatedBytes, NvPlayer.thumbnailTargetBytes, NvPlayer.thumbnailBufferStable)
            if (policy != lastSpeedPolicy) Log.i(TAG, "[speed-experiment] intervalMs=${policy.intervalMs} bytesPerSecond=${policy.bytesPerSecond} bufferMs=${NvPlayer.thumbnailBufferMs} allocatedBytes=${NvPlayer.thumbnailAllocatedBytes} targetBytes=${NvPlayer.thumbnailTargetBytes} stable=${NvPlayer.thumbnailBufferStable}")
            lastSpeedPolicy = policy
            return policy
        }
        private var proximaDireta = 0L
        private fun esperarTaxa(): Unit = synchronized(payloadDispatchMu) {
            val policy = speedPolicy()
            val now = android.os.SystemClock.elapsedRealtime()
            // Apply backoff before dispatch, even when the previous request used fast pacing.
            if (policy == ThumbnailPacing.NORMAL && lastDirectStart > 0)
                proximaDireta = maxOf(proximaDireta, lastDirectStart + maxOf(1000L, lastDirectBytes / 5000L))
            val espera = proximaDireta - now
            if (espera > 0) {
                val before = android.os.SystemClock.elapsedRealtime()
                ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(espera) }
                Log.i(TAG, "[fetch-audit] wait source=${System.identityHashCode(this)} kind=direct-pace ms=${android.os.SystemClock.elapsedRealtime() - before}")
            }
        }
        private fun auditFetch(kind: String, start: Long, headers: Long,
                               timing: ThumbnailReadTiming, payload: Int, permitNanos: Long) {
            probeRecovery.success()
            val end = System.nanoTime()
            val body = (end - headers).coerceAtLeast(0)
            val first = timing.firstReadNanos?.let { (it - start - permitNanos).coerceAtLeast(0) / 1_000_000 } ?: -1
            Log.i(TAG, "[fetch-audit] response source=${System.identityHashCode(this)} kind=$kind payloadKB=${payload / 1024} wireKB=${timing.bytesRead / 1024} headersMs=${(headers - start - permitNanos).coerceAtLeast(0) / 1_000_000} permitWaitMs=${permitNanos / 1_000_000} dispatchToDoneMs=${(end - start) / 1_000_000} firstReadMs=$first bodyMs=${body / 1_000_000} streamReadMs=${timing.readNanos / 1_000_000} localBodyMs=${(body - timing.readNanos).coerceAtLeast(0) / 1_000_000} totalMs=${(end - start - permitNanos).coerceAtLeast(0) / 1_000_000}")
        }
        // counted from the request's START (from its end, measured 23/10, it halved the rate to ~2.5 MB/s)
        private var lastDirectStart = 0L
        private var lastDirectBytes = 0L
        private fun marcarTaxa(bytes: Long, inicio: Long): Unit = synchronized(payloadDispatchMu) {
            // Same pacing for every addon/source: at least 1 s between requests and approximately 5 MB/s.
            val policy = speedPolicy()
            lastDirectStart = inicio; lastDirectBytes = bytes
            val byBytes = bytes * 1000 / policy.bytesPerSecond
            val spacing = maxOf(policy.intervalMs, byBytes)
            proximaDireta = inicio + spacing
            Log.i(TAG, "[fetch-plan] source=${System.identityHashCode(this)} bytes=$bytes binding=${if (byBytes > policy.intervalMs) "byte-cap" else "request-interval"} spacingMs=$spacing remainingWaitMs=${(proximaDireta - android.os.SystemClock.elapsedRealtime()).coerceAtLeast(0)}")
        }

        // The keyframe for tMs straight from its Matroska block, as Annex B into `out` (start codes, as the decoder
        // takes it): (pts us, bytes) or null when the block is unknown, laced, of another track or malformed (the
        // caller then uses the extractor). Its bytes come from the cache (the batch fetched exactly the block).
        @JvmOverloads
        fun blocoChave(tMs: Long, out: ByteBuffer, baseOnly: Boolean = false): Pair<Long, Int>? {
            // Lock order: Fonte -> discoMu for writes/fallback; fast reads never acquire Fonte.
            // Publish only fully written blocks. RandomAccessFile seek/read stays under discoMu.
            val pronto = synchronized(discoMu) {
                val c = cues
                val k = if (c != null) cueParaTempo(tMs, false) else -1
                if (k >= 0 && nalTam > 0 && (noDisco[k]?.get(1) ?: Long.MAX_VALUE) <= out.capacity())
                    lerDoDisco(k)?.let { it to c!![k * 3] / 1000 }
                else null
            }
            if (pronto != null) {
                if (mp4Index == null) return converterBloco(pronto.first, pronto.second, out, baseOnly)
                out.clear()
                return if (writePacket(pronto.first, out, 0, pronto.first.size, baseOnly)) pronto.second to out.position() else null
            }
            return synchronized(this) { blocoChave0(tMs, out, baseOnly) }
        }
        private fun blocoChave0(tMs: Long, out: ByteBuffer, baseOnly: Boolean): Pair<Long, Int>? {
            val c = cues ?: return null
            if (nalTam <= 0) return null
            val kk = cueParaTempo(tMs, false)
            val a = iniBloco[kk] ?: return null; val fim = fimBloco[kk] ?: return null
            if (fim - a > out.capacity()) return null
            val b = synchronized(discoMu) { lerDoDisco(kk) } ?: ler(a, (fim - a).toInt()) ?: return null
            return if (mp4Index != null) {
                out.clear()
                if (!writePacket(b, out, 0, b.size, baseOnly)) null else c[kk * 3] / 1000 to out.position()
            } else converterBloco(b, c[kk * 3] / 1000, out, baseOnly)
        }
        private fun converterBloco(b: ByteArray, ptsUs: Long, out: ByteBuffer, baseOnly: Boolean): Pair<Long, Int>? {
            val (id, l0) = vint(b, 0, true) ?: return null
            val (_, l1) = vint(b, l0, false) ?: return null
            var i = l0 + l1; var fimB = b.size
            if (id == 0xA0L) {                                     // BlockGroup: its Block (0xA1)
                var achou = false
                while (i < b.size) {
                    val (eid, e0) = vint(b, i, true) ?: break
                    val (esz, e1) = vint(b, i + e0, false) ?: break
                    if (eid == 0xA1L) { i += e0 + e1; fimB = minOf(b.size.toLong(), i + esz).toInt(); achou = true; break }
                    i += e0 + e1 + esz.toInt()
                }
                if (!achou) return null
            } else if (id != 0xA3L) return null
            val (trk, t0) = vint(b, i, false) ?: return null
            if (trilhaV >= 0 && trk != trilhaV) return null
            i += t0 + 2                                            // track number, 16-bit relative timecode
            if (i >= fimB) return null
            if ((b[i].toInt() and 0x06) != 0) return null          // laced: not a single frame
            i++
            out.clear()
            if (baseOnly) {
                if (!writePacket(b,out,i,fimB,true)) return null
            } else {
                var p = i
                while (p + nalTam <= fimB) {
                    var len = 0L
                    for (q in 0 until nalTam) len = (len shl 8) or (b[p + q].toLong() and 0xff)
                    p += nalTam
                    if (len <= 0 || p + len > fimB || out.remaining() < len + 4) return null
                    out.put(0); out.put(0); out.put(0); out.put(1)
                    out.put(b,p,len.toInt()); p+=len.toInt()
                }
                if(p!=fimB || out.position()==0) return null
            }
            out.flip()
            blocosLidos.incrementAndGet()
            return ptsUs to out.limit()
        }
        private var basePacketLogs=0
        private fun writePacket(b: ByteArray,out: ByteBuffer,start: Int,end: Int,baseOnly: Boolean): Boolean {
            if(!baseOnly) return ThumbnailNal.write(b,nalTam,out)
            val begin=System.nanoTime()
            val result=ThumbnailNal.writeBaseLayer(b,nalTam,out,start,end) ?: return false
            if(basePacketLogs++<3) Log.i(TAG,"[hevc-base-layer] path=length-prefix inputBytes=${result.inputBytes} outputBytes=${result.outputBytes} removedBytes=${result.removedBytes} removedNals=${result.removedNals} visitedNals=${result.visitedNals} elapsedUs=${(System.nanoTime()-begin)/1000}")
            return true
        }
        private val blocosLidos = java.util.concurrent.atomic.AtomicLong()
        val nBlocos: Long get() = blocosLidos.get()
        // Connection ownership is in MediaHttpBudget/ThumbnailHttpRedirects, never around a request twice.
        private fun <T> comVaga(f: () -> T): T {
            if (direto) esperarTaxa()
            return f()
        }
        // Retry at most one transient timeout. comVaga has already released
        // the player's permit before any cooldown; refusals/HTTP limits never retry.
        private fun <T> retryTimeout(request: () -> T): T {
            var retried = false
            while (true) {
                try { return request() } catch (e: java.net.SocketTimeoutException) {
                    if (retried || recusado) {
                        recusado = true
                        Log.w(TAG, "[fetch-audit] timeout retry exhausted: stopping thumbnails for this title")
                        throw e
                    }
                    retried = true
                    probeRecovery.timeout(android.os.SystemClock.elapsedRealtime())
                    Log.w(TAG, "[fetch-audit] transient timeout: slot released, cooling lookahead (bounded recovery), retry once after 2000 ms")
                    ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(2000) }
                }
            }
        }
        private fun pedirVarios(faixas: List<LongArray>): List<Pair<Long, ByteArray>>? = retryTimeout {
            try { comVaga { pedirVarios0(faixas) } } catch (e: java.net.SocketTimeoutException) {
                if (!optionalProbe || recusado) throw e
                probeRecovery.timeout(android.os.SystemClock.elapsedRealtime())
                Log.w(TAG, "[fetch-audit] expanded probe timed out: slot released, cooling lookahead (bounded recovery), cooldown 2000 ms, current-only fallback")
                ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(2000) }
                null
            }
        }
        private fun auditDispatch(ranges: Int, requestedBytes: Long) {
            val age = if (NvPlayer.thumbnailHealthAtMs > 0) android.os.SystemClock.elapsedRealtime() - NvPlayer.thumbnailHealthAtMs else -1
            Log.i(TAG, "[fetch-dispatch] source=${System.identityHashCode(this)} ranges=$ranges requestedBytes=$requestedBytes bufferMs=${NvPlayer.thumbnailBufferMs} playerState=${NvPlayer.thumbnailState} healthAgeMs=$age")
        }
        private val payloadDispatchMu = Any()
        private fun pedirParalelo(faixas: List<LongArray>): List<Pair<Long, ByteArray>> {
            val start = android.os.SystemClock.elapsedRealtime()
            try {
                val results = ThumbnailPayloadExchange.fetch(faixas, dispatch = { group ->
                    synchronized(payloadDispatchMu) {
                        checkHttpCancelled()
                        esperarTaxa()
                        marcarTaxa(group.sumOf { it[1]-it[0]+1 }, android.os.SystemClock.elapsedRealtime())
                    }
                    ThumbnailHttpRedirects.open(alvo, cab, "bytes=" + group.joinToString(",") { "${it[0]}-${it[1]}" },
                        !manter && (perfilBits and 32768)==0, {}, ::openHttp)
                }, lowBuffer = { NvPlayer.thumbnailState != 3 || NvPlayer.thumbnailBufferMs < 2000 })
                val parts = results.flatMap { it.parts }
                for ((a,data) in parts) registrarBaixa(a,data.size)
                val total = results.sumOf { it.bytes }
                lidos += total; lidosTotal += total; requisicoes += results.size
                msRede += results.sumOf { (it.elapsedMs-it.permitMs).coerceAtLeast(0) }
                nLotes += results.size; bytesLotes += total; kfLotes += faixas.size
                for (r in results) Log.i(TAG,"[payload-parallel] bytes=${r.bytes} elapsedMs=${r.elapsedMs} headersAndDispatchMs=${r.headersMs} bodyMs=${r.bodyMs} permitMs=${r.permitMs}")
                val overlapNs=(minOf(results[0].bodyEndNs,results[1].bodyEndNs)-maxOf(results[0].bodyStartNs,results[1].bodyStartNs)).coerceAtLeast(0)
                val unionNs=results.sumOf { it.bodyEndNs-it.bodyStartNs }-overlapNs
                val bodyRate=if(unionNs>0) total*1000.0/unionNs else 0.0
                Log.i(TAG,"[payload-parallel] batchMs=${android.os.SystemClock.elapsedRealtime()-start} bytes=$total exchanges=${results.size} bodyOverlapMs=${overlapNs/1_000_000} bodyUnionMs=${unionNs/1_000_000} bodyAggregateMBps=%.2f".format(bodyRate))
                resultado(android.os.SystemClock.elapsedRealtime()-start,true,total)
                return parts
            } catch(e: IOException) {
                Log.w(TAG,"[payload-parallel] failure type=${e.javaClass.simpleName} reason=${e.message}")
                checkHttpCancelled()
                speedPacing.failure(android.os.SystemClock.elapsedRealtime())
                resultado(0,false)
                if(e is ThumbnailPayloadExchange.Limited || e is java.net.ConnectException) recusado=true
                throw e
            }
        }
        private fun pedirVarios0(faixas: List<LongArray>): List<Pair<Long, ByteArray>>? {
            if ((perfilBits and 16384) != 0 && direto && !semPausa && !optionalProbe &&
                faixas.size >= 2 && faixas.sumOf { it[1]-it[0]+1 } in (1L shl 20)..(8L shl 20) &&
                NvPlayer.thumbnailState == 3 && NvPlayer.thumbnailBufferMs >= 3000 && !recusado)
                return pedirParalelo(faixas)
            if (recusado) throw IOException("refused earlier")
            if (direto) esperarTaxa()
            if (!semPausa) pausar()   // a 16-byte probe does not wait for the pace gap (the batch after it does)
            var c: HttpURLConnection? = null
            val ini = android.os.SystemClock.elapsedRealtime()
            val auditStart = System.nanoTime()
            val permitBase = MediaHttpBudget.threadWaitNanos()
            var receivedHeaders = false
            try {
                auditDispatch(faixas.size, faixas.sumOf { it[1] - it[0] + 1 })
                c = ThumbnailHttpRedirects.open(alvo, cab, "bytes=" + faixas.joinToString(",") { "${it[0]}-${it[1]}" }, !manter && (perfilBits and 32768) == 0, {}, ::openHttp)
                val code = c.responseCode
                val auditHeaders = System.nanoTime()
                receivedHeaders = true
                val tipo = c.contentType ?: ""
                if (code == 206 && !tipo.lowercase().startsWith("multipart/byteranges")) {
                    // ONE range back for several asked: a server may merge close/overlapping ranges into one (RFC 9110
                    // 14.2). When its Content-Range covers every range asked at a modest extra cost, that IS the batch.
                    // Measured 23/10 (Bond copy): this answer turned batches off for the whole title.
                    val m = Regex("bytes\\s+(\\d+)-(\\d+)/", RegexOption.IGNORE_CASE).find(c.getHeaderField("Content-Range") ?: "")
                    val a = m?.groupValues?.get(1)?.toLongOrNull() ?: -1L
                    val b = m?.groupValues?.get(2)?.toLongOrNull() ?: -1L
                    val pedido = faixas.sumOf { it[1] - it[0] + 1 }
                    val cobre = m != null && a <= faixas[0][0] && b >= faixas.last()[1] && b < tamanho
                    val n = b - a + 1
                    val usa = cobre && n <= pedido * 3 / 2 + (1 shl 20)
                    if (faixas.size > 1 || !usa) Log.i(TAG, "batch of ${faixas.size} ranges answered as ONE range $a-$b (${n / 1024} KB for ${pedido / 1024} KB asked)" +
                        if (usa) ": used as the batch" else ": single ranges from now on")
                    if (!usa) { multiOk = false; MediaHttpBudget.close(c); return null }
                    val d = ByteArray(n.toInt())
                    var got = 0
                    val timedBody = ThumbnailReadTiming(c.inputStream)
                    timedBody.use { s -> while (got < d.size) { val r = s.read(d, got, d.size - got); if (r < 0) break; got += r } }
                    if (got != d.size) throw IOException("short read")
                    MediaHttpBudget.close(c)
                    lidos += got; lidosTotal += got; requisicoes++
                    if (direto) marcarTaxa(got.toLong(), ini + (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000)
                    msRede += (android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0)
                    auditFetch("merged-range", auditStart, auditHeaders, timedBody, got, MediaHttpBudget.threadWaitNanos() - permitBase)
                    nLotes++; bytesLotes += got; kfLotes += faixas.size
                    resultado((android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0), true, got)
                    registrarBaixa(a, got)

                    return listOf(a to d)
                }
                if (code in listOf(401, 403, 429, 503, 509)) {
                    recusado = true
                    Log.w(TAG, "HTTP $code: stopping thumbnails for this title")
                    throw IOException("http $code")
                }
                if (code != 206) {
                    multiOk = false   // whole file: never again for this file (and read nothing more)
                    Log.w(TAG, "multi-range not served here (http $code, $tipo): single ranges from now on")
                    MediaHttpBudget.close(c)
                    return null
                }
                val fronteira = Regex("boundary=\"?([^\";]+)").find(tipo)?.groupValues?.get(1) ?: run { multiOk = false; MediaHttpBudget.close(c); return null }
                // Parsed AS IT ARRIVES, each part read straight into an array of its exact size: no whole-body buffer
                // (readBytes grows by doubling) and no copy per part. Measured 23/10 next to a 4K remux (the player holds
                // ~84 MB of samples on the same heap): those transient 10-20 MB per batch drove the heap to 0 % free,
                // GC stalls of up to 0.8 s, and audio hitches of ~0.5 s.
                val partes = ArrayList<Pair<Long, ByteArray>>()
                var total = 0
                val marca = "--$fronteira"
                val timedMultipart = ThumbnailReadTiming(c.inputStream)
                timedMultipart.buffered(16 * 1024).use { ins ->
                    fun linha(): String? {
                        val sb = StringBuilder()
                        while (true) {
                            val x = ins.read()
                            if (x < 0) return if (sb.isEmpty()) null else sb.toString()
                            if (x == '\n'.code) return sb.toString().trimEnd('\r')
                            sb.append(x.toChar())
                            if (sb.length > 4096) return null
                        }
                    }
                    var l = linha()
                    while (l != null && !l.startsWith(marca)) l = linha()
                    while (l != null && l.startsWith(marca) && !l.startsWith("$marca--")) {
                        var a = -1L; var b = -1L
                        while (true) {
                            val h = linha() ?: break
                            if (h.isEmpty()) break
                            Regex("^Content-Range:\\s*bytes\\s+(\\d+)-(\\d+)/", RegexOption.IGNORE_CASE).find(h)?.let {
                                a = it.groupValues[1].toLong(); b = it.groupValues[2].toLong()
                            }
                        }
                        if (a < 0 || b < a || b >= tamanho || b - a + 1 > (16L shl 20)) break
                        // a server may merge or reorder ranges (RFC 9110): each part is stored where ITS Content-Range says
                        val d = ByteArray((b - a + 1).toInt())
                        var got = 0
                        while (got < d.size) { val r = ins.read(d, got, d.size - got); if (r < 0) break; got += r }
                        if (got != d.size) break
                        partes += a to d

                        total += d.size
                        registrarBaixa(a, d.size)
                        l = linha()                                     // the CRLF ending the part
                        while (l != null && !l.startsWith(marca)) l = linha()
                    }
                }
                lidos += total
                lidosTotal += total
                if (direto) marcarTaxa(total.toLong(), ini + (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000)
                requisicoes++
                msRede += (android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0)
                auditFetch("multipart", auditStart, auditHeaders, timedMultipart, total, MediaHttpBudget.threadWaitNanos() - permitBase)
                nLotes++; bytesLotes += total; kfLotes += faixas.size
                resultado((android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0), true, total)
                MediaHttpBudget.close(c)
                if (partes.isEmpty()) { multiOk = false; return null }
                return partes
            } catch (e: IOException) {
                MediaHttpBudget.close(c)
                checkHttpCancelled()
                speedPacing.failure(android.os.SystemClock.elapsedRealtime())
                resultado(0, false)
                Log.w(TAG, "[fetch-audit] failure source=${System.identityHashCode(this)} phase=${if (receivedHeaders) "body" else "headers"} elapsedMs=${(System.nanoTime()-auditStart)/1_000_000} permitWaitMs=${(MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000} optionalProbe=$optionalProbe type=${e.javaClass.simpleName}")
                if (e is java.net.ConnectException) {
                    recusado = true
                    Log.w(TAG, "connection refused: stopping this title")
                }
                throw e
            }
        }

        private fun indice(h: ByteArray, agulha: ByteArray, de: Int): Int {
            var i = de
            while (i <= h.size - agulha.size) {
                var k = 0
                while (k < agulha.size && h[i + k] == agulha[k]) k++
                if (k == agulha.size) return i
                i++
            }
            return -1
        }

        private fun segmento(p: Long): Pair<Long, ByteArray> {
            for (f in fixos) if (p >= f.first && p < f.first + f.second.size) return f
            for (i in segs.indices.reversed()) {
                val s = segs[i]
                if (p >= s.first && p < s.first + s.second.size) { if (i != segs.size - 1) { segs.removeAt(i); segs.add(s) }; return s }
            }
            // A read that continues right where a cached segment ends during a FORWARD decode (an exact frame: the
            // check's fine pass, the gap fills): 1 MB at a time then, not a keyframe-sized window. Measured 23/10:
            // with ~190 KB windows, exact frames needed ~25 requests each, queued behind the batches, and timed out.
            // A keyframe that overflows its prefetched window only needs the rest of itself: one window, not 1 MB.
            val seguido = fase == F_EXATO && segs.any { it.first + it.second.size == p }
            val n = if (seguido && manter) maxOf(janela, 1 shl 20) else janelaFalta(p, lendoLen)
            diagFalta(p, lendoPos, lendoLen)
            val d = pedir(p, minOf(tamanho - 1, p + n - 1))
            buscou = true
            guardar(p, d)
            return p to d
        }

        // ONE request at a time for the whole engine, at most ~4 per second, on one kept-alive connection
        // (the stream is read to the end and closed, never disconnect()ed on success, so HttpURLConnection
        // reuses the socket). A refused or silent connection is never retried: the title stops (see recusado).
        // Every session (both lanes, the check's readers) waits for this: ONE request in flight for the whole engine,
        // so the lanes share one pooled socket per host and one lane decodes while the other's request is on the wire.
        private fun pedir(a: Long, b: Long): ByteArray = synchronized(VOO) { pedirUm(a, b) }

        private fun pedirUm(a: Long, b: Long): ByteArray = retryTimeout { comVaga { pedirUm0(a, b) } }
        private fun pedirUm0(a: Long, b: Long): ByteArray {
            if (recusado) throw IOException("refused earlier")
            if (direto) esperarTaxa()
            pausar()
            var ultimo: IOException? = null
            for (tentativa in 0 until 2) {
                if (tentativa > 0) { ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(2000) }; pausar() }
                var c: HttpURLConnection? = null
                val ini = android.os.SystemClock.elapsedRealtime()
            val auditStart = System.nanoTime()
            val permitBase = MediaHttpBudget.threadWaitNanos()
                try {
                    auditDispatch(1, b - a + 1)
                    c = ThumbnailHttpRedirects.open(alvo, cab, "bytes=$a-$b", !manter && (perfilBits and 32768) == 0, {}, ::openHttp)
                    val code = c.responseCode
                val auditHeaders = System.nanoTime()
                    if (code in listOf(401, 403, 429, 503, 509)) {
                        recusado = true
                        Log.w(TAG, "HTTP $code: stopping thumbnails for this title")
                    }
                    if (code != 206) throw IOException("http $code")
                    alvo = c.url.toString()   // after debrid/add-on redirects: later reads go straight to the CDN
                    c.getHeaderField("Content-Range")?.substringAfter('/')?.trim()?.toLongOrNull()?.let { tamanho = it }
                    val n = c.contentLength
                    val d = ByteArray(n)
                    var got = 0
                    val timedBody = ThumbnailReadTiming(c.inputStream)
                    timedBody.use { s -> while (got < n) { val r = s.read(d, got, n - got); if (r < 0) break; got += r } }
                    if (got != n) throw IOException("short read")
                    if (b > a) {   // not the 0-0 size probe
                        val cr = c.getHeaderField("Content-Range") ?: ""
                        if (!cr.startsWith("bytes $a-")) throw IOException("range not honoured")
                        registrarBaixa(a, got)
                        D.incrementAndGet(D_UNITARIAS)
                    }
                    lidos += got
                    lidosTotal += got
                    if (direto) marcarTaxa(got.toLong(), ini + (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000)
                    requisicoes++
                    msRede += (android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0)
                    auditFetch("single", auditStart, auditHeaders, timedBody, got, MediaHttpBudget.threadWaitNanos() - permitBase)
                    resultado((android.os.SystemClock.elapsedRealtime() - ini - (MediaHttpBudget.threadWaitNanos() - permitBase) / 1_000_000).coerceAtLeast(0), true, got)
                    MediaHttpBudget.close(c)
                    return d
                } catch (e: IOException) {
                    MediaHttpBudget.close(c)
                    checkHttpCancelled()
                    speedPacing.failure(android.os.SystemClock.elapsedRealtime())
                    ultimo = e
                    resultado(0, false)
                    if (e is java.net.ConnectException) {
                        recusado = true
                        Log.w(TAG, "connection refused: stopping this title")
                        break
                    }
                    if (e is java.net.SocketTimeoutException) throw e
                    if (recusado) break
                }
            }
            throw ultimo ?: IOException("read failed")
        }

        private var lendoPos = 0L; private var lendoLen = 0; private var buscou = false
        override fun readAt(pos: Long, buf: ByteArray, off: Int, len: Int): Int = synchronized(this) { readAt0(pos, buf, off, len) }
        private fun readAt0(pos: Long, buf: ByteArray, off: Int, len: Int): Int {
            if (pos >= tamanho) return -1
            lendoPos = pos; lendoLen = len; buscou = false
            if (pos + len > maxLido) maxLido = minOf(tamanho, pos + len)
            val dentro = temByte(pos)
            D.incrementAndGet(D_LEITURAS)
            return try {
                var feito = 0
                while (feito < len && pos + feito < tamanho) {
                    val p = pos + feito
                    val (ini0, d) = segmento(p)
                    val ini = (p - ini0).toInt()
                    val k = minOf(len - feito, d.size - ini)
                    if (k <= 0) break
                    System.arraycopy(d, ini, buf, off + feito, k)
                    feito += k
                }
                D.incrementAndGet(if (!buscou) D_HITS else if (dentro) D_PARCIAIS else D_FALTAS)
                feito
            } catch (e: Exception) {
                erroRede = true   // the extractor only sees "end of data": tell the C side it was the network
                -1
            }
        }
        @Volatile var erroRede = false

        override fun getSize() = tamanho
        // Initialize all request/error state before constructor I/O can fail.
        init {
            pedir(0, 0)
            if (tamanho > (8L shl 30)) { janela = 2 shl 20; limite = 12L shl 20 }
        }
        override fun close() {
            closed = true
            abortHttp()
            synchronized(this) { /* Wait for a cancelled prefetch before deleting its files. */ }
        }
    }

    private class Sessao(val fonte: Fonte, val ex: MediaExtractor, val fmt: MediaFormat, val mime: String) {
        @Volatile var dec: MediaCodec? = null
        var soft = false
        var hdr = false
        var dvProfile = -1
        var warmFrames = 0
        var hardwareBaselineAvailable = 0L
        // Hardware decoder that gives no frame: first again without the low priority, then (<= 1080p) the software one.
        // Measured 22/10: the Shield's H.264 decoder at priority 1 next to playback timed out on every frame.
        var travadas = 0
        // Codec errors (IllegalState/CodecException) on this file: at 2 the file is given up. Measured 22/10: a copy
        // whose frames threw at once made 8 decoders in ~1 s, and the player's own decoder failed (4003) next to it.
        var errosCodec = 0
        // reused per session: the frame's planes copied in one go (see montar)
        var yArr = ByteArray(0); var uArr = ByteArray(0); var vArr = ByteArray(0)
        var ultimoPts = Long.MIN_VALUE
        var ultimoRes: ByteArray? = null
        // direct mode (quadroDireto): hardware decoder -> SurfaceTexture -> 480x270 on the GPU
        var gpu: GpuMiniatura? = null
        var visW = 0; var visH = 0           // visible picture after crop (rotation applied)
        var amostra: ByteBuffer? = null      // the keyframe, kept to queue it twice
        var pedido = 0L                      // generation of the current request: outputs of earlier ones never count
        var saida: ByteArrayOutputStream? = null   // reused for each direct reply
        var csdAnexoB: ByteArray? = null           // parameter sets (Annex B) for the CPU route
        var amostraCpu: ByteBuffer? = null         // the CPU thread's keyframe buffer
        var memoryAudit: ThumbnailMemoryAudit? = null
        var prefetch: ThumbnailPrefetch? = null
    }

    private val sessoes = ConcurrentHashMap<Long, Sessao>()
    private var proximo = 1L
    private val VOO = Any()

    // ---- read diagnostics for the per-title SUMMARY (diag()); reset when a title starts ----
    const val F_META = 0; const val F_SEEK = 1; const val F_AMOSTRA = 2; const val F_AVANCO = 3; const val F_EXATO = 4
    private val FASES = arrayOf("meta", "seek", "sample", "advance", "exact")
    private const val D_LEITURAS = 0; private const val D_HITS = 1; private const val D_PARCIAIS = 2; private const val D_FALTAS = 3
    private const val D_FASE0 = 4   // 4..8: single fetches by purpose
    private const val D_EVICTED = 9; private const val D_UNICOS = 10; private const val D_REPETIDOS = 11
    private const val D_PICO = 12; private const val D_CUE_ANTES = 13; private const val D_PERTO_DIFERE = 14
    private const val D_PEDIDOS_LOTE = 15; private const val D_DETALHES = 16; private const val D_UNITARIAS = 17
    private const val D_CACHE = 18
    private val D = java.util.concurrent.atomic.AtomicLongArray(19)
    private fun diagCache(f: Fonte) {
        val antes = f.cacheBytes
        f.cacheBytes = f.guardadosAgora()
        val total = D.addAndGet(D_CACHE, f.cacheBytes - antes)
        while (true) { val p = D.get(D_PICO); if (total <= p || D.compareAndSet(D_PICO, p, total)) break }
    }
    // debug.nuvio.miniatura bits (read by the C side when a title starts): 2 = old closest-cue prefetch (A/B)
    @Volatile private var cueMaisPerto = false
    @Volatile private var perfilBits = 0
    @Volatile private var perfilLargura = 0
    @JvmStatic fun pipelineStart(elapsedMs: Long) { ThumbnailPipelineTimeline.clock.start(elapsedMs) }
    @JvmStatic fun pipelineStop() {}
    @JvmStatic fun pipelineSummary(elapsedMs: Long): String = ThumbnailPipelineTimeline.summary(elapsedMs)
    private val nativeTimelineScopes = object : ThreadLocal<MutableMap<Int, java.util.ArrayDeque<AutoCloseable>>>() {
        override fun initialValue() = mutableMapOf<Int, java.util.ArrayDeque<AutoCloseable>>()
    }
    @JvmStatic fun pipelineState(state: Int, begin: Boolean) {
        val scopes = nativeTimelineScopes.get().getOrPut(state) { java.util.ArrayDeque() }
        if (begin) scopes.addLast(ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.entries[state]))
        else if (scopes.isNotEmpty()) scopes.removeLast().close()
    }
    @JvmStatic
    fun configurar(bits: Int, largura: Int) {
        perfilLargura = largura
        perfilBits = bits
        cueMaisPerto = bits and 2 != 0
        dumpar = if (bits and 16 != 0) 12 else 0
        if (bits == 0) return
        if (pastaTeste == null) pastaTeste = try { NvPlayer.contexto()?.getExternalFilesDir("miniaturas-teste") } catch (e: Exception) { null }
    }

    @JvmStatic
    fun perdidos(): Long = NvPlayer.quadrosPerdidosAgora()
    @JvmStatic
    fun audioFalhas(): Long = NvPlayer.audioFalhasAgora()

    // Free room on this app's Java heap (MB): the player's sample buffer lives there too. The C side pauses thumbnail
    // work below 64 MB so a thumbnail never forces a blocking GC on the player's threads.
    @JvmStatic
    fun heapLivreMb(): Long { val r = Runtime.getRuntime(); return (r.maxMemory() - (r.totalMemory() - r.freeMemory())) shr 20 }

    @JvmStatic
    fun diag(zerar: Boolean): String {
        val mb = 1048576.0
        val s = "reads ${D.get(D_LEITURAS)} (hit ${D.get(D_HITS)}, partial ${D.get(D_PARCIAIS)}, miss ${D.get(D_FALTAS)}) | " +
            "single fetches ${D.get(D_UNITARIAS)}: meta ${D.get(D_FASE0)} seek ${D.get(D_FASE0 + 1)} sample ${D.get(D_FASE0 + 2)} " +
            "advance ${D.get(D_FASE0 + 3)} exact ${D.get(D_FASE0 + 4)}, refetched-after-evict ${D.get(D_EVICTED)}, " +
            "on the previous cue (prefetch took the next) ${D.get(D_CUE_ANTES)} | batch keyframes asked ${D.get(D_PEDIDOS_LOTE)}, " +
            "closest!=previous ${D.get(D_PERTO_DIFERE)}${if (cueMaisPerto) " (closest-cue mode)" else ""} | " +
            "unique %.1f MB, repeated %.1f MB, peak cache %.1f MB".format(D.get(D_UNICOS) / mb, D.get(D_REPETIDOS) / mb, D.get(D_PICO) / mb)
        if (zerar) { nDireto = 0; nDiretoCache = 0; msDiretoCache = 0; nDup = 0; nAvanco = 0; nDiretoFalhas = 0; msGpu = 0; msJpeg = 0; msSeek = 0; msEsperaQuadro = 0 }
        if (zerar) for (i in 0 until D_CACHE) if (i != D_PICO) D.set(i, 0)
        if (zerar) D.set(D_PICO, D.get(D_CACHE))
        return s + " | " + MediaHttpBudget.snapshot()
    }

    // Request pacing for ALL sessions together, adaptive. At most ~4/s (250 ms apart; one request takes ~0.29 s, so
    // in practice the single in-flight request is the limit, ~3.4/s). Two parallel connections next to the player
    // got its reconnects refused by TorBox (22/10), so there is never more than one. Any
    // failure or a slow answer (> 1.5 s) doubles the gap at once (up to 1.4 s, ~0.7/s); 20 quick answers in a row
    // take 20 ms off again. The first Shield run showed what crowding the link does: the player's own reconnects
    // were refused.
    private const val INTERVALO_MIN_MS = 250L
    private const val INTERVALO_MAX_MS = 1400L
    private const val LENTO_MS = 1500L
    private var intervalo = INTERVALO_MIN_MS
    private var rapidas = 0
    private var ultimaReq = 0L
    @Volatile private var requisicoes = 0L

    private fun pausar() = synchronized(this) {
        val espera = ultimaReq + intervalo - android.os.SystemClock.elapsedRealtime()
        if (espera > 0) {
            val before = android.os.SystemClock.elapsedRealtime()
            ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(espera) }
            Log.i(TAG, "[fetch-audit] wait kind=adaptive-pace ms=${android.os.SystemClock.elapsedRealtime() - before}")
        }
        ultimaReq = android.os.SystemClock.elapsedRealtime()
    }

    // "Slow" is judged against the size: 1.5 s plus ~0.7 s per MB (a 2 MB 4K window at 2.7 s is a normal download,
    // not a struggling link; measured 22/10, it cut the pace to 1/s for the rest of a 3 h film). Recovery is
    // multiplicative: 10 good answers take 25 % off the gap, so ~50 requests from the floor back to full pace.
    private fun resultado(ms: Long, ok: Boolean, bytes: Int = 0) = synchronized(this) {
        if (ok && bytes <= 1) return@synchronized          // the 0-0 size probe: its latency says nothing about the link
        // In the speed experiment, successful slow responses are measured, not treated as failures.
        // The request's own duration already limits throughput; actual failures retain backoff.
        if (!ok || ((perfilBits and 4096) == 0 && ms > LENTO_MS + bytes / 1500)) {
            val antes = intervalo
            intervalo = minOf(INTERVALO_MAX_MS, intervalo * 2)
            rapidas = 0
            if (intervalo != antes) Log.i(TAG, "slowing down: ${if (ok) "slow answer ${ms} ms for ${bytes / 1024} KB" else "failed request"}, now ${1000 / intervalo}/s")
        } else if (++rapidas >= 10 && intervalo > INTERVALO_MIN_MS) {
            intervalo = maxOf(INTERVALO_MIN_MS, intervalo * 3 / 4)
            rapidas = 0
        }
    }

    @JvmStatic
    fun ritmo(): Long = 1000 / intervalo

    @JvmStatic
    fun requisicoes(): Long = requisicoes

    @Volatile private var criados = 0L

    // For the per-title summary: [requests sent, decoders created] since the app started (the C side takes deltas).
    @JvmStatic
    // [requests, decoders created, bytes, network ms, frame ms, batches, batch bytes, batch keyframes, keyframe bytes, keyframes]
    fun contadores(): LongArray = longArrayOf(requisicoes, criados, lidosTotal, msRede, msQuadros, nLotes, bytesLotes, kfLotes, somaKf, nKf,
        msDecod, msMontar)
    @Volatile private var msDecod = 0L      // keyframe thumbnails only: until the decoder hands the frame out
    @Volatile private var msMontar = 0L     // keyframe thumbnails only: resize + JPEG
    @Volatile private var lidosTotal = 0L
    @Volatile private var msRede = 0L
    @Volatile private var msQuadros = 0L
    @Volatile private var nLotes = 0L
    @Volatile private var bytesLotes = 0L
    @Volatile private var kfLotes = 0L
    @Volatile private var somaKf = 0L
    @Volatile private var nKf = 0L

    @JvmStatic
    fun lote(h: Long): Int = sessoes[h]?.fonte?.loteIdeal() ?: 8

    // The link AFTER the add-on/debrid redirect (the CDN address): later sessions on the same copy open it directly,
    // so the add-on mints one debrid link per copy, not one per session.
    @JvmStatic
    fun alvo(h: Long): String = sessoes[h]?.fonte?.alvo ?: ""

    // ---- the tone curve for PQ (HDR10 / Dolby Vision) frames: nits -> SDR grey, OpenSeek's Hable ----
    private val pqLut: IntArray by lazy {
        fun hable(x: Double): Double { val a = 0.15; val b = 0.5; val c = 0.1; val d = 0.2; val e = 0.02; val f = 0.3
            return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f }
        val m1 = 0.1593017578125; val m2 = 78.84375; val c1 = 0.8359375; val c2 = 18.8515625; val c3 = 18.6875
        val white = hable(11.2)
        IntArray(256) { i ->
            val e = (i / 255.0).pow(1 / m2)
            val nits = 10000.0 * (maxOf(e - c1, 0.0) / (c2 - c3 * e)).pow(1 / m1)
            val v = hable(nits / 100.0 * 2.0) / white
            (255.0 * maxOf(0.0, minOf(1.0, v)).pow(1 / 2.2)).toInt()
        }
    }

    @JvmStatic
    fun abrir(url: String, cabecalhos: String, manter: Boolean): Long = try {
        val f = Fonte(url, cabecalhos, manter)
        val ex = MediaExtractor()
        ex.setDataSource(f)
        var s: Sessao? = null
        for (i in 0 until ex.trackCount) {
            val fm = ex.getTrackFormat(i)
            val m = fm.getString(MediaFormat.KEY_MIME) ?: continue
            if (m.startsWith("video/")) { ex.selectTrack(i); s = Sessao(f, ex, fm, m); break }
        }
        if (s == null) { Log.w(TAG, "open failed: no video track (${ex.trackCount} tracks)"); ex.release(); 0L } else {
            s.hdr = s.fmt.containsKey(MediaFormat.KEY_COLOR_TRANSFER) &&
                s.fmt.getInteger(MediaFormat.KEY_COLOR_TRANSFER) == MediaFormat.COLOR_TRANSFER_ST2084 ||
                s.mime == "video/dolby-vision"
            synchronized(this) { val id = proximo++; sessoes[id] = s; id }
        }
    } catch (e: Exception) {
        Log.w(TAG, "open failed: ${e.javaClass.simpleName}: ${e.message?.take(80)?.replace(Regex("https?://\\S+"), "<url>")}")
        0L
    }

    // [duration ms, width, height, hdr, bytes read so far, codec usable (1 = H.264/HEVC/Dolby Vision)]
    // Only those: 23/10 a 1080p film's copy was MPEG-4 Part 2 (XviD) in Matroska and Android's software decoder
    // failed on every frame; OpenSeek accepts the same two codecs.
    @JvmStatic
    fun info(h: Long): LongArray {
        val s = sessoes[h] ?: return LongArray(6)
        val d = if (s.fmt.containsKey(MediaFormat.KEY_DURATION)) s.fmt.getLong(MediaFormat.KEY_DURATION) / 1000 else 0L
        val codecOk = s.mime == "video/avc" || s.mime == "video/hevc" || s.mime == "video/dolby-vision"
        return longArrayOf(d, s.fmt.getInteger(MediaFormat.KEY_WIDTH).toLong(), s.fmt.getInteger(MediaFormat.KEY_HEIGHT).toLong(),
            if (s.hdr) 1 else 0, s.fonte.lidos, if (codecOk) 1 else 0)
    }

    @JvmStatic
    fun lidos(h: Long): Long = sessoes[h]?.fonte?.lidos ?: 0L

    // Prefetch the keyframes for these copy times in one multi-range request. >= 0 fetched, -1 not possible for this
    // file (not Matroska, no index, server answered single ranges), -2 the connection was refused (stop the title).
    private fun pipelineAllowed(s: Sessao): Boolean {
        if (s.dec == null || (perfilBits and 2048) == 0) return false
        if (!thumbnailOverlapBufferHealthy(NvPlayer.thumbnailState == 3, NvPlayer.thumbnailPlaying,
            NvPlayer.thumbnailBufferMs, android.os.SystemClock.elapsedRealtime() - NvPlayer.thumbnailHealthAtMs,
            NvPlayer.thumbnailAllocatedBytes, NvPlayer.thumbnailTargetBytes, NvPlayer.thumbnailBufferStable)) return false
        val heap = Runtime.getRuntime().let { it.maxMemory() - it.totalMemory() + it.freeMemory() }
        if (heap < (96L shl 20)) return false
        val am = NvPlayer.contexto()?.getSystemService(android.content.Context.ACTIVITY_SERVICE) as? android.app.ActivityManager ?: return false
        val memory = android.app.ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        return memory.availMem >= (364L shl 20)
    }
    @JvmStatic @JvmOverloads
    fun preparar(h: Long, tempos: LongArray, proximos: LongArray = longArrayOf()): Int {
        val s = sessoes[h] ?: return -1
        val result = try { s.fonte.prefetch(tempos, proximos) } catch (e: IOException) { if (s.fonte.recusado) -2 else -1 }
        if (result >= 0 && proximos.isNotEmpty() && pipelineAllowed(s) && s.fonte.completedOnDisk(tempos)) {
            val ahead = s.prefetch ?: ThumbnailPrefetch(
                allowed = { pipelineAllowed(s) },
                prepare = { times -> s.fonte.prefetchBounded(times) },
                prepareFurther = { times, further -> s.fonte.prefetchBounded(times, further) },
                abort = { s.fonte.abortHttp() },
                report = { message -> Log.i(TAG, "[prefetch-pipeline] source=${System.identityHashCode(s.fonte)} $message") }
            ).also { s.prefetch = it }
            ahead.offer(proximos.take(16).toLongArray(),
                if ((perfilBits and 65536) != 0) proximos.drop(16).take(16).toLongArray() else longArrayOf())
        }
        return result
    }

    // 1 ready on disk; 0 still fetching; -1 normal synchronous/fallback path.




    private var ultimaCriacao = 0L
    private fun decoder(s: Sessao): MediaCodec? {
        s.dec?.let { return it }
        if (s.errosCodec >= 2) return null
        synchronized(this) {
            val espera = ultimaCriacao + 2000 - android.os.SystemClock.elapsedRealtime()
            if (espera > 0) ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(espera) }
            ultimaCriacao = android.os.SystemClock.elapsedRealtime()
        }
        val altura = s.fmt.getInteger(MediaFormat.KEY_HEIGHT)
        val tentativas = mutableListOf<Pair<String?, Boolean>>()
        val mime = if (s.mime == "video/dolby-vision") "video/hevc" else s.mime
        val softs = MediaCodecList(MediaCodecList.ALL_CODECS).codecInfos.filter { ci ->
            !ci.isEncoder && (ci.name.startsWith("OMX.google.") || ci.name.startsWith("c2.android.")) && ci.supportedTypes.any { it.equals(mime, true) }
        }.map { it.name to true }
        // Up to 1080p the CPU decodes a keyframe in ~50-100 ms (Shield): software FIRST, so thumbnails never take a
        // hardware session next to the player (the Shield's H.264 one gave no frames there, 22/10). Above: hardware.
        if (altura <= 1088) { tentativas += softs; tentativas += null to false }
        else { if (s.travadas < 2) tentativas += null to false; tentativas += softs }
        for ((nome, soft) in tentativas) {
            var c: MediaCodec? = null
            try {
                c = if (nome == null) MediaCodec.createDecoderByType(s.mime) else MediaCodec.createByCodecName(nome)
                val f = MediaFormat(s.fmt)
                if (s.mime == "video/dolby-vision" && soft) f.setString(MediaFormat.KEY_MIME, "video/hevc")
                // Not real time: the player's decoder keeps the priority (shared hardware on the Shield), unless that
                // already left this decoder without frames once.
                if (s.travadas == 0) f.setInteger(MediaFormat.KEY_PRIORITY, 1)
                c.configure(f, null, null, 0)
                c.start()
                s.dec = c; s.soft = soft
                criados++
                Log.i(TAG, "decoder ${c.name} (${if (soft) "software" else "hardware"})")
                return c
            } catch (e: Exception) {
                try { c?.release() } catch (_: Exception) {}
            }
        }
        return null
    }

    // Frees the decoder (the container stays open): the next frame opens it again.
    @JvmStatic
    fun soltarDecoder(h: Long) {
        val s = sessoes[h] ?: return
        s.prefetch?.close(); s.prefetch = null
        val dec = s.dec
        s.dec = null; s.pedido++
        s.memoryAudit?.snapshot("pre-decoder-release")
        try { dec?.stop() } catch (_: Exception) {}
        try { dec?.release() } catch (_: Exception) {}
        // A SurfaceTexture can retain full-resolution graphic buffers after codec release.
        // Never carry those buffers into another hardware admission attempt.
        s.gpu?.soltar(); s.gpu = null
        s.amostra = null
        s.memoryAudit?.snapshot("post-codec-and-gpu-release")
        if (dec != null) Log.i(TAG, "decoder and GPU released: session=$h")
    }

    @JvmStatic
    fun fechar(h: Long) {
        soltarDecoder(h)
        sessoes[h]?.fonte?.close()
        sessoes.remove(h)?.let { it.ex.release(); it.fonte.soltarCache(); it.gpu?.soltar(); it.gpu = null; it.memoryAudit?.snapshot("post-source-release") }
    }

    // The keyframe nearest `tMs` (closest=true) or the one at/before it.
    // Result: 8 bytes pts ms (big endian) + 1 byte "signature valid" + SW*SH signature bytes (grey, the C side
    // normalises) + the JPEG (only when `jpeg`). null on failure. The same keyframe twice in a row is not
    // decoded again (matching probes the same spot from neighbouring times).
    // 1 when the last quadro/quadroExato of this session failed because a READ failed (back off), 0 otherwise
    // (a frame that would not decode: skip it).
    @JvmStatic
    fun falhaRede(h: Long): Int {
        val f = sessoes[h]?.fonte ?: return 0
        return if (f.recusado) 2 else if (f.erroRede) 1 else 0   // 2 = refused/silent: stop the title
    }

    private fun motivo(s: Sessao, tMs: Long, m: String): ByteArray? {
        Log.w(TAG, "no frame at ${tMs / 1000}s: $m${if (s.fonte.erroRede) " (read failed)" else ""}")
        return null
    }

    @JvmStatic
    fun quadro(h: Long, tMs: Long, closest: Boolean, jpeg: Boolean): ByteArray? {
        val t0 = android.os.SystemClock.elapsedRealtime()
        try { return quadroUm(h, tMs, closest, jpeg) } finally { msQuadros += android.os.SystemClock.elapsedRealtime() - t0 }
    }

    private fun quadroUm(h: Long, tMs: Long, closest: Boolean, jpeg: Boolean): ByteArray? {
        val s = sessoes[h] ?: return null
        s.fonte.erroRede = false
        try {
            s.fonte.fase = F_SEEK; s.fonte.alvoMs = tMs
            s.ex.seekTo(tMs * 1000, if (closest) MediaExtractor.SEEK_TO_CLOSEST_SYNC else MediaExtractor.SEEK_TO_PREVIOUS_SYNC)
            val st = s.ex.sampleTime
            if (st < 0) return motivo(s, tMs, "seek found no sample")
            if (st == s.ultimoPts && !jpeg) s.ultimoRes?.let { return it }
            val dec = decoder(s) ?: return motivo(s, tMs, "no decoder")
            dec.flush()
            val info = MediaCodec.BufferInfo()
            var fed = 0
            var eos = false
            val ini = System.currentTimeMillis()
            while (System.currentTimeMillis() - ini < 4000) {
                if (!eos && fed < 40) {
                    val ib = dec.dequeueInputBuffer(2000)
                    if (ib >= 0) {
                        // Software decoders hand the frame out on end-of-stream; Nvidia's drops it there, so it is fed on.
                        // HEVC gets ONE more sample first: Android's software HEVC decoder sometimes ended without a
                        // frame on the keyframe alone (4 of 612, 22/10); the first frame out is still the keyframe.
                        val antesDoFim = if (s.mime == "video/hevc" || s.mime == "video/dolby-vision") 2 else 1
                        s.fonte.fase = F_AMOSTRA
                        val n = if (s.soft && fed >= antesDoFim) -1 else s.ex.readSampleData(dec.getInputBuffer(ib)!!, 0)
                        if (n < 0) { dec.queueInputBuffer(ib, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); eos = true }
                        else {
                            if (fed == 0) s.fonte.ajustarJanela(n)   // the keyframe's real size sizes the next reads
                            dec.queueInputBuffer(ib, 0, n, s.ex.sampleTime, 0)
                            fed++
                            // No advance() after the LAST sample fed (software path): on Matroska it reads on through
                            // the audio blocks to the next video frame, past the prefetched window. Measured 23/10:
                            // ~1 extra request per thumbnail (847 requests for 25 batches on a 2 h film).
                            s.fonte.fase = F_AVANCO
                            if (!s.soft || fed < antesDoFim) s.ex.advance()
                        }
                    }
                }
                val ob = dec.dequeueOutputBuffer(info, 2000)
                if (ob >= 0) {
                    var res: ByteArray? = null
                    if (info.size > 0) {
                        val t1 = System.currentTimeMillis()
                        msDecod += t1 - ini                     // seek + sample read (cached) + the decoder's own work
                        dec.getOutputImage(ob)?.use { img -> res = montar(s, img, info.presentationTimeUs / 1000, jpeg) }
                        msMontar += System.currentTimeMillis() - t1   // planes copy + resize + JPEG
                    }
                    dec.releaseOutputBuffer(ob, false)
                    if (res != null) { s.ultimoPts = st; s.ultimoRes = if (jpeg) null else res; return res }
                    if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) return motivo(s, tMs, "decoder ended without a frame (fed $fed)")
                }
            }
            soltarDecoder(h)   // stuck: start clean next time, one step further down the decoder ladder
            if (!s.soft && !s.fonte.erroRede) s.travadas++
            return motivo(s, tMs, "decoder timed out (fed $fed), next try: ${if (s.travadas >= 2) "software" else "normal priority"}")
        } catch (e: Exception) {
            soltarDecoder(h)
            if (e is IllegalStateException || e is MediaCodec.CodecException) s.errosCodec++
            return motivo(s, tMs, "error ${e.javaClass.simpleName}${if (s.errosCodec >= 2) ": decoder given up for this file" else ""}")
        }
    }

    // ---- DIRECT THUMBNAILS FROM THE PLAYING FILE (4K, no usable smaller copy; experimental) ----
    // The playing file is read in multi-range batches with the socket closed after each request (debrid connection
    // limits, see Fonte), decoded by ONE persistent hardware decoder per source onto a SurfaceTexture and scaled on the
    // GPU (GpuMiniatura). The player's own decoder and extractor are never touched.
    @Volatile private var msGpu = 0L
    @Volatile private var msJpeg = 0L
    @Volatile private var nDireto = 0L         // direct thumbnails made
    @Volatile private var nDiretoCache = 0L    // ...of which with no network read during the call (prefetched)
    @Volatile private var msDiretoCache = 0L   // decode + GPU + JPEG time of those
    @Volatile private var nDup = 0L            // outputs obtained by the repeated keyframe (no extra bytes)
    @Volatile private var nAvanco = 0L         // ...that needed the following samples instead
    @Volatile private var nDiretoFalhas = 0L
    @Volatile private var msSeek = 0L            // extractor seek (incl. reading the keyframe from the cache)
    @Volatile private var msEsperaQuadro = 0L    // decoder output -> frame available on the SurfaceTexture

    // Opens the playing file for direct thumbnails. 0 = not possible; the reason is logged. Profiles declined: Dolby
    // Vision without an HDR10/SDR base layer (profile 5), codecs other than H.264/HEVC.
    @JvmStatic
    fun abrirDireto(url: String, cabecalhos: String): Long {
        val h = abrir(url, cabecalhos, false)
        val s = sessoes[h] ?: return 0L
        s.fonte.modoDireto()
        if (s.mime != "video/avc" && s.mime != "video/hevc" && s.mime != "video/dolby-vision") {
            Log.w(TAG, "direct: codec ${s.mime} not supported"); fechar(h); return 0L
        }
        val dv = s.fonte.perfilDv()
        if (dv >= 0) {
            val perfil = dv / 16; val compat = dv % 16
            s.dvProfile = perfil
            // compat 1 = HDR10 base (8.1), 2 = SDR (8.2), 4 = HLG (8.4), 6 = profile 7's HDR10 base layer
            val ok = ThumbnailDolbyVision.supported(dv) || ThumbnailDolbyVision.requiresDolbyColour(dv)
            Log.i(TAG, "direct: Dolby Vision profile $perfil (base-layer compat $compat) ${if (ok) "-> decoding compatible video" else "-> declined"}")
            if (!ok) { fechar(h); return 0L }
            if (compat == 1 || compat == 6) s.hdr = true
        }
        if (s.mime == "video/dolby-vision" && dv < 0) { Log.w(TAG, "direct: Dolby Vision track without its profile: declined"); fechar(h); return 0L }
        return h
    }

    // ---- CPU ROUTE (src/miniatura_motor.c, Namer03's FFmpeg engine in packet mode) ----
    // JNI consumes these on the thumbnail thread immediately after quadroDireto, before another capture/close.
    private val colourGpu=ThreadLocal<DolbyThumbnailGpu>()

    @JvmStatic
    fun converterDolbyGpu(w: Int,h: Int,signal: ByteBuffer,metadata: ByteBuffer,rgba: ByteBuffer): Int {
        if(w !in 2..640 || h !in 2..360 || signal.capacity()!=w*h*12 || metadata.capacity()!=4096 || rgba.capacity()!=w*h*4)return -1
        return try {
            var gpu=colourGpu.get()
            if(gpu==null) {
                val am=NvPlayer.contexto()?.getSystemService(android.content.Context.ACTIVITY_SERVICE) as? android.app.ActivityManager ?: return -1
                val memory=android.app.ActivityManager.MemoryInfo().also {am.getMemoryInfo(it)}
                if(memory.availMem<(384L shl 20))return -1
                gpu=DolbyThumbnailGpu();colourGpu.set(gpu)
            }
            if(gpu.convert(w,h,signal,metadata,rgba))1 else -1
        } catch(e: Exception) {Log.w(TAG,"[dv5-gpu] CPU fallback: ${e.javaClass.simpleName}: ${e.message?.take(180)}");fecharDolbyGpu();-1}
    }
    @JvmStatic
    fun fecharDolbyGpu() {try {colourGpu.get()?.close()}finally {colourGpu.remove()}}

    @JvmStatic
    fun pixelsDireto(h: Long): ByteBuffer? = sessoes[h]?.gpu?.pixelsProntos()
    @JvmStatic
    fun pixelsDimensoes(h: Long): Int = sessoes[h]?.gpu?.let { (it.largura shl 16) or it.altura } ?: 0

    // 1 = HEVC (also Dolby Vision's base layer), 2 = H.264, 0 = the CPU route cannot take this source (keyframes are
    // taken straight from Matroska blocks: needs the index and plain, unlaced blocks).
    @JvmStatic
    fun codecCpu(h: Long): Int {
        val s = sessoes[h] ?: return 0
        if (s.dvProfile == 5) return 3 // HEVC plus mandatory per-frame Dolby RPU colour conversion.
        if (s.fonte.nalTam <= 0) s.fonte.keyframeMs(0)
        if (s.fonte.nalTam <= 0) return 0
        return when (s.mime) { "video/hevc", "video/dolby-vision" -> 1; "video/avc" -> 2; else -> 0 }
    }

    // The keyframe for tMs as one Annex B access unit with the stream's parameter sets in front (the decoder gets no
    // extradata): 8 bytes pts ms (big endian) + the bytes. null when the block is not usable (or a read failed).
    @JvmStatic
    fun pacoteCpu(h: Long, tMs: Long): ByteArray? {
        val s = sessoes[h] ?: return null
        s.fonte.erroRede = false
        val am = s.amostraCpu ?: ByteBuffer.allocateDirect(8 shl 20).also { s.amostraCpu = it }
        val r = try { s.fonte.blocoChave(tMs, am) } catch (e: Exception) { null } ?: return null
        val csd = s.csdAnexoB ?: run {
            val o = ByteArrayOutputStream()
            for (k in arrayOf("csd-0", "csd-1")) if (s.fmt.containsKey(k)) s.fmt.getByteBuffer(k)?.let { b ->
                val d = b.duplicate(); d.position(0); val a = ByteArray(d.remaining()); d.get(a); o.write(a)
            }
            o.toByteArray().also { s.csdAnexoB = it }
        }
        val out = ByteArray(8 + csd.size + r.second)
        val pts = r.first / 1000
        for (k in 0 until 8) out[k] = ((pts shr (8 * (7 - k))) and 0xff).toByte()
        System.arraycopy(csd, 0, out, 8, csd.size)
        val d = am.duplicate(); d.position(0); d.get(out, 8 + csd.size, r.second)
        return out
    }

    @JvmStatic
    fun prepararDecoder(h: Long): Int = try { val s = sessoes[h]; if (s != null && decoderDireto(s) != null) 1 else 0 } catch (e: Exception) { 0 }

    @JvmStatic
    fun decoderPronto(h: Long): Int = if (sessoes[h]?.dec != null) 1 else 0

    @JvmStatic
    fun keyframeMs(h: Long, tMs: Long): Long = try { sessoes[h]?.fonte?.keyframeMs(tMs) ?: -1L } catch (e: Exception) { -1L }

    // The hardware decoder for this source rendering into the GPU step's Surface. Rate-limited like decoder().
    private fun decoderDireto(s: Sessao): MediaCodec? {
        if (s.dvProfile == 5) {
            Log.i(TAG, "[dv5] CPU RPU colour route: Shield surface output was not colour-correct")
            return null
        }
        s.dec?.let { return it }
        if (s.errosCodec >= 2) return null
        synchronized(this) {
            val espera = ultimaCriacao + 2000 - android.os.SystemClock.elapsedRealtime()
            if (espera > 0) ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.PACING).use { Thread.sleep(espera) }
            ultimaCriacao = android.os.SystemClock.elapsedRealtime()
        }
        // Recheck after the rate-limit wait: playback may have resumed meanwhile.
        if (!NvPlayer.miniaturaPodeCriar()) {
            Log.i(TAG, "direct: decoder admission denied: playback active")
            return null
        }
        val amMemory = NvPlayer.contexto()?.getSystemService(android.content.Context.ACTIVITY_SERVICE) as? android.app.ActivityManager
        val memory = android.app.ActivityManager.MemoryInfo()
        amMemory?.getMemoryInfo(memory)
        if ((perfilBits and 512) != 0 && s.memoryAudit == null) s.memoryAudit = ThumbnailMemoryAudit(System.identityHashCode(s))
        s.memoryAudit?.snapshot("pre-gpu-admission")
        val reserve = miniaturaReservaBytes(s.fmt.getInteger(MediaFormat.KEY_WIDTH), s.fmt.getInteger(MediaFormat.KEY_HEIGHT), s.hdr || s.mime != "video/avc")
        Log.i(TAG, "[decoder-admission] source=${System.identityHashCode(s)} width=${s.fmt.getInteger(MediaFormat.KEY_WIDTH)} height=${s.fmt.getInteger(MediaFormat.KEY_HEIGHT)} availableKB=${memory.availMem / 1024} reserveKB=${reserve / 1024} safetyFloorKB=${300 * 1024} recoveryMarginKB=${64 * 1024} requiredKB=${reserve / 1024 + 364 * 1024} decision=${if (miniaturaMemoriaAdmite(memory.availMem, reserve)) "admit" else "deny"}")
        if (!miniaturaMemoriaAdmite(memory.availMem, reserve)) {
            Log.i(TAG, "direct: decoder admission denied: available=${memory.availMem shr 20} MiB reserve=${reserve shr 20} MiB")
            return null
        }
        val g = s.gpu ?: run {
            val am = NvPlayer.contexto()?.getSystemService(android.content.Context.ACTIVITY_SERVICE) as? android.app.ActivityManager
            val info = android.app.ActivityManager.MemoryInfo()
            am?.getMemoryInfo(info)
            val w = miniaturaLargura(am?.isLowRamDevice == true, info.totalMem, perfilBits, perfilLargura)
            GpuMiniatura(w, w * 9 / 16).also {
                if (!it.iniciar()) { it.soltar(); Log.w(TAG, "direct: GPU step failed to start"); s.errosCodec = 2; return null }
                Log.i(TAG, "direct: thumbnail output ${it.largura}x${it.altura}")
                s.gpu = it
            }
        }
        val mime = if (s.mime == "video/dolby-vision") "video/hevc" else s.mime
        val f = MediaFormat(s.fmt)
        if ((perfilBits and 131072)!=0 && s.dvProfile==7) {
            for (key in arrayOf("csd-0","csd-1")) if (f.containsKey(key)) f.getByteBuffer(key)?.let { original ->
                val view=original.duplicate(); view.position(0)
                val copy=ByteBuffer.allocate(view.remaining()); copy.put(view); copy.flip()
                val r=ThumbnailHevcBaseLayer.filter(copy)
                if(r!=null) { f.setByteBuffer(key,copy); Log.i(TAG,"[hevc-base-layer] config=$key removedBytes=${r.removedBytes}") }
            }
        }
        f.setString(MediaFormat.KEY_MIME, mime)
        if (s.mime == "video/dolby-vision") f.setInteger(MediaFormat.KEY_PROFILE, MediaCodecInfo.CodecProfileLevel.HEVCProfileMain10)
        // the hardware decoder that takes THIS format (profile, bit depth, size), never a software one
        val lista = MediaCodecList(MediaCodecList.REGULAR_CODECS)
        val nomes = lista.codecInfos.filter { ci ->
            !ci.isEncoder && ci.supportedTypes.any { it.equals(mime, true) } &&
                !ci.name.startsWith("OMX.google.") && !ci.name.startsWith("c2.android.") &&
                (android.os.Build.VERSION.SDK_INT < 29 || ci.isHardwareAccelerated) &&
                try { ci.getCapabilitiesForType(mime).videoCapabilities.isSizeSupported(s.fmt.getInteger(MediaFormat.KEY_WIDTH), s.fmt.getInteger(MediaFormat.KEY_HEIGHT)) } catch (e: Exception) { false }
        }.map { it.name }
        for (nome in nomes) {
            var c: MediaCodec? = null
            try {
                if (!NvPlayer.miniaturaPodeCriar()) return null
                c = MediaCodec.createByCodecName(nome)
                if (s.travadas == 0) f.setInteger(MediaFormat.KEY_PRIORITY, 1) else f.removeKey(MediaFormat.KEY_PRIORITY)
                if ((perfilBits and 131072)!=0 && android.os.Build.VERSION.SDK_INT>=30) {
                    val caps=lista.codecInfos.first { it.name==nome }.getCapabilitiesForType(mime)
                    val lowLatency=caps.isFeatureSupported(MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency)
                    if(lowLatency) f.setInteger(MediaFormat.KEY_LOW_LATENCY,1)
                    Log.i(TAG,"[hardware-headroom] decoder=$nome lowLatencySupported=$lowLatency")
                }
                s.hardwareBaselineAvailable=memory.availMem; s.warmFrames=0
                hardwareHeadroom(s,"pre-configure")
                s.memoryAudit?.snapshot("pre-configure-$nome")
                c.configure(f, g.surface, null, 0)
                s.memoryAudit?.snapshot("post-configure")
                c.start()
                s.memoryAudit?.snapshot("post-start")
                hardwareHeadroom(s,"post-start")
                s.dec = c; s.soft = false
                criados++
                Log.i(TAG, "direct: decoder $nome (hardware, ${if (s.hdr) "HDR/PQ" else "SDR"}, ${s.fmt.getInteger(MediaFormat.KEY_WIDTH)}x${s.fmt.getInteger(MediaFormat.KEY_HEIGHT)})")
                return c
            } catch (e: Exception) {
                Log.w(TAG, "direct: decoder $nome refused: ${e.javaClass.simpleName}")
                try { c?.release() } catch (_: Exception) {}
                s.memoryAudit?.snapshot("failed-configure-released")
            }
        }
        s.errosCodec++
        return null
    }

    private fun hardwareHeadroom(s: Sessao, phase: String) {
        if ((perfilBits and 131072) == 0) return
        val am = NvPlayer.contexto()?.getSystemService(android.content.Context.ACTIVITY_SERVICE) as? android.app.ActivityManager ?: return
        val mem = android.app.ActivityManager.MemoryInfo().also { am.getMemoryInfo(it) }
        Log.i(TAG,"[hardware-headroom] phase=$phase frames=${s.warmFrames} availableKB=${mem.availMem/1024} systemDropKB=${(s.hardwareBaselineAvailable-mem.availMem)/1024} playerBytes=${NvPlayer.thumbnailAllocatedBytes}")
    }
    private fun baseLayerPacket(s: Sessao, data: ByteBuffer): Int? {
        if ((perfilBits and 131072)==0 || s.dvProfile!=7) return data.limit()
        val result = ThumbnailHevcBaseLayer.filter(data) ?: return null
        if (s.warmFrames < 3) Log.i(TAG,"[hevc-base-layer] inputBytes=${result.inputBytes} outputBytes=${result.outputBytes} removedBytes=${result.removedBytes} removedNals=${result.removedNals}")
        return result.outputBytes
    }

    private fun visivel(s: Sessao, of: MediaFormat) {
        var w = of.getInteger(MediaFormat.KEY_WIDTH); var h = of.getInteger(MediaFormat.KEY_HEIGHT)
        if (of.containsKey("crop-right") && of.containsKey("crop-left")) w = of.getInteger("crop-right") - of.getInteger("crop-left") + 1
        if (of.containsKey("crop-bottom") && of.containsKey("crop-top")) h = of.getInteger("crop-bottom") - of.getInteger("crop-top") + 1
        val rot = if (s.fmt.containsKey(MediaFormat.KEY_ROTATION)) s.fmt.getInteger(MediaFormat.KEY_ROTATION) else 0
        if (rot == 90 || rot == 270) { val t = w; w = h; h = t }
        s.visW = w; s.visH = h
        if (s.dvProfile == 5) Log.i(TAG, "[dv5] output format: $of")
        if (of.containsKey(MediaFormat.KEY_COLOR_TRANSFER) && of.getInteger(MediaFormat.KEY_COLOR_TRANSFER) == MediaFormat.COLOR_TRANSFER_ST2084) s.hdr = true
    }

    // End of sequence after the keyframe, then the SAME keyframe again (1 us later): the second random-access picture
    // makes the decoder output the first at once (HEVC C.5.2.2 / H.264 C.4.4 bumping), with no extra bytes read and no
    // reliance on end-of-stream (Nvidia's decoder drops the last frame there). Only the first picture is kept, by its
    // exact timestamp. Samples from the extractor are Annex B (start codes) for both codecs.
    private val EOS_HEVC = byteArrayOf(0, 0, 0, 1, 0x48, 0x01)
    private val EOS_AVC = byteArrayOf(0, 0, 0, 1, 0x0A)

    // The keyframe the extractor lands on for tMs, as a 480x270 JPEG made on the GPU. Same reply format as quadro().
    @JvmStatic
    fun quadroDireto(h: Long, tMs: Long): ByteArray? {
        val s = sessoes[h] ?: return null
        val t0 = android.os.SystemClock.elapsedRealtime()
        val req0 = s.fonte.lidos
        s.fonte.erroRede = false
        val pedido = ++s.pedido
        var timelineDecode: AutoCloseable? = null
        try {
            s.fonte.fase = F_SEEK; s.fonte.alvoMs = tMs
            s.fonte.maxLido = 0
            s.fonte.fimAtual = s.fonte.fimBlocoPara(tMs)
            val cap = maxOf(8 shl 20, s.amostra?.capacity() ?: 0)
            val am = s.amostra?.takeIf { it.capacity() >= cap } ?: ByteBuffer.allocateDirect(cap).also { s.amostra = it }
            val ts0 = android.os.SystemClock.elapsedRealtime()
            // the keyframe straight from its block (only its own bytes were fetched); the extractor when that fails
            val bloco = try { s.fonte.blocoChave(tMs, am, (perfilBits and 131072)!=0 && s.dvProfile==7) } catch (e: Exception) { null }
            val st: Long
            var n: Int
            if (bloco != null) {
                st = bloco.first; n = bloco.second
            } else {
                s.ex.seekTo(tMs * 1000, MediaExtractor.SEEK_TO_PREVIOUS_SYNC)
                st = s.ex.sampleTime
                if (st < 0) return motivo(s, tMs, "direct: seek found no sample")
                if (s.ex.sampleFlags and MediaExtractor.SAMPLE_FLAG_SYNC == 0) return motivo(s, tMs, "direct: landed on a non-keyframe")
                s.fonte.fase = F_AMOSTRA
                am.clear()
                n = s.ex.readSampleData(am, 0)
                if (n <= 0) return motivo(s, tMs, "direct: keyframe read failed")
                s.fonte.registrarExcesso(s.fonte.fimAtual)   // how far past the keyframe's block the parser read
            }
            am.position(0); am.limit(n)
            // The indexed path already filtered during its bulk-copy conversion.
            // Only the rare extractor fallback still needs Annex B boundary discovery.
            if (bloco == null) n = baseLayerPacket(s, am) ?: return motivo(s,tMs,"invalid HEVC base-layer packet")
            msSeek += android.os.SystemClock.elapsedRealtime() - ts0
            s.fonte.ajustarJanela(n)
            val dec = s.dec ?: return motivo(s, tMs, "direct: decoder released; explicit admission required")
            val g = s.gpu ?: return null
            timelineDecode = ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.DECODE_ONLY)
            dec.flush()
            g.descartarPendentes()
            val eos = if (s.mime == "video/avc") EOS_AVC else EOS_HEVC
            val info = MediaCodec.BufferInfo()
            var enviados = 0          // 0 keyframe, 1 its repeat, then following samples (fallback)
            var seguintes = 0
            var tDecod = 0L
            val ini = android.os.SystemClock.elapsedRealtime()
            var ultimaSaida = ini
            while (android.os.SystemClock.elapsedRealtime() - ini < 3000) {
                if (pedido != s.pedido) return null
                val parado = android.os.SystemClock.elapsedRealtime() - ultimaSaida
                if (enviados < 2 || (parado > 400 && seguintes < 8)) {
                    val ib = dec.dequeueInputBuffer(5000)
                    if (ib >= 0) {
                        val buf = dec.getInputBuffer(ib)!!
                        buf.clear()
                        if (enviados < 2) {
                            val dup = am.duplicate(); dup.position(0); dup.limit(n)
                            if (buf.remaining() < n + eos.size) { dec.queueInputBuffer(ib, 0, 0, 0, 0); return motivo(s, tMs, "direct: keyframe bigger than the input buffer") }
                            buf.put(dup); buf.put(eos)
                            dec.queueInputBuffer(ib, 0, n + eos.size, st + enviados, 0)
                        } else {
                            // the repeat gave nothing: feed what follows the keyframe, as the copy path does (the
                            // advance past the keyframe happens only now: it reads the next blocks)
                            if (seguintes == 0) {
                                if (bloco != null) s.ex.seekTo(st, MediaExtractor.SEEK_TO_PREVIOUS_SYNC)
                                s.fonte.fase = F_AVANCO; s.ex.advance()
                            }
                            s.fonte.fase = F_AMOSTRA
                            val r = s.ex.readSampleData(buf, 0)
                            if (r < 0) { dec.queueInputBuffer(ib, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); seguintes = 99 }
                            else { dec.queueInputBuffer(ib, 0, r, s.ex.sampleTime + 2, 0); s.fonte.fase = F_AVANCO; s.ex.advance(); seguintes++ }
                            ultimaSaida = android.os.SystemClock.elapsedRealtime()
                        }
                        enviados++
                    }
                }
                val ob = dec.dequeueOutputBuffer(info, 10000)
                if (ob == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) { visivel(s, dec.outputFormat); continue }
                if (ob < 0) continue
                ultimaSaida = android.os.SystemClock.elapsedRealtime()
                if (info.presentationTimeUs != st || info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) {
                    dec.releaseOutputBuffer(ob, false)
                    if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) break
                    continue
                }
                if (s.visW <= 0) visivel(s, dec.outputFormat)
                tDecod = android.os.SystemClock.elapsedRealtime() - ini
                dec.releaseOutputBuffer(ob, true)
                val g0 = g.msGpu
                val tw0 = android.os.SystemClock.elapsedRealtime()
                if (!g.capturar(st, s.visW, s.visH, s.hdr, 1500)) return motivo(s, tMs, "direct: the GPU step got no frame")
                msEsperaQuadro += android.os.SystemClock.elapsedRealtime() - tw0 - (g.msGpu - g0)
                if (pedido != s.pedido) return null
                msGpu += g.msGpu - g0
                if (seguintes == 0) nDup++ else nAvanco++
                val j0 = android.os.SystemClock.elapsedRealtime()
                // header + EMPTY signature (direct thumbnails are not matched against anything; assinaturaDe would
                // allocate a 0.5 MB pixel array per thumbnail on the heap the player shares)
                val out = s.saida ?: ByteArrayOutputStream(48 * 1024).also { s.saida = it }
                out.reset()
                for (k in 7 downTo 0) out.write((((st / 1000) shr (8 * k)) and 0xff).toInt())
                out.write(0)
                out.write(ByteArray(SW * SH))
                g.bmp.compress(Bitmap.CompressFormat.JPEG, 80, out)
                val tj = android.os.SystemClock.elapsedRealtime() - j0
                msJpeg += tj
                msDecod += tDecod
                nDireto++
                s.warmFrames++
                if(s.warmFrames==1 || s.warmFrames%30==0) hardwareHeadroom(s,"decoded")
                s.memoryAudit?.frame()
                val total = android.os.SystemClock.elapsedRealtime() - t0
                if (s.fonte.lidos == req0) { nDiretoCache++; msDiretoCache += total }
                if (nDireto <= 5 || nDireto % 25 == 0L)
                    Log.i(TAG, "direct frame ${st / 1000} ms for ${tMs} ms: keyframe ${n / 1024} KB, decode $tDecod ms (${if (seguintes == 0) "repeat" else "+$seguintes samples"}), " +
                        "gpu ${g.msGpu - g0} ms, jpeg $tj ms, total $total ms, network ${if (s.fonte.lidos == req0) "none" else "${(s.fonte.lidos - req0) / 1024} KB"}" +
                        ", ${s.visW}x${s.visH}${if (s.hdr) " PQ" else ""}")
                if (dumpar > 0) dumparJpeg(st / 1000, out.toByteArray())
                return out.toByteArray()
            }
            nDiretoFalhas++
            soltarDecoder(h)
            if (!s.fonte.erroRede) s.travadas++
            if (s.travadas >= 2) s.errosCodec = 2      // stuck twice (also at normal priority): no more decoders for this file
            return motivo(s, tMs, "direct: decoder timed out (sent $enviados, following $seguintes)")
        } catch (e: Exception) {
            nDiretoFalhas++
            soltarDecoder(h)
            if (e is IllegalStateException || e is MediaCodec.CodecException) s.errosCodec++
            return motivo(s, tMs, "direct: error ${e.javaClass.simpleName}${if (s.errosCodec >= 2) ": decoder given up for this file" else ""}")
        } finally {
            timelineDecode?.close()
            msQuadros += android.os.SystemClock.elapsedRealtime() - t0
        }
    }

    // Debug (debug.nuvio.miniatura bit 16): the first direct thumbnails written to the app's external files folder, for
    // checking colours against the screen (adb pull /sdcard/Android/data/<package>/files/miniaturas-teste).
    @Volatile private var dumpar = 0
    @Volatile var pastaTeste: java.io.File? = null
    private fun dumparJpeg(ptsMs: Long, r: ByteArray) {
        val d = pastaTeste ?: return
        dumpar--
        try {
            d.mkdirs()
            val jpg = r.copyOfRange(9 + SW * SH, r.size)
            java.io.File(d, "direto_%07d.jpg".format(ptsMs)).writeBytes(jpg)
        } catch (_: Exception) {}
    }

    @JvmStatic
    fun diagDireto(): String {
        val f = sessoes.values.firstOrNull { it.fonte.direto }?.fonte
        return "direct: $nDireto thumbnails (repeat-keyframe $nDup, needed following samples $nAvanco, failures $nDiretoFalhas), " +
            "seek %.1f s, gpu+readback %.1f s, frame wait %.1f s, jpeg %.1f s, prefetched-only %d in %.1f s (%.0f ms each)".format(
                msSeek / 1000.0, msGpu / 1000.0, msEsperaQuadro / 1000.0, msJpeg / 1000.0, nDiretoCache, msDiretoCache / 1000.0,
                if (nDiretoCache > 0) msDiretoCache.toDouble() / nDiretoCache else 0.0) +
            (if (f != null) " | exact ranges ${f.nExatos}, guessed ${f.nChutes}, probes ${f.nSondas}, from blocks ${f.nBlocos}, margin ${f.margem / 1024} KB" else "")
    }

    // The frame AT `tMs` (not the keyframe): decode forward from the keyframe before it and keep the first frame
    // whose time is >= tMs. Signature only. For the fine pass of the timeline check, on the small copy only
    // (a 720p GOP is cheap to run through; a 4K one is not).
    @JvmStatic
    fun quadroExato(h: Long, tMs: Long, jpeg: Boolean): ByteArray? {
        val s = sessoes[h] ?: return null
        s.fonte.erroRede = false
        try {
            s.fonte.fase = F_SEEK; s.fonte.alvoMs = tMs
            s.ex.seekTo(tMs * 1000, MediaExtractor.SEEK_TO_PREVIOUS_SYNC)
            if (s.ex.sampleTime < 0) return motivo(s, tMs, "exact: seek found no sample")
            s.fonte.fase = F_EXATO
            val dec = decoder(s) ?: return null
            dec.flush()
            s.ultimoPts = Long.MIN_VALUE
            val info = MediaCodec.BufferInfo()
            var fed = 0
            var eos = false
            val ini = System.currentTimeMillis()
            while (System.currentTimeMillis() - ini < 15000) {   // forward decode through a GOP: reads queue behind the batches
                if (!eos) {
                    val ib = dec.dequeueInputBuffer(2000)
                    if (ib >= 0) {
                        val n = if (fed >= 400) -1 else s.ex.readSampleData(dec.getInputBuffer(ib)!!, 0)
                        if (n < 0) { dec.queueInputBuffer(ib, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM); eos = true }
                        else { dec.queueInputBuffer(ib, 0, n, s.ex.sampleTime, 0); s.ex.advance(); fed++ }
                    }
                }
                val ob = dec.dequeueOutputBuffer(info, 2000)
                if (ob >= 0) {
                    var res: ByteArray? = null
                    val pts = info.presentationTimeUs / 1000
                    if (info.size > 0 && pts >= tMs) dec.getOutputImage(ob)?.use { img -> res = montar(s, img, pts, jpeg) }
                    dec.releaseOutputBuffer(ob, false)
                    if (res != null) return res
                    if (info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM != 0) return null
                }
            }
            soltarDecoder(h)
            return null
        } catch (e: Exception) {
            soltarDecoder(h)
            if (e is IllegalStateException || e is MediaCodec.CodecException) s.errosCodec++
            return null
        }
    }

    // The frame on screen (NvPlayer.assinaturaTela), in the reply format of quadro() without a JPEG. Luma from RGB
    // (Rec. 709 weights), the same crop and 32x18 grid as montar(); the C side removes the mean and normalises, so
    // the RGB-vs-Y range difference does not matter.
    @JvmStatic
    fun tela(): ByteArray? = NvPlayer.assinaturaTela()

    fun assinaturaDe(b: Bitmap, posMs: Long): ByteArray {
        val w = b.width; val h = b.height
        val px = IntArray(w * h)
        b.getPixels(px, 0, w, 0, 0, w, h)
        val sig = ByteArray(SW * SH)
        val x0 = w / 10; val x1 = w - w / 10; val y0 = h * 16 / 100; val y1 = h - h * 16 / 100
        for (j in 0 until SH) for (i in 0 until SW) {
            val ax = x0 + (x1 - x0) * i / SW; val bx = maxOf(ax + 1, x0 + (x1 - x0) * (i + 1) / SW)
            val ay = y0 + (y1 - y0) * j / SH; val by = maxOf(ay + 1, y0 + (y1 - y0) * (j + 1) / SH)
            var sum = 0; var n = 0
            for (yy in ay until by) for (xx in ax until bx) {
                val c = px[yy * w + xx]
                sum += (((c shr 16) and 0xff) * 54 + ((c shr 8) and 0xff) * 183 + (c and 0xff) * 19) shr 8
                n++
            }
            sig[j * SW + i] = (if (n > 0) sum / n else 0).toByte()
        }
        val out = ByteArrayOutputStream(600)
        for (k in 7 downTo 0) out.write(((posMs shr (8 * k)) and 0xff).toInt())
        out.write(1)
        out.write(sig)
        return out.toByteArray()
    }

    private fun montar(s: Sessao, img: Image, ptsMs: Long, jpeg: Boolean): ByteArray {
        // The VISIBLE area: decoders pad (1920x1088 for 1080p) and report the picture in cropRect.
        val cr = img.cropRect
        val cx = if (cr.width() > 0) cr.left else 0
        val cy = if (cr.height() > 0) cr.top else 0
        val w = if (cr.width() > 0) cr.width() else img.width
        val h = if (cr.height() > 0) cr.height() else img.height
        val py = img.planes[0]; val pu = img.planes[1]; val pv = img.planes[2]
        // Each plane copied ONCE into a reused array, then plain array reads: ByteBuffer.get() per sample cost ~0.4 s of
        // CPU per 480x270 thumbnail on the Shield (23/10: frames 345 s vs network 108 s on a 2 h film).
        fun copia(b: java.nio.ByteBuffer, atual: ByteArray): ByteArray {
            val d = b.duplicate(); d.position(0)
            val arr = if (atual.size >= d.limit()) atual else ByteArray(d.limit())
            d.get(arr, 0, d.limit())
            return arr
        }
        s.yArr = copia(py.buffer, s.yArr)
        val ya8 = s.yArr
        val rs = py.rowStride; val ps = py.pixelStride
        val crs = pu.rowStride; val cps = pu.pixelStride
        val off = if (img.format == 0x36) 1 else 0   // YCBCR_P010 (10-bit in 16): the high byte, little endian
        val pq = s.hdr
        fun y(x: Int, yy: Int): Int { val v = ya8[(yy + cy) * rs + (x + cx) * ps + off].toInt() and 0xff; return if (pq) pqLut[v] else v }
        // signature: crop 10 % left/right, 16 % top/bottom (letterbox), box-average to 32x18
        val sig = ByteArray(SW * SH)
        val x0 = w / 10; val x1 = w - w / 10; val y0 = h * 16 / 100; val y1 = h - h * 16 / 100
        for (j in 0 until SH) for (i in 0 until SW) {
            val ax = x0 + (x1 - x0) * i / SW; val bx = x0 + (x1 - x0) * (i + 1) / SW
            val ay = y0 + (y1 - y0) * j / SH; val byy = y0 + (y1 - y0) * (j + 1) / SH
            var sum = 0; var n = 0
            var yy = ay
            while (yy < byy) { var xx = ax; while (xx < bx) { sum += y(xx, yy); n++; xx += 2 }; yy += 2 }
            sig[j * SW + i] = (if (n > 0) sum / n else 0).toByte()
        }
        val out = ByteArrayOutputStream(if (jpeg) 16384 else 600)
        for (k in 7 downTo 0) out.write(((ptsMs shr (8 * k)) and 0xff).toInt())
        out.write(1)
        out.write(sig)
        if (jpeg) {
            val outputW = when (val chosen = perfilLargura) { 320, 480, 640 -> chosen; else -> TW }
            val outputH = outputW * 9 / 16
            s.uArr = copia(pu.buffer, s.uArr); s.vArr = copia(pv.buffer, s.vArr)
            val ua8 = s.uArr; val va8 = s.vArr
            // The picture keeps ITS shape inside the 16:9 thumbnail, with black bars, as it looks on the TV. Measured
            // 22/10: a 1280x536 (2.39:1) copy squeezed into 480x270 stretched everything vertically.
            val px = IntArray(outputW * outputH) { 0xff000000.toInt() }
            val dw: Int; val dh: Int
            if (w.toLong() * outputH >= h.toLong() * outputW) { dw = outputW; dh = ((outputW.toLong() * h / w).toInt() / 2 * 2).coerceIn(2, outputH) }
            else { dh = outputH; dw = ((outputH.toLong() * w / h).toInt() / 2 * 2).coerceIn(2, outputW) }
            val ox = (outputW - dw) / 2; val oy = (outputH - dh) / 2
            // 2x2 samples per output pixel (not one): far less aliasing on edges and text, for 3 more reads each
            val qx = maxOf(1, w / (dw * 4)); val qy = maxOf(1, h / (dh * 4))
            for (ty in 0 until dh) {
                val sy = (ty * h + h / 2) / dh
                val ya = maxOf(0, sy - qy); val yb = minOf(h - 1, sy + qy)
                for (tx in 0 until dw) {
                    val sx = (tx * w + w / 2) / dw
                    val xa = maxOf(0, sx - qx); val xb = minOf(w - 1, sx + qx)
                    val yv = (y(xa, ya) + y(xb, ya) + y(xa, yb) + y(xb, yb) + 2) shr 2
                    val ci = ((sy + cy) / 2) * crs + ((sx + cx) / 2) * cps + off
                    var u = (ua8[ci].toInt() and 0xff) - 128
                    var v = (va8[ci].toInt() and 0xff) - 128
                    if (pq) { u /= 2; v /= 2 }   // the tone curve flattens luma; keep colour from oversaturating
                    val c = (yv - 16).coerceAtLeast(0) * 298
                    val r = ((c + 409 * v + 128) shr 8).coerceIn(0, 255)
                    val g = ((c - 100 * u - 208 * v + 128) shr 8).coerceIn(0, 255)
                    val b = ((c + 516 * u + 128) shr 8).coerceIn(0, 255)
                    px[(oy + ty) * outputW + ox + tx] = (0xff shl 24) or (r shl 16) or (g shl 8) or b
                }
            }
            val bmp = Bitmap.createBitmap(px, outputW, outputH, Bitmap.Config.ARGB_8888)
            bmp.compress(Bitmap.CompressFormat.JPEG, 80, out)
            bmp.recycle()
        }
        return out.toByteArray()
    }
}
