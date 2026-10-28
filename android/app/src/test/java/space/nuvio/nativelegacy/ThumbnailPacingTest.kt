package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailPacingTest {
    @Test fun fastRequiresHealthyWindowAndBuffer() {
        val p = ThumbnailPacing()
        assertEquals(ThumbnailPacing.NORMAL, p.policy(0,true,true,true,12000,0))
        assertEquals(ThumbnailPacing.FAST, p.policy(10000,true,true,true,12000,0))
        assertEquals(ThumbnailPacing.FAST, p.policy(11000,true,true,true,9000,0))
        assertEquals(ThumbnailPacing.NORMAL, p.policy(12000,false,true,true,12000,0))
    }
    @Test fun bufferLossOrNetworkFailureBacksOffForAMinute() {
        val p = ThumbnailPacing()
        p.policy(0,true,true,true,12000,0)
        assertEquals(ThumbnailPacing.FAST,p.policy(10000,true,true,true,12000,0))
        assertEquals(ThumbnailPacing.NORMAL,p.policy(11000,true,true,true,1000,0))
        assertEquals(ThumbnailPacing.NORMAL,p.policy(12000,true,true,true,12000,0))
        assertEquals(ThumbnailPacing.NORMAL,p.policy(25000,true,true,true,12000,0))
        assertEquals(ThumbnailPacing.FAST,p.policy(26000,true,true,true,12000,0))
        p.failure(72000)
        assertEquals(ThumbnailPacing.NORMAL,p.policy(90000,true,true,true,12000,0))
    }
    @Test fun byteCappedStableBufferCanEnterFastBelowTenSeconds() {
        val p = ThumbnailPacing()
        assertEquals(ThumbnailPacing.NORMAL,p.policy(0,true,true,true,4000,0,48,48,true))
        assertEquals(ThumbnailPacing.FAST,p.policy(10000,true,true,true,4000,0,48,48,true))
        assertEquals(ThumbnailPacing.FAST,p.policy(11000,true,true,true,3000,0,40,48,true))
        assertEquals(ThumbnailPacing.NORMAL,p.policy(12000,true,true,true,3000,0,40,48,false))
    }
    @Test fun unknownOrUnderfilledByteBufferDoesNotBypassTimeRequirement() {
        val p = ThumbnailPacing()
        p.policy(0,true,true,true,4000,0,20,48,true)
        assertEquals(ThumbnailPacing.NORMAL,p.policy(20000,true,true,true,4000,0,20,48,true))
        assertEquals(ThumbnailPacing.NORMAL,p.policy(30000,true,true,true,4000,0,-1,-1,true))
    }
}
