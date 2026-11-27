package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Test

class MiniaturaPerfilTest {
    @Test fun explicitResolutionOverridesAutoAndInvalidChoiceFallsBack() {
        assertEquals(576, miniaturaLargura(true, 1L shl 30, 0, 576))
        assertEquals(320, miniaturaLargura(true, 1L shl 30, 0, 640))   // 640 is no longer a size: falls back to automatic
        assertEquals(480, miniaturaLargura(true, 1L shl 30, 0, 480))
        assertEquals(320, miniaturaLargura(false, 4L shl 30, 0, 320))
        assertEquals(320, miniaturaLargura(true, 1L shl 30, 0, 123))
    }
    @Test fun lowMemoryDevicesUseSmallerOutput() {
        assertEquals(320, miniaturaLargura(true, 4L shl 30, 0))
        assertEquals(320, miniaturaLargura(false, 2L shl 30, 0))
        assertEquals(320, miniaturaLargura(false, 1L shl 30, 0))
    }
    @Test fun unknownAndMidMemoryDevicesKeepMediumQuality() {
        assertEquals(480, miniaturaLargura(false, 0, 0))
        assertEquals(480, miniaturaLargura(false, 9L shl 28, 0))   // 2.25 GB
        assertEquals(480, miniaturaLargura(false, 5L shl 29, 0))   // exactly 2.5 GB
    }
    @Test fun automaticGoesHighAboveTwoAndAHalfGigabytes() {
        assertEquals(576, miniaturaLargura(false, 2946L shl 20, 0))  // NVIDIA Shield (2946 MB reported)
        assertEquals(576, miniaturaLargura(false, 3L shl 30, 0))
        assertEquals(576, miniaturaLargura(false, 4L shl 30, 0))
        assertEquals(320, miniaturaLargura(true, 8L shl 30, 0))      // low-RAM flag still wins
    }
    @Test fun diagnosticOverridesPermitComparisonsOnTheSameDevice() {
        assertEquals(480, miniaturaLargura(true, 1L shl 30, 128))
        assertEquals(320, miniaturaLargura(false, 4L shl 30, 64))
        assertEquals(480, miniaturaLargura(true, 1L shl 30, 64 or 128))
    }
}
