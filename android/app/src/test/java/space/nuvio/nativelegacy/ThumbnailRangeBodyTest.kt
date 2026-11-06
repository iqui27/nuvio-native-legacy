package space.nuvio.nativelegacy

import java.io.ByteArrayInputStream
import java.io.IOException
import org.junit.Assert.*
import org.junit.Test

class ThumbnailRangeBodyTest {
    @Test fun unknownLengthIsBoundedAndExact() {
        fun read(data: ByteArray) = ThumbnailRangeBody.read(ByteArrayInputStream(data), -1, "bytes 10-12/100", 10, 12)
        assertArrayEquals(byteArrayOf(1, 2, 3), read(byteArrayOf(1, 2, 3)))
        for (data in listOf(byteArrayOf(1, 2), byteArrayOf(1, 2, 3, 4))) {
            try { read(data); fail("invalid length accepted") } catch (_: IOException) {}
        }
    }
    @Test fun invalidOrOversizedRangesFail() {
        for (range in listOf(null, "bytes 0-16777216/20000000", "bytes 1-2/100")) {
            try { ThumbnailRangeBody.read(ByteArrayInputStream(byteArrayOf()), -1, range, 0, 20000000); fail() }
            catch (_: IOException) {}
        }
    }
}
