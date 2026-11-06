package space.nuvio.nativelegacy

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.IOException
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

class ThumbnailSingleLanesTest {

    @Test fun lanesOnlyWithoutMultiRangeAndWithFastPacing() {
        assertEquals(ThumbnailSingleLanes.LANES, ThumbnailSingleLanes.lanes(multiRangeOk = false, fastPacing = true))
        assertEquals(1, ThumbnailSingleLanes.lanes(multiRangeOk = false, fastPacing = false))
        // TorBox: multipart works, one stream stays the measured best shape.
        assertEquals(1, ThumbnailSingleLanes.lanes(multiRangeOk = true, fastPacing = true))
    }

    @Test fun ownSlotsGiveMoreLanesButStillOnlyWhenFast() {
        assertEquals(ThumbnailSingleLanes.LANES_OWN, ThumbnailSingleLanes.lanes(multiRangeOk = false, fastPacing = true, ownSlots = true))
        assertEquals(1, ThumbnailSingleLanes.lanes(multiRangeOk = false, fastPacing = false, ownSlots = true))
        assertEquals(1, ThumbnailSingleLanes.lanes(multiRangeOk = true, fastPacing = true, ownSlots = true))
    }

    @Test fun lanesArgumentCapsInFlightBelowThePool() {
        val exec = Executors.newFixedThreadPool(6)
        val active = AtomicInteger(); val peak = AtomicInteger()
        try {
            val ranges = (0 until 12).map { longArrayOf(it.toLong(), it.toLong()) }
            ThumbnailSingleLanes.fetchAll(ranges, exec, lanes = 2) { r ->
                val n = active.incrementAndGet(); peak.accumulateAndGet(n, ::maxOf)
                Thread.sleep(20); active.decrementAndGet(); r[0]
            }
            assertEquals(2, peak.get())
        } finally { exec.shutdownNow() }
    }

    @Test fun awaitTurnsCancellationIntoInterruptedIo() {
        val exec = Executors.newFixedThreadPool(1)
        try {
            val f = ThumbnailSingleLanes.submit(exec) { Thread.sleep(5_000); 1 }
            f.cancel(true)
            try { ThumbnailSingleLanes.await(f); throw AssertionError("should have thrown") }
            catch (e: java.io.InterruptedIOException) { /* expected */ }
        } finally { exec.shutdownNow() }
    }

    @Test fun learnedCapLowersOwnLanesButNeverRaisesThem() {
        assertEquals(4, ThumbnailSingleLanes.lanes(false, true, ownSlots = true, learnedCap = 4))
        assertEquals(ThumbnailSingleLanes.LANES_OWN, ThumbnailSingleLanes.lanes(false, true, ownSlots = true, learnedCap = 50))
        assertEquals(ThumbnailSingleLanes.MIN_CAP, ThumbnailSingleLanes.lanes(false, true, ownSlots = true, learnedCap = 0))
        assertEquals(1, ThumbnailSingleLanes.lanes(false, false, ownSlots = true, learnedCap = 4))
    }

    @Test fun refusalIsALimitOnlyOnABusyWorkingLink() {
        assertEquals(5, ThumbnailSingleLanes.capAfterRefusal(inFlight = 6, msSinceLastOk = 800))
        assertEquals(ThumbnailSingleLanes.MIN_CAP, ThumbnailSingleLanes.capAfterRefusal(inFlight = 3, msSinceLastOk = 0))
        assertEquals(null, ThumbnailSingleLanes.capAfterRefusal(inFlight = 2, msSinceLastOk = 100))     // too few
        assertEquals(null, ThumbnailSingleLanes.capAfterRefusal(inFlight = 6, msSinceLastOk = 30_000))  // dead link
        assertEquals(null, ThumbnailSingleLanes.capAfterRefusal(inFlight = 6, msSinceLastOk = -5))
    }

    @Test fun gateHoldsNewRequestsAfterTheLimitDrops() {
        val gate = ThumbnailSingleLanes.Gate(4)
        val exec = Executors.newFixedThreadPool(8)
        val active = AtomicInteger(); val peakAfter = AtomicInteger()
        val lowered = java.util.concurrent.atomic.AtomicBoolean(false)
        try {
            val first = (0 until 4).map { exec.submit { gate.run { active.incrementAndGet(); Thread.sleep(150); active.decrementAndGet() } } }
            Thread.sleep(50)
            gate.setLimit(2); lowered.set(true)
            val later = (0 until 6).map { exec.submit { gate.run {
                val n = active.incrementAndGet(); if (lowered.get()) peakAfter.accumulateAndGet(n, ::maxOf)
                Thread.sleep(30); active.decrementAndGet()
            } } }
            (first + later).forEach { it.get(5, TimeUnit.SECONDS) }
            assertTrue("a new request started while ${peakAfter.get()} ran", peakAfter.get() <= 4)
            assertEquals(0, gate.active())
        } finally { exec.shutdownNow() }
    }

    @Test fun capKeepsWhatFitsAndAlwaysOne() {
        val mb = 1L shl 20
        val r = listOf(longArrayOf(0, 2 * mb - 1), longArrayOf(10 * mb, 12 * mb - 1), longArrayOf(20 * mb, 22 * mb - 1))
        assertEquals(2, ThumbnailSingleLanes.capBytes(r, 5 * mb).size)
        assertEquals(1, ThumbnailSingleLanes.capBytes(r, 1).size)
        assertEquals(3, ThumbnailSingleLanes.capBytes(r, 100 * mb).size)
    }

    @Test fun fetchAllKeepsOrderAndOverlapsUpToThePoolSize() {
        val exec = Executors.newFixedThreadPool(3)
        val active = AtomicInteger(); val peak = AtomicInteger()
        val threeIn = CountDownLatch(3)
        try {
            val ranges = (0 until 6).map { longArrayOf(it * 100L, it * 100L + 99) }
            val out = ThumbnailSingleLanes.fetchAll(ranges, exec) { r ->
                val n = active.incrementAndGet(); peak.accumulateAndGet(n, ::maxOf)
                threeIn.countDown()
                // the first three wait until all three are in flight: proves the overlap
                threeIn.await(5, TimeUnit.SECONDS)
                Thread.sleep((600 - r[0]) / 20)   // later ranges finish first
                active.decrementAndGet()
                r[0]
            }
            assertEquals(ranges.map { it[0] }, out)
            assertEquals(3, peak.get())
        } finally { exec.shutdownNow() }
    }

    @Test fun firstFailureIsRethrownAndTheRestCancelled() {
        val exec = Executors.newFixedThreadPool(2)
        val started = AtomicInteger()
        try {
            val ranges = (0 until 8).map { longArrayOf(it.toLong(), it.toLong()) }
            try {
                ThumbnailSingleLanes.fetchAll(ranges, exec) { r ->
                    started.incrementAndGet()
                    if (r[0] == 0L) throw IOException("http 404")
                    Thread.sleep(200)
                    r[0]
                }
                throw AssertionError("should have failed")
            } catch (e: IOException) {
                assertEquals("http 404", e.message)
            }
            exec.shutdown(); assertTrue(exec.awaitTermination(5, TimeUnit.SECONDS))
            assertTrue("cancelled ranges must not all start (started=${started.get()})", started.get() < ranges.size)
        } finally { exec.shutdownNow() }
    }
}
