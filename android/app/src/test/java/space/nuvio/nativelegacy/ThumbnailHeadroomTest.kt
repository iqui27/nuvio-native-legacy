package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Test

class ThumbnailHeadroomTest {
    private val full = ThumbnailHeadroom.Snapshot(10000, 1, 3, true, true, 4000, 48, 48, true)
    private fun headroom(sample: ThumbnailHeadroom.Snapshot? = full, desiredMs: Long = 6000, now: Long = 10000) =
        ThumbnailHeadroom.evaluate(now, sample, desiredMs)

    @Test fun stableFullByteBufferMeetsBatchAndRefinementTargets() {
        assertEquals(1, headroom())
        assertEquals(1, headroom(desiredMs = 8000))
        assertEquals(1, headroom(full.copy(allocatedBytes = 90, targetBytes = 100)))
        assertEquals(0, headroom(full.copy(allocatedBytes = 89, targetBytes = 100)))
    }

    @Test fun drainingUnknownOrUnderfilledBytesDoNotBypassSecondsTarget() {
        assertEquals(0, headroom(full.copy(stable = false)))
        assertEquals(0, headroom(full.copy(allocatedBytes = 20)))
        assertEquals(0, headroom(full.copy(allocatedBytes = -1, targetBytes = -1)))
        assertEquals(0, headroom(full.copy(bufferMs = 2999)))
    }

    @Test fun absentSampleIsDistinctFromAStaleOrFutureSample() {
        assertEquals(-1, headroom(null))
        assertEquals(1, headroom(now = 12000))
        assertEquals(0, headroom(now = 12001))
        assertEquals(0, headroom(now = 9999))
    }

    @Test fun bufferingErrorsTransientStopsAndLowBufferAlwaysBlock() {
        assertEquals(0, headroom(full.copy(state = 2)))
        assertEquals(0, headroom(full.copy(state = 1)))
        assertEquals(0, headroom(full.copy(playing = false)))
        assertEquals(0, headroom(full.copy(bufferMs = 1999)))
    }

    @Test fun explicitReadyPauseDoesNotWaitForBufferThatIsNotConsumed() {
        val paused = full.copy(playing = false, playWhenReady = false, bufferMs = 2000, stable = false)
        assertEquals(1, headroom(paused, desiredMs = 8000))
        assertEquals(0, headroom(paused.copy(state = 2)))
        assertEquals(0, headroom(paused, now = 12001))
        assertEquals(0, headroom(paused.copy(bufferMs = 1999)))
    }

    @Test fun secondsTargetRemainsAvailableWithoutByteMetadata() {
        val timeOnly = full.copy(allocatedBytes = -1, targetBytes = -1, stable = false, bufferMs = 6000)
        assertEquals(1, headroom(timeOnly))
        assertEquals(0, headroom(timeOnly, desiredMs = 8000))
        assertEquals(1, headroom(timeOnly.copy(bufferMs = 8000), desiredMs = 8000))
    }

    @Test fun freshHealthyStartupNeedsNoAdditionalIncidentCooldown() {
        // Audio/drop recovery remains native-owned, not inferred from startup BUFFERING.
        assertEquals(1, ThumbnailHeadroom.evaluate(10000, full, 6000, latestIncidentAtMs = 9999))
        assertEquals(0, ThumbnailHeadroom.evaluate(10001, full, 6000, latestIncidentAtMs = 10001))
        assertEquals(0, ThumbnailHeadroom.evaluate(10000, full, 6000, latestIncidentAtMs = 10000))
        assertEquals(1, ThumbnailHeadroom.evaluate(10002, full.copy(sampledAtMs = 10002), 6000,
            latestIncidentAtMs = 10001))
    }

    @Test fun anEarlierSourcesHealthySnapshotCannotAdmitANewSource() {
        assertEquals(0, ThumbnailHeadroom.evaluate(10000, full, 6000, currentSourceEpoch = 2))
        assertEquals(-1, ThumbnailHeadroom.evaluate(10000, null, 6000, currentSourceEpoch = 2))
        assertEquals(1, ThumbnailHeadroom.evaluate(10000, full.copy(sourceEpoch = 2), 6000,
            currentSourceEpoch = 2))
    }

    @Test fun sharedByteRulePreservesFastEntryAndContinuationHysteresis() {
        assertEquals(false, ThumbnailHeadroom.byteHealthy(2999, 100, 100, true))
        assertEquals(true, ThumbnailHeadroom.byteHealthy(3000, 90, 100, true))
        assertEquals(false, ThumbnailHeadroom.byteHealthy(3000, 89, 100, true))
        assertEquals(true, ThumbnailHeadroom.byteHealthy(2500, 80, 100, true, continuingFast = true))
        assertEquals(false, ThumbnailHeadroom.byteHealthy(2499, 80, 100, true, continuingFast = true))
        assertEquals(false, ThumbnailHeadroom.byteHealthy(2500, 79, 100, true, continuingFast = true))
        assertEquals(false, ThumbnailHeadroom.byteHealthy(3000, 100, 100, false, continuingFast = true))
    }
}
