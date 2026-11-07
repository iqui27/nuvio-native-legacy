package space.nuvio.nativelegacy

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class ThumbnailMapTest {
    @get:Rule val tmp = TemporaryFolder()

    private fun sample(): ThumbnailMap.Data {
        val n = 500
        val cues = LongArray(n * 3)
        for (k in 0 until n) {
            cues[k * 3] = k * 10_010_000_000L                 // ns, ~10 s apart
            cues[k * 3 + 1] = 4_000L + k * 130_000_000L        // cluster positions
            cues[k * 3 + 2] = (k % 7) * 1_000L + 12            // CueRelativePosition
        }
        val blocks = HashMap<Int, LongArray>()
        for (k in 0 until n step 3) { val ini = cues[k * 3 + 1] + cues[k * 3 + 2]; blocks[k] = longArrayOf(ini, ini + 700_000 + k) }
        return ThumbnailMap.Data(69_675_067_678L, 1, 4, 69_600_000_000L, 69_600_300_000L, cues, blocks)
    }
    private fun head(seed: Int) = ByteArray(65_536) { ((it * 31 + seed) and 0xff).toByte() }

    @Test fun encodeDecodeRoundTripsExactly() {
        val d = sample()
        val back = ThumbnailMap.decode(ThumbnailMap.encode(d))!!
        assertEquals(d.size, back.size); assertEquals(d.trackV, back.trackV); assertEquals(d.nalLength, back.nalLength)
        assertEquals(d.cuesIni, back.cuesIni); assertEquals(d.cuesFim, back.cuesFim)
        assertArrayEquals(d.cues, back.cues)
        assertEquals(d.blocks.keys, back.blocks.keys)
        for ((k, b) in d.blocks) assertArrayEquals(b, back.blocks[k])
        assertTrue("compact: ${ThumbnailMap.encode(d).size} bytes", ThumbnailMap.encode(d).size < 8_000)
    }

    @Test fun onlyTheSameFileCanOpenItsMap() {
        val fp = ThumbnailMap.fingerprint(1_000_000L, head(1))
        val other = ThumbnailMap.fingerprint(1_000_000L, head(2))
        val otherSize = ThumbnailMap.fingerprint(1_000_001L, head(1))
        assertFalse(fp.contentEquals(other)); assertFalse(fp.contentEquals(otherSize))
        val sealed = ThumbnailMap.seal(fp, byteArrayOf(1, 2, 3))
        assertArrayEquals(byteArrayOf(1, 2, 3), ThumbnailMap.open(fp, sealed))
        assertNull(ThumbnailMap.open(other, sealed))
        sealed[sealed.size - 1] = (sealed[sealed.size - 1] + 1).toByte()             // tampered
        assertNull(ThumbnailMap.open(fp, sealed))
    }

    @Test fun idAndBucketRevealNothingOfTheFingerprint() {
        val fp = ThumbnailMap.fingerprint(1_000_000L, head(1))
        val id = ThumbnailMap.id(fp)
        assertEquals(64, id.length)
        assertNotEquals(fp.joinToString("") { "%02x".format(it) }, id)
        assertEquals(4, ThumbnailMap.bucket(id).length)
    }

    @Test fun storeSavesLoadsAndDropsCorruptMaps() {
        val dir = tmp.newFolder("maps")
        val fp = ThumbnailMap.fingerprint(69_675_067_678L, head(3))
        assertNull(ThumbnailMap.load(fp, dir))
        ThumbnailMap.save(fp, sample(), dir)
        val back = ThumbnailMap.load(fp, dir)!!
        assertArrayEquals(sample().cues, back.cues)
        assertNull(ThumbnailMap.load(ThumbnailMap.fingerprint(69_675_067_678L, head(4)), dir))   // another file
        val f = File(dir, ThumbnailMap.id(fp) + ".map")
        f.writeBytes(byteArrayOf(9, 9, 9))
        assertNull(ThumbnailMap.load(fp, dir))
        assertFalse("a corrupt map is deleted", f.exists())
    }
}
