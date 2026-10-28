package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer

class ThumbnailMp4IndexTest {
    private fun ints(vararg values: Int) = ByteBuffer.allocate(values.size * 4).also { b -> values.forEach { b.putInt(it) } }.array()
    private fun box(type: String, data: ByteArray) = ints(data.size + 8) + type.toByteArray() + data
    private fun table(type: String, vararg values: Int) = box(type, ints(0, *values))
    private fun fixture(edit: Int = 0, signed: Boolean = false, co64: Boolean = false,
                        sync: IntArray = intArrayOf(1, 3), fragment: Boolean = false,
                        samples: Int = 4, movieScale: Int = 1000, sc: IntArray = intArrayOf(1,2,1)): ByteArray {
        val ftyp = box("ftyp", "isom0000".toByteArray())
        val mdat = box("mdat", byteArrayOf(0,0,0,2,0x26,1, 0,0,0,3,0x02,1,0,
            0,0,0,2,0x26,1, 0,0,0,3,0x02,1,0))
        val start = ftyp.size + 8
        val offsets = if (co64) box("co64", ints(0,2) + ByteBuffer.allocate(16).putLong(start.toLong()).putLong(start + 13L).array())
            else table("stco", 2, start, start + 13)
        val cfg = ByteArray(22).also { it[0] = 1; it[21] = 3 }
        val entry = box("hvc1", ByteArray(78) + box("hvcC", cfg))
        val stsd = box("stsd", ints(0,1) + entry)
        val ctts = if (signed) box("ctts", ints(0x01000000,4,1,100,1,-100,1,-200,1,0)) else byteArrayOf()
        val stbl = box("stbl", stsd + table("stsz", 0,samples,6,7,6,7) + offsets +
            table("stsc", sc.size / 3, *sc) + table("stts", 1,4,1000) +
            table("stss", sync.size, *sync) + ctts)
        val mdhd = box("mdhd", ints(0,0,0,1000,4000))
        val hdlr = box("hdlr", ints(0,0) + "vide".toByteArray())
        val edts = when (edit) {
            1 -> box("edts", table("elst", 1,4000,50,0x10000))
            2 -> box("edts", table("elst", 2,1000,-1,0x10000,4000,50,0x10000))
            3 -> box("edts", table("elst", 1,4000,50,0x20000))
            else -> byteArrayOf()
        }
        val moov = box("moov", box("mvhd", ints(0,0,0,movieScale,4000)) +
            box("trak", edts + box("mdia", mdhd + hdlr + box("minf", stbl))) +
            if (fragment) box("mvex", byteArrayOf()) else byteArrayOf())
        return ftyp + mdat + moov
    }
    internal fun mediaForHttp() = fixture()
    private fun parse(data: ByteArray) = ThumbnailMp4Index.read(data.size.toLong()) { pos,n -> data.copyOfRange(pos.toInt(),pos.toInt()+n) }

    @Test fun realAvcAndHevcWithReorderedFramesMatchFfprobeOffsetsSizesAndPts() {
        for (codec in listOf("avc", "hevc")) {
            val loader = javaClass.classLoader!!
            val media = loader.getResourceAsStream("thumbnail-mp4/$codec.mp4")!!.use { it.readBytes() }
            val expected = loader.getResourceAsStream("thumbnail-mp4/$codec.tsv")!!.bufferedReader().use { it.readLines() }
                .map { line -> line.split('\t').map { it.toLong() } }
            val index = parse(media)!!
            assertEquals(codec, expected.size, index.sizes.size)
            for (i in expected.indices) {
                assertTrue("$codec PTS $i", kotlin.math.abs(expected[i][0] - index.cues[i * 3] / 1000) <= 1)
                assertEquals(expected[i][1], index.cues[i * 3 + 1])
                assertEquals(expected[i][2].toInt(), index.sizes[i])
                val start = index.cues[i * 3 + 1].toInt()
                val sample = media.copyOfRange(start, start + index.sizes[i])
                assertTrue(ThumbnailNal.write(sample, index.nalLength, ByteBuffer.allocate(sample.size * 4)))
            }
        }
    }

    @Test fun exactRangesAndPresentationTimesFromTailMetadata() {
        val result = parse(fixture())!!
        assertEquals(4, result.nalLength)
        assertArrayEquals(intArrayOf(6,6), result.sizes)
        assertArrayEquals(longArrayOf(0,24,0,2_000_000_000,37,0), result.cues)
    }
    @Test fun signedCompositionOffsetsAndEmptyEditUseSeparateTimescales() {
        val ordinary = parse(fixture(signed=true))!!
        assertEquals(100_000_000L, ordinary.cues[0])
        assertEquals(1_800_000_000L, ordinary.cues[3])
        val edited = parse(fixture(edit=2,signed=true))!!
        assertEquals(1_050_000_000L, edited.cues[0])
        assertEquals(2_750_000_000L, edited.cues[3])
        val differentScale = parse(fixture(edit=2, signed=true, movieScale=2000))!!
        assertEquals(550_000_000L, differentScale.cues[0])
        assertEquals(2_250_000_000L, differentScale.cues[3])
    }
    @Test fun co64AndMultipleChunkRunsWork() {
        assertArrayEquals(parse(fixture())!!.cues, parse(fixture(co64=true))!!.cues)
        assertNotNull(parse(fixture(sc=intArrayOf(1,2,1,2,2,1))))
    }
    @Test fun duplicateSyncSamplesAndMismatchedCountsFailClosed() {
        assertNull(parse(fixture(sync=intArrayOf(1,1))))
        assertNull(parse(fixture(samples=5)))
        assertNull(parse(fixture(sc=intArrayOf(1,3,1))))
    }
    @Test fun fragmentedAndUnsupportedRateEditsFailClosed() {
        assertNull(parse(fixture(fragment=true)))
        assertNull(parse(fixture(edit=3)))
    }
    @Test fun truncationMissingMetadataAndShortReadFailClosed() {
        val data = fixture()
        assertNull(parse(data.copyOf(data.size - 1)))
        assertNull(parse(box("ftyp", byteArrayOf())))
        assertNull(ThumbnailMp4Index.read(data.size.toLong()) { _,_ -> byteArrayOf() })
    }
    @Test fun nalConversionIsBoundedAndMalformedInputDoesNotPartiallyWrite() {
        val output = ByteBuffer.allocate(32)
        assertTrue(ThumbnailNal.write(byteArrayOf(2,0x26,1,1,0x42),1,output))
        assertArrayEquals(byteArrayOf(0,0,0,1,0x26,1,0,0,0,1,0x42), output.array().copyOf(output.position()))
        val position = output.position()
        assertFalse(ThumbnailNal.write(byteArrayOf(2,1),1,output))
        assertEquals(position, output.position())
        assertFalse(ThumbnailNal.write(byteArrayOf(0,0,0,2,1,2),4,ByteBuffer.allocate(5)))
    }
}
