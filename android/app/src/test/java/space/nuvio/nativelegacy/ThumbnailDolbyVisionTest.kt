package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer

class ThumbnailDolbyVisionTest {
    private fun box(type: String, payload: ByteArray, extended: Boolean = false): ByteArray =
        if (extended) ByteBuffer.allocate(16 + payload.size).putInt(1).put(type.toByteArray())
            .putLong(16L + payload.size).put(payload).array()
        else ByteBuffer.allocate(8 + payload.size).putInt(8 + payload.size)
            .put(type.toByteArray()).put(payload).array()

    private fun movie(profile: Int = 8, compat: Int = 1, base: Boolean = true,
                      type: String = "dvvC", extended: Boolean = false): ByteArray {
        val config = byteArrayOf(1, 0, (profile shl 1).toByte(), (if (base) 5 else 4).toByte(), (compat shl 4).toByte())
        val entry = box("dvh1", ByteArray(78) + box(type, config))
        val stsd = box("stsd", ByteBuffer.allocate(8).putInt(0).putInt(1).array() + entry)
        return box("moov", box("trak", box("mdia", box("minf", box("stbl", stsd)))), extended)
    }
    private fun parse(data: ByteArray): Int = ThumbnailDolbyVision.mp4(data.size.toLong()) { pos, count ->
        data.copyOfRange(pos.toInt(), pos.toInt() + count)
    }

    @Test fun readsHdr10ProfileEightAndExtendedBoxes() {
        assertEquals(129, parse(movie()))
        assertEquals(129, parse(movie(extended = true)))
        assertTrue(ThumbnailDolbyVision.supported(parse(movie())))
    }
    @Test fun profileFiveRequiresDolbyColourConversion() {
        assertTrue(ThumbnailDolbyVision.requiresDolbyColour(parse(movie(5, 0))))
        assertFalse(ThumbnailDolbyVision.supported(parse(movie(5, 0))))
        assertFalse(ThumbnailDolbyVision.requiresDolbyColour(parse(movie(8, 1))))
        assertFalse(ThumbnailDolbyVision.requiresDolbyColour(-1))
        assertFalse(ThumbnailDolbyVision.requiresDolbyColour(5 * 16 + 1))
    }
    @Test fun recognizesDvcCAndPreservesProfileFiveRejection() {
        assertEquals(80, parse(movie(5, 0, type = "dvcC")))
        assertFalse(ThumbnailDolbyVision.supported(parse(movie(5, 0, type = "dvcC"))))
        assertTrue(ThumbnailDolbyVision.supported(parse(movie(7, 6, type = "dvcC"))))
    }
    @Test fun rejectsMissingBaseAndUnsupportedCompatibility() {
        assertEquals(-1, parse(movie(base = false)))
        assertFalse(ThumbnailDolbyVision.supported(parse(movie(8, 0))))
        assertFalse(ThumbnailDolbyVision.supported(-1))
    }
    @Test fun tailMetadataBeyondFourGiBSkipsMoviePayload() {
        val ftyp = box("ftyp", "isom0000".toByteArray())
        val mdatSize = (5L shl 30) + 16
        val mdat = ByteBuffer.allocate(16).putInt(1).put("mdat".toByteArray()).putLong(mdatSize).array()
        val tailStart = ftyp.size + mdatSize
        val metadata = movie()
        val chunks = listOf(0L to ftyp, ftyp.size.toLong() to mdat, tailStart to metadata)
        var readBytes = 0
        val parsed = ThumbnailDolbyVision.mp4(tailStart + metadata.size) { pos, count ->
            readBytes += count
            val chunk = chunks.firstOrNull { pos >= it.first && pos + count <= it.first + it.second.size }
            assertNotNull("must never read mdat payload", chunk)
            chunk!!.second.copyOfRange((pos - chunk.first).toInt(), (pos - chunk.first).toInt() + count)
        }
        assertEquals(129, parsed)
        assertTrue(readBytes < 256)
    }
    @Test fun ignoresConfigurationOutsideVideoSampleEntry() {
        assertEquals(-1, parse(box("moov", box("dvvC", byteArrayOf(1,0,16,5,16)))))
        assertEquals(-1, parse(box("mdat", movie())))
    }
    @Test fun truncatedAndOverflowingBoxesFailClosed() {
        val good = movie()
        assertEquals(-1, parse(good.copyOf(good.size - 1)))
        assertEquals(-1, parse(ByteBuffer.allocate(16).putInt(1).put("moov".toByteArray()).putLong(Long.MIN_VALUE).array()))
        assertEquals(-1, parse(ByteBuffer.allocate(8).putInt(4).put("moov".toByteArray()).array()))
        assertEquals(-1, parse(ByteArray(7)))
    }
    @Test fun shortReadsAndExcessiveBoxCountsAreBounded() {
        assertEquals(-1, ThumbnailDolbyVision.mp4(100) { _, _ -> byteArrayOf(1) })
        val many = ByteArray(8 * 1000)
        val empty = box("free", byteArrayOf())
        for (i in 0 until 1000) empty.copyInto(many, i * 8)
        var reads = 0
        assertEquals(-1, ThumbnailDolbyVision.mp4(many.size.toLong()) { pos, count ->
            reads++
            many.copyOfRange(pos.toInt(), pos.toInt() + count)
        })
        assertEquals(512, reads)
    }
}
