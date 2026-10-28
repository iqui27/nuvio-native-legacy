package space.nuvio.nativelegacy
import org.junit.Assert.*
import org.junit.Test

class ThumbnailProbeRecoveryTest {
    @Test fun recoveryRequiresCooldownAndSuccessfulRequests() {
        val p = ThumbnailProbeRecovery()
        assertTrue(p.allowed(0))
        p.timeout(1000)
        repeat(4) { p.success() }
        assertFalse(p.allowed(61000))
        p.success()
        assertFalse(p.allowed(60999))
        assertTrue(p.allowed(61000))
        p.timeout(62000)
        repeat(5) { p.success() }
        assertFalse(p.allowed(181999))
        assertTrue(p.allowed(182000))
    }
    @Test fun repeatedTimeoutsCannotRecoverIndefinitely() {
        val p = ThumbnailProbeRecovery()
        repeat(4) { n ->
            p.timeout(n * 1_000_000L)
            repeat(5) { p.success() }
            assertEquals(n < 3, p.allowed(n * 1_000_000L + 240_000))
        }
    }
}
