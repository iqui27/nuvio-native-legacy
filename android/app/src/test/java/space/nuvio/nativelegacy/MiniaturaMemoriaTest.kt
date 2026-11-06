package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test

class MiniaturaMemoriaTest {
    @Test fun fourKMain10NeedsMoreThanTheOldFreeMemoryFloor() {
        val reserve = miniaturaReservaBytes(3840, 2160, true)
        assertFalse(miniaturaMemoriaAdmite(400L shl 20, reserve))
        assertTrue(miniaturaMemoriaAdmite(700L shl 20, reserve))
        assertTrue(reserve > miniaturaReservaBytes(1920, 1080, false))
    }
    @Test fun recoveryMarginIsPreservedAfterProspectiveAllocation() {
        val reserve = miniaturaReservaBytes(1920, 1080, false)
        assertFalse(miniaturaMemoriaAdmite(reserve + (364L shl 20) - 1, reserve))
        assertTrue(miniaturaMemoriaAdmite(reserve + (364L shl 20), reserve))
        assertFalse(miniaturaMemoriaAdmite(0, reserve))
    }
}
