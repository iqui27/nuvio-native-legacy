package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

class ThumbnailPrefetchTest {
    @Test fun fetchRunsOffCallerAndOnlyOneBatchIsOutstanding() {
        val entered = CountDownLatch(1)
        val release = CountDownLatch(1)
        val size = AtomicInteger()
        val worker = ThumbnailPrefetch({ true }, { times ->
            size.set(times.size); entered.countDown(); release.await(); 1
        }, { release.countDown() })
        try {
            assertTrue(worker.offer(LongArray(32) { it.toLong() }))
            assertTrue(entered.await(2, TimeUnit.SECONDS))
            assertEquals(16, size.get())
            assertFalse(worker.offer(longArrayOf(99)))
        } finally { worker.close() }
        assertFalse(worker.offer(longArrayOf(100)))
    }

    @Test fun unhealthyPlaybackDoesNotDispatch() {
        val calls = AtomicInteger()
        ThumbnailPrefetch({ false }, { calls.incrementAndGet() }, {}).use {
            assertFalse(it.offer(longArrayOf(1)))
        }
        assertEquals(0, calls.get())
    }

    @Test fun closeInterruptsWorkerAndAbortsOnlyOnce() {
        val entered = CountDownLatch(1)
        val exited = CountDownLatch(1)
        val aborts = AtomicInteger()
        val worker = ThumbnailPrefetch({ true }, {
            entered.countDown()
            try { CountDownLatch(1).await(); 0 } finally { exited.countDown() }
        }, { aborts.incrementAndGet() })
        assertTrue(worker.offer(longArrayOf(1)))
        assertTrue(entered.await(2, TimeUnit.SECONDS))
        worker.close(); worker.close()
        assertTrue(exited.await(2, TimeUnit.SECONDS))
        assertEquals(1, aborts.get())
    }
}
