package space.nuvio.nativelegacy

import android.net.Uri
import androidx.media3.common.C
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import org.junit.Assert.*
import org.junit.Test
import java.io.IOException

class LiveTsDataSourceTest {
    private class Source : DataSource {
        val requests = mutableListOf<DataSpec>()
        var remaining = 0
        var closes = 0
        var empty = false
        override fun open(spec: DataSpec): Long {
            requests += spec
            remaining = if (empty) 0 else 188
            return remaining.toLong()
        }
        override fun read(buffer: ByteArray, offset: Int, length: Int): Int {
            if (length == 0) return 0
            if (remaining == 0) return C.RESULT_END_OF_INPUT
            val count = minOf(length, remaining)
            repeat(count) { i -> buffer[offset + i] = if ((188 - remaining + i) % 188 == 0) 0x47 else 0x00 }
            remaining -= count
            return count
        }
        override fun close() { closes++ }
        override fun getUri(): Uri = Uri.parse("https://edge.test/redirected.ts")
        override fun getResponseHeaders() = mapOf("Content-Length" to listOf("188"))
        override fun addTransferListener(listener: TransferListener) = Unit
    }

    @Test fun finiteLiveResponsesContinueWithoutExtractorEofOrRanges() {
        val uri = Uri.parse("https://origin.test/live/user/pass/42.ts?signature=keep")
        val upstream = Source()
        val factory = DataSource.Factory { upstream }
        val source = LivePlaylistDataSource.Factory(factory, factory, liveTsUri = uri).createDataSource()
        val spec = DataSpec.Builder().setUri(uri).setHttpRequestHeaders(mapOf("Authorization" to "token")).build()
        assertEquals(C.LENGTH_UNSET.toLong(), source.open(spec))
        val buffer = ByteArray(188)
        repeat(100) { assertEquals(188, source.read(buffer, 0, buffer.size)) }
        assertEquals(0, source.read(buffer, 0, 0))
        assertEquals(100, upstream.requests.size)
        upstream.requests.forEach {
            assertEquals(uri, it.uri)
            assertEquals(0L, it.position)
            assertEquals("token", it.httpRequestHeaders["Authorization"])
        }
        upstream.empty = true
        try {
            source.read(buffer, 0, buffer.size)
            fail("An empty reconnect must fail rather than spin or publish EOF")
        } catch (_: IOException) { }
        assertEquals(101, upstream.requests.size)
        source.close()
        assertEquals(101, upstream.closes)
    }

    @Test fun reconnectWithSplitPesHeadersKeepsMedia3SampleTimesAdvancing() {
        // Include every header split, especially adaptation length 166 (17 payload bytes).
        for (payloadBytes in (1..19).toList() + 176) {
        // One PES per connection: PCR=PTS-DTS lead, PTS=1s, DTS=0.9s.
        val packet = ByteArray(188) { 0xff.toByte() }
        packet[0] = 0x47; packet[1] = 0x41; packet[2] = 0; packet[3] = 0x30
        packet[4] = 7; packet[5] = 0x10
        fun putTimestamp(at: Int, ticks: Long, prefix: Int) {
            packet[at] = (prefix or (((ticks shr 30).toInt() and 7) shl 1) or 1).toByte()
            packet[at + 1] = (ticks shr 22).toByte()
            packet[at + 2] = (((ticks shr 15).toInt() and 127) * 2 + 1).toByte()
            packet[at + 3] = (ticks shr 7).toByte()
            packet[at + 4] = ((ticks.toInt() and 127) * 2 + 1).toByte()
        }
        val pcr = 72_000L
        repeat(4) { packet[6 + it] = (pcr shr (25 - it * 8)).toByte() }
        packet[10] = (((pcr and 1).toInt() shl 7) or 0x7e).toByte(); packet[11] = 0
        byteArrayOf(0, 0, 1, 0xe0.toByte(), 0, 0, 0x80.toByte(), 0xc0.toByte(), 10)
            .copyInto(packet, 12)
        putTimestamp(21, 90_000, 0x30); putTimestamp(26, 81_000, 0x10)
        val first = packet.copyOf()
        first.fill(0xff.toByte(), 12)
        first[4] = (183 - payloadBytes).toByte()
        packet.copyInto(first, 188 - payloadBytes, 12, 12 + payloadBytes)
        val packets = mutableListOf(first)
        var copied = payloadBytes
        while (copied < 19) {
            // Another PID and an adaptation-only packet must not become PES bytes.
            packets += ByteArray(188).also { it[0] = 0x47; it[3] = 0x10 }
            packets += ByteArray(188).also {
                it[0] = 0x47; it[1] = 1; it[3] = 0x20; it[4] = 183.toByte()
            }
            val count = minOf(3, 19 - copied)
            packets += ByteArray(188) { 0xff.toByte() }.also {
                it[0] = 0x47; it[1] = 1; it[2] = 0; it[3] = 0x30
                it[4] = (183 - count).toByte(); it[5] = 0
                packet.copyInto(it, 188 - count, 12 + copied, 12 + copied + count)
            }
            copied += count
        }
        val stream = packets.fold(ByteArray(0)) { bytes, next -> bytes + next }
        val upstream = object : DataSource {
            var position = 0
            override fun open(spec: DataSpec): Long { position = 0; return stream.size.toLong() }
            override fun read(b: ByteArray, o: Int, n: Int): Int {
                if (position == stream.size) return C.RESULT_END_OF_INPUT
                val count = minOf(n, 17, stream.size - position)
                stream.copyInto(b, o, position, position + count); position += count
                return count
            }
            override fun close() = Unit
            override fun getUri() = Uri.parse("https://origin.test/live/u/p/42.ts")
            override fun addTransferListener(listener: TransferListener) = Unit
        }
        val factory = DataSource.Factory { upstream }
        val source = LivePlaylistDataSource.Factory(factory, factory, liveTsUri = upstream.uri).createDataSource()
        source.open(DataSpec(upstream.uri!!))
        val adjuster = androidx.media3.common.util.TimestampAdjuster(0)
        fun timestamp(b: ByteArray, at: Int): Long =
            ((b[at].toLong() and 14) shl 29) or ((b[at + 1].toLong() and 255) shl 22) or
                ((b[at + 2].toLong() and 254) shl 14) or ((b[at + 3].toLong() and 255) shl 7) or
                ((b[at + 4].toLong() and 254) shr 1)
        var previous = -1L
        repeat(3) {
            val bytes = ByteArray(stream.size)
            var size = 0
            while (size < bytes.size) size += source.read(bytes, size, minOf(13, bytes.size - size))
            val pes = mutableListOf<Byte>()
            for (start in bytes.indices step 188) {
                val control = (bytes[start + 3].toInt() ushr 4) and 3
                if (bytes[start + 1].toInt() and 31 != 1 || control and 1 == 0) continue
                val payload = if (control and 2 != 0) 5 + (bytes[start + 4].toInt() and 255) else 4
                pes.addAll(bytes.slice(start + payload until start + 188))
            }
            val header = pes.toByteArray()
            val pts = timestamp(header, 9)
            val sampleUs = adjuster.adjustTsTimestamp(pts)
            assertTrue("Reconnected PTS must advance beyond queued samples", sampleUs > previous)
            assertEquals(9_000L, (pts - timestamp(header, 14)) and 0x1ffffffffL)
            val clock = ((bytes[6].toLong() and 255) shl 25) or
                ((bytes[7].toLong() and 255) shl 17) or ((bytes[8].toLong() and 255) shl 9) or
                ((bytes[9].toLong() and 255) shl 1) or ((bytes[10].toLong() and 128) shr 7)
            assertEquals(18_000L, (pts - clock) and 0x1ffffffffL)
            previous = sampleUs
        }
        source.close()
        }
    }

    @Test fun vodAndHlsSegmentsKeepFiniteLengthAndEof() {
        val live = Uri.parse("https://origin.test/live/u/p/42.ts")
        for (path in listOf("movie/u/p/42.ts", "live/u/p/segment.ts")) {
            val upstream = Source()
            val factory = DataSource.Factory { upstream }
            val source = LivePlaylistDataSource.Factory(factory, factory, liveTsUri = live).createDataSource()
            assertEquals(188L, source.open(DataSpec(Uri.parse("https://origin.test/$path"))))
            assertEquals(188, source.read(ByteArray(188), 0, 188))
            assertEquals(C.RESULT_END_OF_INPUT, source.read(ByteArray(188), 0, 188))
            assertEquals(1, upstream.requests.size)
            source.close()
        }
    }
}
