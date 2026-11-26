package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailPlaybackGateTest {
    @Test fun buffersMustRecoverBeforeAnotherRequest() {
        val p = ThumbnailPlaybackGate()
        assertTrue(p.shouldWait(false,3,13000,0, false))
        assertFalse(p.shouldWait(false,3,8000,0, true))
        assertTrue(p.shouldWait(false,2,332,0))
        assertTrue(p.shouldWait(false,3,2500,0))
        assertFalse(p.shouldWait(false,3,3000,0))
        assertTrue(p.shouldWait(false,3,138,0))
    }
    @Test fun deepBufferIsAdmittedWithoutStabilityHistory() {
        val p = ThumbnailPlaybackGate()
        assertTrue(p.shouldWait(false,3,19000,0, false))
        assertFalse(p.shouldWait(false,3,50000,0, false))
        assertTrue(p.shouldWait(false,3,1500,0))          // the running check still holds
    }
    @Test fun fullSteadyBufferIsAdmittedBelowEightSeconds() {
        val p = ThumbnailPlaybackGate()
        assertTrue(p.shouldWait(false,3,5000,0, true, false))   // steady but still filling
        assertTrue(p.shouldWait(false,3,5000,0, false, true))   // full but draining
        assertTrue(p.shouldWait(false,3,3900,0, true, true))    // full and steady, too few seconds
        assertTrue(p.shouldWait(false,3,5000,3000, true, true)) // stale health
        assertTrue(p.shouldWait(false,2,5000,0, true, true))    // not playing
        assertFalse(p.shouldWait(false,3,5000,0, true, true))
        assertTrue(p.shouldWait(false,3,1500,0))                // the running check still holds
    }
    @Test fun allocationNearTheTargetCountsAsFull() {
        val target = 83_886_080L
        assertTrue(ThumbnailPlaybackGate.isFull(82_968_576L, target))
        assertTrue(ThumbnailPlaybackGate.isFull(85_721_088L, target))
        assertFalse(ThumbnailPlaybackGate.isFull(70_000_000L, target))
        assertFalse(ThumbnailPlaybackGate.isFull(-1L, -1L))
        assertFalse(ThumbnailPlaybackGate.isFull(83_886_080L, 0L))
    }
    @Test fun startupAndExplicitPauseRemainCreationWindows() {
        val p = ThumbnailPlaybackGate()
        assertFalse(p.shouldWait(true,2,0,0))
        assertTrue(p.shouldWait(false,3,5000,3000))
        assertFalse(p.shouldWait(true,3,1000,0))
    }
}
