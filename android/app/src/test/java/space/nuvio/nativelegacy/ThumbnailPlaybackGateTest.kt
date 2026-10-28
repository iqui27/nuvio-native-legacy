package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailPlaybackGateTest {
    @Test fun buffersMustRecoverBeforeAnotherRequest() {
        val p = ThumbnailPlaybackGate()
        assertFalse(p.shouldWait(false,3,6000,0))
        assertTrue(p.shouldWait(false,2,332,0))
        assertTrue(p.shouldWait(false,3,2500,0))
        assertFalse(p.shouldWait(false,3,3000,0))
        assertTrue(p.shouldWait(false,3,138,0))
    }
    @Test fun startupAndExplicitPauseRemainCreationWindows() {
        val p = ThumbnailPlaybackGate()
        assertFalse(p.shouldWait(true,2,0,0))
        assertTrue(p.shouldWait(false,3,5000,3000))
        assertFalse(p.shouldWait(true,3,1000,0))
    }
}
