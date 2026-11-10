package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class ThumbnailProbeAdmissionTest {
    @Test fun bufferingIsNotAConfirmedPause() {
        assertFalse(ThumbnailProbeAdmission.allowed(true,true,false,1000,1100,1,1))
        assertFalse(ThumbnailProbeAdmission.allowed(true,false,true,1000,1100,1,1))
    }
    @Test fun pausedIdleRequiresFreshObservation() {
        assertTrue(ThumbnailProbeAdmission.allowed(true,false,false,1000,1100,1,1))
        assertFalse(ThumbnailProbeAdmission.allowed(true,false,false,1000,4000,1,1))
        assertFalse(ThumbnailProbeAdmission.allowed(true,false,false,0,1100,1,1))
    }
    @Test fun ClosedPlayerPermitsButNewSourceAlwaysCancels() {
        assertTrue(ThumbnailProbeAdmission.allowed(false,false,false,0,1100,1,1))
        assertFalse(ThumbnailProbeAdmission.allowed(false,false,false,0,1100,2,1))
        assertFalse(ThumbnailProbeAdmission.allowed(true,false,false,1000,1100,2,1))
    }
}
