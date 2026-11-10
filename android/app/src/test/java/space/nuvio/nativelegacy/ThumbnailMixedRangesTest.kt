package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailMixedRangesTest {
    @Test fun retainsProbesAndFitsCompletePayloadsWithinBudget() {
        val probes = listOf(longArrayOf(0,99))
        val result = thumbnailMixedRanges(probes,listOf(longArrayOf(1000,8L shl 20),longArrayOf(10000,10999)))
        assertEquals(2,result.size)
        assertTrue(result.any { it.contentEquals(probes[0]) })
        assertTrue(result.sumOf { it[1]-it[0]+1 } <= (8L shl 20))
    }
    @Test fun capsRangeCountAndAvoidsContainedDuplicates() {
        val probes = (0 until 31).map { longArrayOf(it*1000L,it*1000L+99) }
        val result = thumbnailMixedRanges(probes,listOf(longArrayOf(0,50),longArrayOf(50000,50099),longArrayOf(60000,60099)))
        assertEquals(32,result.size)
        assertTrue(result.any { it[0]==50000L })
    }

    private fun ranges(count: Int, base: Long, length: Long = 81) =
        (0 until count).map { longArrayOf(base + it * 1000000, base + it * 1000000 + length - 1) }

    @Test fun currentTwelvePayloadsReserveSlotsBeforeTwentyEightFutureProbes() {
        val required = ranges(4, 0)
        val payloads = ranges(12, 100000000, 100000)
        val future = ranges(28, 200000000)
        val plan = thumbnailCurrentRanges(required, payloads, future)!!
        assertEquals(32, plan.ranges.size)
        assertEquals(12, plan.currentPayloadCount)
        assertEquals((0 until 16).toList(), plan.optionalProbeIndices)
        for (range in required + payloads) assertTrue(plan.ranges.any { it.contentEquals(range) })
    }

    @Test fun thirtyTwoMandatoryProbesAreNeverDroppedForPayloadOrFutureWork() {
        val required = ranges(32, 0)
        val plan = thumbnailCurrentRanges(required, ranges(1, 100000000), ranges(5, 200000000))!!
        assertEquals(32, plan.ranges.size)
        assertEquals(0, plan.currentPayloadCount)
        assertTrue(plan.optionalProbeIndices.isEmpty())
        for (range in required) assertTrue(plan.ranges.any { it.contentEquals(range) })
    }

    @Test fun reservesBytesAsWellAsSlotsAndMayFitASmallerFutureProbe() {
        val required = listOf(longArrayOf(0, 99))
        val payload = listOf(longArrayOf(1000, 1799))
        val future = listOf(longArrayOf(3000, 3199), longArrayOf(4000, 4123))
        val plan = thumbnailCurrentRanges(required, payload, future, 1024)!!
        assertEquals(1024L, plan.ranges.sumOf { it[1] - it[0] + 1 })
        assertEquals(listOf(1), plan.optionalProbeIndices)
        assertEquals(1, plan.currentPayloadCount)
    }

    @Test fun mandatoryOverflowReturnsFallbackInsteadOfAPartialPlan() {
        assertNull(thumbnailCurrentRanges(ranges(33, 0), emptyList(), emptyList()))
        assertNull(thumbnailCurrentRanges(listOf(longArrayOf(0, 8L shl 20)), emptyList(), emptyList()))
        assertNull(thumbnailCurrentRanges(listOf(longArrayOf(0, Long.MAX_VALUE)), emptyList(), emptyList()))
        assertNull(thumbnailCurrentRanges(listOf(longArrayOf(2, 1)), emptyList(), emptyList()))
    }

    @Test fun noMissingHeadersDoesNotCreateAnExtraProbeRequest() {
        val payload = ranges(1, 100000000, 100000)
        val plan = thumbnailCurrentRanges(emptyList(), payload, emptyList())!!
        assertEquals(1, plan.ranges.size)
        assertEquals(1, plan.currentPayloadCount)
        assertTrue(plan.optionalProbeIndices.isEmpty())
        assertArrayEquals(payload[0], plan.ranges[0])
    }

    @Test fun oversizedCurrentBlockCannotBeDelayedByFutureOnlyProbes() {
        val oversized = listOf(longArrayOf(100000000, 100000000 + (8L shl 20)))
        val noRequired = thumbnailCurrentRanges(emptyList(), oversized, ranges(16, 200000000))!!
        assertTrue(noRequired.ranges.isEmpty())
        assertTrue(noRequired.optionalProbeIndices.isEmpty())
        val required = ranges(1, 0)
        val withRequired = thumbnailCurrentRanges(required, oversized, ranges(16, 200000000))!!
        assertEquals(1, withRequired.ranges.size)
        assertArrayEquals(required[0], withRequired.ranges[0])
        assertTrue(withRequired.optionalProbeIndices.isEmpty())
    }

    @Test fun cannotExpandEightMiBCapOrDispatchWithOnlyFutureWork() {
        val plan = thumbnailCurrentRanges(emptyList(), ranges(2, 100000000, 5L shl 20), ranges(2, 200000000), Long.MAX_VALUE)!!
        assertEquals(1, plan.currentPayloadCount)
        assertTrue(plan.ranges.sumOf { it[1] - it[0] + 1 } <= (8L shl 20))
        assertTrue(thumbnailCurrentRanges(emptyList(), emptyList(), ranges(32, 0))!!.ranges.isEmpty())
    }

    @Test fun containedOptionalHeaderNeedsNoExtraBytesOrRangeSlot() {
        val payload = listOf(longArrayOf(1000, 1999))
        val plan = thumbnailCurrentRanges(emptyList(), payload, listOf(longArrayOf(1100, 1180)), 1000)!!
        assertEquals(1, plan.ranges.size)
        assertEquals(listOf(0), plan.optionalProbeIndices)
        assertEquals(1000L, plan.ranges.sumOf { it[1] - it[0] + 1 })
    }

    @Test fun skipsOversizedPayloadButKeepsCompleteSmallerCurrentWork() {
        val payloads = listOf(longArrayOf(0, 8L shl 20), longArrayOf(20000000, 20000999))
        val plan = thumbnailCurrentRanges(emptyList(), payloads, ranges(2, 100000000))!!
        assertEquals(1, plan.currentPayloadCount)
        assertTrue(plan.ranges.any { it.contentEquals(payloads[1]) })
        assertFalse(plan.ranges.any { it.contentEquals(payloads[0]) })
    }
}
