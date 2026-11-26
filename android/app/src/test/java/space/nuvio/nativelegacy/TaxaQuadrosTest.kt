package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TaxaQuadrosTest {
    private fun m(id: Int, hz: Float, w: Int = 3840, h: Int = 2160) = TaxaQuadros.Modo(id, w, h, hz)

    // A typical Amlogic box: 2160p and 1080p at the usual HDMI rates.
    private val box = listOf(
        m(1, 60f), m(2, 59.94006f), m(3, 50f), m(4, 30f), m(5, 29.97003f),
        m(6, 25f), m(7, 24f), m(8, 23.976025f),
        m(9, 60f, 1920, 1080), m(10, 23.976025f, 1920, 1080),
    )
    private val em60 = box[0]

    @Test fun film23976PicksTheExactRateNot24() {
        assertEquals(8, TaxaQuadros.escolher(box, em60, 23.976f)?.id)
        assertEquals(8, TaxaQuadros.escolher(box, em60, 23.98f)?.id)
    }

    @Test fun film24Picks24() = assertEquals(7, TaxaQuadros.escolher(box, em60, 24f)?.id)

    @Test fun pal25Prefers50Hz() = assertEquals(3, TaxaQuadros.escolher(box, em60, 25f)?.id)

    @Test fun ntsc2997Prefers5994() = assertEquals(2, TaxaQuadros.escolher(box, em60, 29.97f)?.id)

    @Test fun alreadyMatchingModeStays() {
        assertNull(TaxaQuadros.escolher(box, em60, 30f))
        assertNull(TaxaQuadros.escolher(box, em60, 60f))
        assertNull(TaxaQuadros.escolher(box, box[2], 25f))
        assertNull(TaxaQuadros.escolher(box, box[7], 23.976f))
    }

    @Test fun keepsTheCurrentResolution() =
        assertEquals(10, TaxaQuadros.escolher(box, box[8], 23.976f)?.id)

    @Test fun panelWithOnlySixtyHzChangesNothing() =
        assertNull(TaxaQuadros.escolher(listOf(em60), em60, 23.976f))

    @Test fun unknownFrameRateChangesNothing() {
        assertNull(TaxaQuadros.escolher(box, em60, -1f))
        assertNull(TaxaQuadros.escolher(box, em60, 0f))
    }

    @Test fun multiplesWithinTolerance() {
        assertTrue(TaxaQuadros.casa(47.952f, 23.976f))
        assertFalse(TaxaQuadros.casa(24f, 23.976f))
        assertFalse(TaxaQuadros.casa(60f, 59.94f))
    }
}
