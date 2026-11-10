package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class ThumbnailHeaderHintsTest {
    @Test fun metadataNeverFillsAShortOrEmptyPayloadList() {
        val hints = thumbnailSeparatedHints(longArrayOf(30000), longArrayOf(10000, 20000, 40000))
        assertArrayEquals(longArrayOf(30000), hints.payloads)
        assertArrayEquals(longArrayOf(10000, 20000, 40000), hints.headers)
        assertTrue(thumbnailSeparatedHints(longArrayOf(), longArrayOf(10000)).payloads.isEmpty())
    }

    @Test fun bothKindsStayBoundedWithoutChangingOrder() {
        val hints = thumbnailSeparatedHints(LongArray(40) { it.toLong() }, LongArray(40) { 100L + it })
        assertArrayEquals(LongArray(16) { it.toLong() }, hints.payloads)
        assertArrayEquals(LongArray(16) { 100L + it }, hints.headers)
    }

    @Test fun existingCoarseLookaheadKeepsPriorityAndDenseHeadersOnlyFillSpareCapacity() {
        val future = LongArray(32) { it.toLong() }
        val dense = LongArray(64) { 100L + it }
        val full = thumbnailBridgeHints(future, dense, { it.toInt() }, { false })
        assertArrayEquals(LongArray(16) { it.toLong() }, full.payloads)
        assertArrayEquals(LongArray(16) { 16L + it }, full.headers)
        val partlyKnown = thumbnailBridgeHints(future, dense, { it.toInt() }, { it in 16..23 })
        assertArrayEquals(LongArray(8) { 24L + it } + LongArray(8) { 100L + it }, partlyKnown.headers)
        assertArrayEquals(full.payloads, partlyKnown.payloads)
        val short = thumbnailBridgeHints(longArrayOf(30), dense, { it.toInt() }, { false })
        assertArrayEquals(longArrayOf(30), short.payloads)
        assertArrayEquals(LongArray(16) { 100L + it }, short.headers)
    }

    @Test fun filtersKnownAndAliasedCuesBeforeSelectingSixteen() {
        val hints = thumbnailUnknownHeaderHints(longArrayOf(-1, 0, 1, 10, 11, 20, 30),
            cueAt = { (it / 10).toInt() }, known = { it == 0 || it == 2 })
        assertArrayEquals(longArrayOf(10, 30), hints)
    }

    @Test fun doesNotProbeEntireIndexOrRepeatKnownPrefix() {
        var examined = 0
        val hints = thumbnailUnknownHeaderHints(LongArray(2000) { it.toLong() },
            cueAt = { examined++; it.toInt() }, known = { it < 48 })
        assertEquals(64, examined)
        assertArrayEquals(LongArray(16) { 48L + it }, hints)
        val none = thumbnailUnknownHeaderHints(LongArray(2000) { it.toLong() },
            cueAt = { it.toInt() }, known = { true })
        assertTrue(none.isEmpty())
    }
}
