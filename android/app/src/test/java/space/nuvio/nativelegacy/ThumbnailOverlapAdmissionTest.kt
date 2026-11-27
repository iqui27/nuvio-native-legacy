package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailOverlapAdmissionTest {
    private fun allowed(buffer: Long = 4000, stable: Boolean = true, bytes: Long = 48,
                        age: Long = 0, ready: Boolean = true, playing: Boolean = true) =
        thumbnailOverlapBufferHealthy(ready,playing,buffer,age,bytes,48,stable)
    @Test fun stableFullByteBufferCanAdmitOverlapBelowEightSeconds() {
        assertTrue(allowed())
        assertFalse(allowed(bytes=40))
        assertFalse(allowed(stable=false))
        assertFalse(allowed(buffer=2999))
    }
    @Test fun playbackFaultsAlwaysPreventOverlap() {
        assertFalse(allowed(ready=false))
        assertFalse(allowed(playing=false))
        assertFalse(allowed(age=2001))
        assertFalse(allowed(age=-1))
    }
    @Test fun existingTimeBufferAdmissionStillWorks() {
        assertTrue(allowed(buffer=8000,bytes=0,stable=false))
    }
}
