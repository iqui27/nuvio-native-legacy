package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.io.InputStream
import java.io.IOException

class ThumbnailReadTimingTest {
    @Test fun bulkReadsPreserveBytesAndSeparateReadTimeFromOtherWork() {
        var clock = 0L
        var calls = 0
        val input = object : InputStream() {
            override fun read(): Int = error("bulk reads must not become per-byte reads")
            override fun read(b: ByteArray, off: Int, len: Int): Int {
                clock += 5_000_000
                if (calls++ > 0) return -1
                b[off] = 10; b[off + 1] = 20
                return 2
            }
        }
        val timing = ThumbnailReadTiming(input) { clock }
        val bytes = ByteArray(4)
        assertEquals(2, timing.read(bytes))
        clock += 100_000_000 // local parsing is outside the measured read
        assertEquals(-1, timing.read(bytes))
        assertEquals(10, bytes[0].toInt())
        assertEquals(20, bytes[1].toInt())
        assertEquals(2L, timing.bytesRead)
        assertEquals(10_000_000L, timing.readNanos)
        assertEquals(5_000_000L, timing.firstReadNanos)
    }
    @Test fun eofDoesNotInventAFirstBodyRead() {
        val timing = ThumbnailReadTiming(object : InputStream() {
            override fun read() = -1
        }) { 0L }
        assertEquals(-1, timing.read())
        assertNull(timing.firstReadNanos)
        assertEquals(0L, timing.bytesRead)
    }
    @Test fun failedReadsAreTimedAndCloseIsForwarded() {
        var clock = 0L
        var closed = false
        val timing = ThumbnailReadTiming(object : InputStream() {
            override fun read(): Int { clock += 7; throw IOException("test") }
            override fun close() { closed = true }
        }) { clock }
        try { timing.read(); fail("expected read failure") } catch (_: IOException) { }
        assertEquals(7L, timing.readNanos)
        timing.close()
        assertTrue(closed)
    }
}
