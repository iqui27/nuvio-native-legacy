package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailBufferHealthTest {
    @Test fun fullBufferCannotMaskDrainingOrStaleHistory() {
        val h = ThumbnailBufferHealth()
        for (t in 0..15) assertFalse(h.sample(t*250L,5000,true))
        assertTrue(h.sample(4000,5000,true))
        assertFalse(h.sample(4250,3500,true))
        assertFalse(h.sample(7000,5000,true))
        assertFalse(h.sample(7250,5000,false))
    }
}
