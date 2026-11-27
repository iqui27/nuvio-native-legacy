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

    // MKV DefaultDuration 41708333 ns -> 23.97602; rounded containers -> 23.98.
    @Test fun snapsContainerRatesToStandard() {
        assertEquals(24000f / 1001f, TaxaQuadros.padronizar(1e9f / 41708333f), 0.0001f)
        assertEquals(24000f / 1001f, TaxaQuadros.padronizar(23.98f), 0.0001f)
        assertEquals(24f, TaxaQuadros.padronizar(24.0001f), 0.0001f)
        assertEquals(25f, TaxaQuadros.padronizar(1e9f / 40000000f), 0.0001f)
        assertEquals(60000f / 1001f, TaxaQuadros.padronizar(59.94f), 0.0001f)
        assertEquals(17f, TaxaQuadros.padronizar(17f), 0.0001f)   // not near a standard rate
    }

    @Test fun snappedMkvRatePicksTheMatchingMode() =
        assertEquals(8, TaxaQuadros.escolher(box, em60, TaxaQuadros.padronizar(1e9f / 41708333f))?.id)

    // MKV timestamps are whole milliseconds: 23.976 arrives as 41/42 ms gaps,
    // and B-frames arrive out of order.
    @Test fun measuresFromMillisecondTimestamps() {
        val med = TaxaQuadros.Medidor()
        val pts = (0 until 48).map { (it * 1001000L / 24 / 1000) * 1000 }
        val ordem = pts.chunked(4).flatMap { listOf(it[0]) + it.drop(1).reversed() }
        ordem.forEach { med.quadro(it) }
        assertTrue(med.cheio)
        assertEquals(24000f / 1001f, med.fps(), 0.0001f)
    }

    @Test fun measures25AndIgnoresADroppedFrame() {
        val med = TaxaQuadros.Medidor()
        (0 until 49).filter { it != 10 }.forEach { med.quadro(it * 40000L) }
        assertEquals(25f, med.fps(), 0.0001f)
    }

    // Every other frame dropped: 83.4 ms gaps, 11.988 fps is no standard rate.
    @Test fun halfRateIsUnknown() {
        val med = TaxaQuadros.Medidor()
        (0 until 48).forEach { med.quadro(it * 2 * 1001000L / 24) }
        assertEquals(-1f, med.fps(), 0f)
    }

    // A seek back mid-sample: only the frames after it count.
    @Test fun seekStartsTheSampleOver() {
        val med = TaxaQuadros.Medidor()
        (0 until 30).forEach { med.quadro(10_000_000L + it * 40000L) }
        (0 until 48).forEach { med.quadro(it * 1001000L / 24) }
        assertTrue(med.cheio)
        assertEquals(24000f / 1001f, med.fps(), 0.0001f)
    }

    @Test fun tooFewFramesIsUnknown() {
        val med = TaxaQuadros.Medidor()
        (0 until 10).forEach { med.quadro(it * 40000L) }
        assertFalse(med.cheio)
        assertEquals(-1f, med.fps(), 0f)
    }
}
