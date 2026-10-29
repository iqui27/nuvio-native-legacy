package space.nuvio.nativelegacy

import android.net.Uri
import android.os.SystemClock
import androidx.media3.common.C
import androidx.media3.common.util.UnstableApi
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.IOException

/** Revalidate stale ordinary HLS manifests without touching the player or its sample queues.
 * Never rewrite sequence numbers, segment URLs or timestamps. Media3 remains responsible for
 * parsing, ordering and genuine origin resets. LL-HLS, ranges, VOD and media bytes pass through.
 */
@androidx.annotation.OptIn(UnstableApi::class)
internal class LivePlaylistDataSource private constructor(
    private val upstream: DataSource,
    private val factory: Factory,
) : DataSource {
    internal data class Snapshot(val sequence: Long, val count: Int, val targetMs: Long) {
        val end: Long get() = sequence + count
    }

    class Factory(
        private val upstream: DataSource.Factory,
        // Separate short-timeout HTTP factory; keep the same authentication/user-agent.
        private val revalidation: DataSource.Factory,
        private val clock: () -> Long = { SystemClock.elapsedRealtime() },
        private val report: (String) -> Unit = {},
        internal val liveTsUri: Uri? = null,
    ) : DataSource.Factory {
        private data class Seen(var snapshot: Snapshot, var progressAt: Long, var retryAt: Long = -60_000L)
        private val seen = LinkedHashMap<String, Seen>()
        private var budgetAt = -60_000L
        private var retries = 0
        override fun createDataSource(): DataSource = LivePlaylistDataSource(upstream.createDataSource(), this)

        @Synchronized private fun needsRefresh(key: String, next: Snapshot): Boolean {
            val now = clock()
            val old = seen[key]
            if (old == null) {
                if (seen.size >= 16) seen.remove(seen.keys.first())
                seen[key] = Seen(next, now)
                return false
            }
            if (next.sequence >= old.snapshot.sequence && next.end > old.snapshot.end) {
                old.snapshot = next
                old.progressAt = now
                return false
            }
            // One unchanged poll is normal. Allow at least 1.5 target durations.
            val reset = next.end < old.snapshot.sequence
            val stale = now - old.progressAt >= (old.snapshot.targetMs * 3 / 2).coerceIn(6_000L, 60_000L)
            if (!reset && !stale) return false
            if (now - old.retryAt < 15_000L) return false
            if (now - budgetAt >= 60_000L) { budgetAt = now; retries = 0 }
            if (retries >= 2) return false
            old.retryAt = now
            retries++
            return true
        }

        private fun refresh(spec: DataSpec, original: ByteArray, snapshot: Snapshot, resolved: Uri?): ByteArray {
            if (!needsRefresh(spec.uri.toString(), snapshot)) return original
            val requestHeaders = spec.httpRequestHeaders.filterKeys {
                !it.equals("If-None-Match", true) && !it.equals("If-Modified-Since", true) &&
                    !it.equals("Cache-Control", true) && !it.equals("Pragma", true)
            } + mapOf("Cache-Control" to "no-cache, max-age=0", "Pragma" to "no-cache")
            val source = revalidation.createDataSource()
            val started = clock()
            try {
                // Keep the exact URI (including signatures), headers and request context.
                source.open(spec.buildUpon().setHttpRequestHeaders(requestHeaders).build())
                val bytes = readBounded(source) {
                    if (clock() - started > 3_000L) throw IOException("Playlist revalidation deadline")
                }
                val fresh = parse(bytes)
                if (source.uri == resolved && fresh != null && fresh.sequence >= snapshot.sequence && fresh.end > snapshot.end) {
                    synchronized(this) {
                        seen[spec.uri.toString()]?.let {
                            if (fresh.sequence >= it.snapshot.sequence && fresh.end > it.snapshot.end) {
                                it.snapshot = fresh
                                it.progressAt = clock()
                            }
                        }
                    }
                    report("[live-playlist] revalidated advanced=true elapsedMs=${clock() - started}")
                    return bytes
                }
                report("[live-playlist] revalidated advanced=false elapsedMs=${clock() - started}")
            } catch (_: IOException) {
                // A failed optimization must not turn a successful normal request into an error.
                report("[live-playlist] revalidation unavailable elapsedMs=${clock() - started}")
            } finally {
                try { source.close() } catch (_: IOException) { }
            }
            return original
        }

        internal fun process(spec: DataSpec, bytes: ByteArray, resolved: Uri?): ByteArray {
            val snapshot = parse(bytes) ?: return bytes
            return refresh(spec, bytes, snapshot, resolved)
        }
    }

    private var tsPacket = ByteArray(188)
    private val queuedPackets = ArrayDeque<ByteArray>()
    private val pesHeaders = mutableMapOf<Int, MutableList<Pair<ByteArray, Int>>>()
    private var packetPosition = 188
    private var timestampOffset = 0L
    private var lastTimestamp = -1L
    private var reconnectTimestamp = false
    private var liveTsSpec: DataSpec? = null
    private var connectionHasData = false
    private var prefix: ByteArrayInputStream? = null
    private var upstreamOpen = false
    private var resolvedUri: Uri? = null
    private var headers: Map<String, List<String>> = emptyMap()

    override fun addTransferListener(listener: TransferListener) = upstream.addTransferListener(listener)
    override fun getUri(): Uri? = resolvedUri
    override fun getResponseHeaders(): Map<String, List<String>> = headers
    override fun open(dataSpec: DataSpec): Long {
        upstreamOpen = true
        val length = upstream.open(dataSpec)
        liveTsSpec = dataSpec.takeIf {
            it.uri == factory.liveTsUri && it.httpMethod == DataSpec.HTTP_METHOD_GET &&
                it.position == 0L && it.length == C.LENGTH_UNSET.toLong()
        }
        connectionHasData = false
        packetPosition = 188
        queuedPackets.clear()
        pesHeaders.clear()
        timestampOffset = 0L
        lastTimestamp = -1L
        reconnectTimestamp = false
        resolvedUri = upstream.uri
        headers = upstream.responseHeaders
        // A live TS connection is one portion of an endless stream, even when
        // the server advertises Content-Length. Keep the extractor loading.
        if (liveTsSpec != null) return C.LENGTH_UNSET.toLong()
        val http = dataSpec.uri.scheme.equals("http", true) || dataSpec.uri.scheme.equals("https", true)
        val manifest = dataSpec.uri.path?.endsWith(".m3u8", true) == true || headers.any { (key, values) ->
            key.equals("Content-Type", true) && values.any {
                it.substringBefore(';').trim().lowercase() in setOf("application/vnd.apple.mpegurl", "application/x-mpegurl", "audio/mpegurl", "audio/x-mpegurl")
            }
        }
        if (!http || !manifest || dataSpec.httpMethod != DataSpec.HTTP_METHOD_GET ||
            dataSpec.position != 0L || dataSpec.length != C.LENGTH_UNSET.toLong()) return length
        val bytes = readBounded(upstream)
        if (bytes.size > MAX_MANIFEST_BYTES) {
            // Oversize manifests remain streaming; neither truncate nor keep an unbounded copy.
            prefix = ByteArrayInputStream(bytes)
            return length
        }
        upstream.close()
        upstreamOpen = false
        val result = factory.process(dataSpec, bytes, resolvedUri)
        prefix = ByteArrayInputStream(result)
        return result.size.toLong()
    }

    override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
        if (length == 0) return 0
        prefix?.let {
            val read = it.read(buffer, offset, length)
            if (read >= 0) return read
            prefix = null
        }
        if (!upstreamOpen) return C.RESULT_END_OF_INPUT
        if (liveTsSpec == null) return upstream.read(buffer, offset, length)
        if (packetPosition == 188) {
            if (queuedPackets.isEmpty()) {
                do {
                    val packet = ByteArray(188)
                    var size = 0
                    while (size < 188) {
                        val count = readLiveTs(packet, size, 188 - size)
                        if (count <= 0) throw IOException("Incomplete live TS packet")
                        size += count
                    }
                    queuedPackets.addLast(packet)
                    rebaseTimestamps(packet)
                    if (queuedPackets.size > 8192) throw IOException("Live TS PES header stalled")
                } while (pesHeaders.isNotEmpty())
            }
            tsPacket = queuedPackets.removeFirst()
            packetPosition = 0
        }
        val count = minOf(length, 188 - packetPosition)
        tsPacket.copyInto(buffer, offset, packetPosition, packetPosition + count)
        packetPosition += count
        return count
    }

    private fun readLiveTs(buffer: ByteArray, offset: Int, length: Int): Int {
        var count = upstream.read(buffer, offset, length)
        val spec = liveTsSpec
        if (count == C.RESULT_END_OF_INPUT && spec != null) {
            if (!connectionHasData) throw IOException("Empty live TS connection")
            upstreamOpen = false
            upstream.close()
            upstreamOpen = true
            // Reconnect the signed origin, not the expiring redirect or a byte
            // range into the old broadcast. Preserve headers and request context.
            upstream.open(spec)
            resolvedUri = upstream.uri
            headers = upstream.responseHeaders
            connectionHasData = false
            reconnectTimestamp = true
            pesHeaders.clear()
            // An HTTP response may end mid-packet; never splice two sessions' packets.
            if (offset != 0) throw IOException("Truncated live TS packet")
            count = upstream.read(buffer, offset, length)
            if (count == C.RESULT_END_OF_INPUT) throw IOException("Empty live TS reconnect")
        }
        if (count > 0) connectionHasData = true
        return count
    }

    // Keep PCR, PTS and DTS on the same clock across independent HTTP sessions.
    // Hold packets until each PES timestamp is complete, including interleaved PIDs.
    private fun rebaseTimestamps(b: ByteArray) {
        if (b[0] != 0x47.toByte()) throw IOException("Invalid live TS sync byte")
        val mask = 0x1ffffffffL
        fun adjusted(raw: Long): Long {
            var value = raw + timestampOffset
            // Unwrap the 33-bit clock relative to the previous sample.
            if (lastTimestamp >= 0) {
                while (value < lastTimestamp - (1L shl 32)) value += 1L shl 33
                while (value > lastTimestamp + (1L shl 32)) value -= 1L shl 33
            }
            if (reconnectTimestamp) {
                if (lastTimestamp >= 0 && value <= lastTimestamp) {
                    timestampOffset += lastTimestamp + 1 - value
                    value = lastTimestamp + 1
                }
                reconnectTimestamp = false
            }
            lastTimestamp = maxOf(lastTimestamp, value)
            return value and mask
        }
        val control = (b[3].toInt() ushr 4) and 3
        var payload = 4
        if (control and 2 != 0) {
            val size = b[4].toInt() and 255
            payload = 5 + size
            if (payload > 188) throw IOException("Invalid live TS adaptation field")
            if (size >= 7 && b[5].toInt() and 0x10 != 0) {
                var raw = 0L
                repeat(4) { raw = (raw shl 8) or (b[6 + it].toLong() and 255) }
                raw = (raw shl 1) or ((b[10].toLong() and 128) shr 7)
                val value = adjusted(raw)
                repeat(4) { b[6 + it] = (value shr (25 - it * 8)).toByte() }
                b[10] = ((b[10].toInt() and 127) or ((value.toInt() and 1) shl 7)).toByte()
            }
        }
        if (control and 1 == 0 || payload >= 188) return
        val pid = ((b[1].toInt() and 31) shl 8) or (b[2].toInt() and 255)
        if (b[1].toInt() and 0x40 != 0) pesHeaders[pid] = mutableListOf()
        val parts = pesHeaders[pid] ?: return
        for (at in payload until 188) {
            parts.add(b to at)
            val header = ByteArray(parts.size) { i -> parts[i].first[parts[i].second] }
            if (header.size == 3 && !header.contentEquals(byteArrayOf(0, 0, 1))) {
                pesHeaders.remove(pid)
                return
            }
            if (header.size < 9) continue
            val flags = header[7].toInt() and 0xc0
            if (flags != 0x80 && flags != 0xc0) {
                pesHeaders.remove(pid)
                return
            }
            if (header.size < if (flags == 0xc0) 19 else 14) continue
            rewritePesTimestamp(header, 9, ::adjusted)
            if (flags == 0xc0) rewritePesTimestamp(header, 14, ::adjusted)
            parts.forEachIndexed { i, (packet, index) -> packet[index] = header[i] }
            pesHeaders.remove(pid)
            return
        }
    }

    private fun rewritePesTimestamp(b: ByteArray, at: Int, adjusted: (Long) -> Long) {
            val raw = ((b[at].toLong() and 14) shl 29) or ((b[at + 1].toLong() and 255) shl 22) or
                ((b[at + 2].toLong() and 254) shl 14) or ((b[at + 3].toLong() and 255) shl 7) or
                ((b[at + 4].toLong() and 254) shr 1)
            val value = adjusted(raw)
            b[at] = ((b[at].toInt() and 0xf1) or (((value shr 30).toInt() and 7) shl 1)).toByte()
            b[at + 1] = (value shr 22).toByte()
            b[at + 2] = ((((value shr 15).toInt() and 127) shl 1) or 1).toByte()
            b[at + 3] = (value shr 7).toByte()
            b[at + 4] = (((value.toInt() and 127) shl 1) or 1).toByte()
    }

    override fun close() {
        prefix = null
        liveTsSpec = null
        queuedPackets.clear()
        pesHeaders.clear()
        connectionHasData = false
        resolvedUri = null
        headers = emptyMap()
        if (upstreamOpen) { upstreamOpen = false; upstream.close() }
    }

    companion object {
        // Only the Xtream continuous live endpoint; never VOD or HLS segments.
        internal fun isLiveTs(uri: Uri): Boolean =
            (uri.scheme.equals("http", true) || uri.scheme.equals("https", true)) &&
                uri.path?.matches(Regex("/live/[^/]+/[^/]+/[^/]+\\.ts", RegexOption.IGNORE_CASE)) == true

        private const val MAX_MANIFEST_BYTES = 256 * 1024
        private fun readBounded(source: DataSource, check: () -> Unit = {}): ByteArray {
            val out = ByteArrayOutputStream()
            val chunk = ByteArray(4096)
            while (out.size() <= MAX_MANIFEST_BYTES) {
                check()
                val count = source.read(chunk, 0, minOf(chunk.size, MAX_MANIFEST_BYTES + 1 - out.size()))
                if (count == C.RESULT_END_OF_INPUT) break
                out.write(chunk, 0, count)
            }
            return out.toByteArray()
        }

        // Only a refresh hint; the bytes are still parsed and validated by Media3 unchanged.
        private fun parse(bytes: ByteArray): Snapshot? {
            if (bytes.size > MAX_MANIFEST_BYTES) return null
            val text = bytes.toString(Charsets.UTF_8)
            if (!text.startsWith("#EXTM3U") || text.contains("#EXT-X-ENDLIST") ||
                text.contains("#EXT-X-PLAYLIST-TYPE:VOD") || text.contains("#EXT-X-PART") ||
                text.contains("#EXT-X-SKIP") || text.contains("CAN-BLOCK-RELOAD=YES")) return null
            var sequence = 0L
            var target = 0L
            var count = 0
            for (raw in text.lineSequence()) {
                val line = raw.trim()
                when {
                    line.startsWith("#EXT-X-MEDIA-SEQUENCE:") -> sequence = line.substringAfter(':').toLongOrNull() ?: return null
                    line.startsWith("#EXT-X-TARGETDURATION:") -> target = line.substringAfter(':').toLongOrNull() ?: return null
                    line.startsWith("#EXTINF:") -> count++
                }
            }
            if (sequence < 0 || target !in 1..60 || count == 0 || sequence > Long.MAX_VALUE - count) return null
            return Snapshot(sequence, count, target * 1000)
        }
    }
}
