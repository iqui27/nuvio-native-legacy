package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class ThumbnailCueSelectionTest {
    private fun cues(vararg timesNs: Long) = LongArray(timesNs.size * 3).also { out ->
        timesNs.forEachIndexed { index, time ->
            out[index * 3] = time
            out[index * 3 + 1] = 1000L + index * 10000L
            out[index * 3 + 2] = 5
        }
    }

    private fun assertRoundTrip(cues: LongArray, selected: ThumbnailSelectedCue) {
        val tokenNs = selected.requestMs * 1_000_000L
        val resolved = (0 until cues.size / 3).lastOrNull { cues[it * 3] <= tokenNs } ?: 0
        assertEquals(selected.index, resolved)
        assertEquals(cues[selected.index * 3], selected.timeNs)
        assertEquals(cues[selected.index * 3] / 1000, selected.presentationTimeUs)
    }

    @Test fun nearestChoosesEitherSideAndTiesPreferEarlierCue() {
        val input = cues(6_000_000_000L, 14_000_000_000L)
        val selector = ThumbnailCueSelection(input)
        assertEquals(0, selector.nearest(9000)!!.index)
        assertEquals(0, selector.nearest(10000)!!.index)
        assertEquals(1, selector.nearest(11000)!!.index)
        assertEquals(14000L, selector.nearest(11000)!!.requestMs)
        assertRoundTrip(input, selector.nearest(11000)!!)
    }

    @Test fun zeroUsesEarliestPositiveCueAndDoesNotRenameItsPresentationTime() {
        val input = cues(0, 4_656_000_000L, 9_312_000_000L)
        val selected = ThumbnailCueSelection(input).nearest(0)!!
        assertEquals(1, selected.index)
        assertEquals(4656L, selected.requestMs)
        assertEquals(4_656_000L, selected.presentationTimeUs)
        assertRoundTrip(input, selected)
    }

    @Test fun zeroOnlyOrDistantPositiveCueIsExplicitlyUnavailable() {
        assertNull(ThumbnailCueSelection(cues(0)).nearest(0))
        assertNull(ThumbnailCueSelection(cues(0, 999)).nearest(0))
        assertEquals(1L, ThumbnailCueSelection(cues(0, 1000)).nearest(0)!!.presentationTimeUs)
        assertNull(ThumbnailCueSelection(cues(0, 5_000_000_001L)).nearest(0))
        assertEquals(5000L, ThumbnailCueSelection(cues(0, 5_000_000_000L)).nearest(0)!!.requestMs)
    }

    @Test fun firstAndLastCueAreBoundedByActualFiveSecondDistance() {
        val selector = ThumbnailCueSelection(cues(2_000_000_000L, 20_000_000_000L))
        assertEquals(0, selector.nearest(1000)!!.index)
        assertEquals(1, selector.nearest(25000)!!.index)
        assertNull(selector.nearest(25001))
        assertNull(selector.nearest(10000))
    }

    @Test fun toleranceIsStrictInNanosecondsOnBothSides() {
        assertNotNull(ThumbnailCueSelection(cues(5_000_000_000L)).nearest(10000))
        assertNull(ThumbnailCueSelection(cues(4_999_999_999L)).nearest(10000))
        assertNotNull(ThumbnailCueSelection(cues(15_000_000_000L)).nearest(10000))
        assertNull(ThumbnailCueSelection(cues(15_000_000_001L)).nearest(10000))
    }

    @Test fun subMillisecondMatroskaCueUsesCeilingTokenAndPreservesPrecisePts() {
        // CueTime * a non-millisecond TimestampScale, stored in nanoseconds.
        val input = cues(0, 22_397_500_000L, 27_052_500_000L)
        val selected = ThumbnailCueSelection(input).nearest(22000)!!
        assertEquals(22398L, selected.requestMs)
        assertEquals(22_397_500L, selected.presentationTimeUs)
        assertEquals(22_397_500_000L, input[3])
        assertRoundTrip(input, selected)
        // Flooring would select the previous cue instead of the chosen packet.
        assertTrue(input[3] > 22397L * 1_000_000L)
    }

    @Test fun singleNanosecondFractionSurvivesTheMillisecondRoundTrip() {
        val input = cues(10_000_001L, 20_000_000_000L)
        val selected = ThumbnailCueSelection(input).nearest(10)!!
        assertEquals(11L, selected.requestMs)
        assertEquals(10_000_001L, selected.timeNs)
        assertEquals(10_000L, selected.presentationTimeUs)
        assertRoundTrip(input, selected)
    }

    @Test fun ambiguousCuesInsideOneMillisecondRejectInsteadOfChangingIdentity() {
        val selector = ThumbnailCueSelection(cues(0, 10_000_000_001L, 10_000_500_000L))
        assertNull(selector.nearest(10000))
        assertEquals(2, selector.nearest(10001)!!.index)
    }

    @Test fun malformedOrOverflowingTimestampsFailClosed() {
        assertNull(ThumbnailCueSelection(longArrayOf()).nearest(0))
        assertNull(ThumbnailCueSelection(longArrayOf(1, 2)).nearest(0))
        assertNull(ThumbnailCueSelection(cues(2, 1)).nearest(0))
        assertNull(ThumbnailCueSelection(cues(-1)).nearest(0))
        val selector = ThumbnailCueSelection(cues(1))
        assertNull(selector.nearest(-1))
        assertNull(selector.nearest(Long.MAX_VALUE))
        val maximumMs = Long.MAX_VALUE / 1_000_000L
        assertNull(ThumbnailCueSelection(cues(Long.MAX_VALUE)).nearest(maximumMs))
    }

    @Test fun realMp4AvcAndHevcSyncSamplesKeepExactIndexAndPts() {
        for (codec in listOf("avc", "hevc")) {
            val data = javaClass.classLoader!!.getResourceAsStream("thumbnail-mp4/$codec.mp4")!!.use { it.readBytes() }
            val index = ThumbnailMp4Index.read(data.size.toLong()) { pos, size ->
                data.copyOfRange(pos.toInt(), pos.toInt() + size)
            }!!
            val selector = ThumbnailCueSelection(index.cues)
            for (i in index.sizes.indices) {
                val timeNs = index.cues[i * 3]
                if (timeNs == 0L) continue // nominal zero has the explicit positive-cue policy
                val selected = selector.nearest(timeNs / 1_000_000L)!!
                assertEquals("$codec sample $i", i, selected.index)
                assertRoundTrip(index.cues, selected)
            }
        }
    }
}
