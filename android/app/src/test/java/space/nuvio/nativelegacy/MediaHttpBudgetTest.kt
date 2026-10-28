package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.io.IOException
import java.io.InterruptedIOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import java.net.ServerSocket
import java.net.InetAddress

class MediaHttpBudgetTest {
    @Test fun realHttpServerSeesOnlyFourRequestsWhileTheirBodiesAreHeld() {
        val server = ServerSocket(0, 16, InetAddress.getByName("127.0.0.1"))
        val accepted = AtomicInteger()
        val firstFour = CountDownLatch(4)
        val allAttempted = CountDownLatch(10)
        val release = CountDownLatch(1)
        val handlers = Executors.newCachedThreadPool()
        val clients = Executors.newFixedThreadPool(10)
        val acceptor = Thread {
            while (!server.isClosed) try {
                val socket = server.accept()
                handlers.submit {
                    socket.use {
                        val input = it.getInputStream().bufferedReader()
                        while (true) { val line = input.readLine() ?: break; if (line.isEmpty()) break }
                        accepted.incrementAndGet(); firstFour.countDown()
                        val output = it.getOutputStream()
                        output.write("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nConnection: close\r\n\r\n".toByteArray())
                        output.flush()
                        if (release.await(5, TimeUnit.SECONDS)) { output.write('x'.code); output.flush() }
                    }
                }
            } catch (_: IOException) { if (!server.isClosed) throw AssertionError("accept failed") }
        }.apply { isDaemon = true; start() }
        val jobs = (0 until 10).map { i -> clients.submit<Int> {
            allAttempted.countDown()
            val c = MediaHttpBudget.open(URL("http://127.0.0.1:${server.localPort}/$i"), if (i % 2 == 0) "player" else "thumbnail")
            try {
                c.setRequestProperty("Connection", "close")
                c.readTimeout = 5000
                assertEquals(200, c.responseCode)
                c.inputStream.use { it.read() }
            } finally { MediaHttpBudget.close(c) }
        } }
        try {
            assertTrue(allAttempted.await(5, TimeUnit.SECONDS))
            assertTrue(firstFour.await(5, TimeUnit.SECONDS))
            assertEquals(4, accepted.get())
            assertEquals(4, MediaHttpBudget.active())
            release.countDown()
            jobs.forEach { assertEquals('x'.code, it.get(5, TimeUnit.SECONDS)) }
            assertEquals(10, accepted.get())
        } finally {
            release.countDown(); server.close(); acceptor.join(1000)
            clients.shutdownNow(); handlers.shutdownNow()
            clients.awaitTermination(5, TimeUnit.SECONDS); handlers.awaitTermination(5, TimeUnit.SECONDS)
        }
        assertEquals(4, MediaHttpBudget.available())
    }

    private class Connection(url: URL) : HttpURLConnection(url) {
        override fun connect() {}
        override fun disconnect() {}
        override fun usingProxy() = false
    }
    @Test fun playersAndThumbnailsAcrossHostsShareOneCapAndCloseExactlyOnce() {
        assertEquals(4, MediaHttpBudget.available())
        val pool = Executors.newFixedThreadPool(10)
        val firstFour = CountDownLatch(4)
        val release = CountDownLatch(1)
        val active = AtomicInteger()
        val peak = AtomicInteger()
        val jobs = (0 until 10).map { i -> pool.submit {
            val c = MediaHttpBudget.open(URL("https://host${i % 2}.example/file"), if (i % 2 == 0) "player" else "thumbnail",
                { Connection(it) })
            val n = active.incrementAndGet(); peak.accumulateAndGet(n, ::maxOf)
            firstFour.countDown()
            try { assertTrue(release.await(5, TimeUnit.SECONDS)) }
            finally { active.decrementAndGet(); MediaHttpBudget.close(c); MediaHttpBudget.close(c) }
        } }
        try {
            assertTrue(firstFour.await(5, TimeUnit.SECONDS))
            assertEquals(4, active.get())
            assertEquals(0, MediaHttpBudget.available())
            release.countDown()
            jobs.forEach { it.get(5, TimeUnit.SECONDS) }
            assertTrue(peak.get() <= 4)
        } finally { release.countDown(); pool.shutdownNow(); pool.awaitTermination(5, TimeUnit.SECONDS) }
        assertEquals(0, MediaHttpBudget.active())
        assertEquals(4, MediaHttpBudget.available())
    }
    @Test fun cancelledPermitWaitDoesNotOpenOrLeak() {
        val held = (0 until 4).map { MediaHttpBudget.open(URL("https://video.example/$it"), "test", { Connection(it) }) }
        val cancelled = AtomicBoolean()
        val opened = AtomicBoolean()
        val pool = Executors.newSingleThreadExecutor()
        val job = pool.submit<Boolean> {
            try {
                MediaHttpBudget.open(URL("https://other.example/file"), "thumbnail", {
                    opened.set(true); Connection(it)
                }) { if (cancelled.get()) throw InterruptedIOException("cancelled") }
                false
            } catch (_: InterruptedIOException) { true }
        }
        try {
            cancelled.set(true)
            assertTrue(job.get(3, TimeUnit.SECONDS))
            assertFalse(opened.get())
        } finally { held.forEach { MediaHttpBudget.close(it) }; pool.shutdownNow() }
        assertEquals(4, MediaHttpBudget.available())
    }
    @Test fun ownedReleaseRunsExactlyOnceOnCloseAndFactoryFailure() {
        val calls=AtomicInteger()
        val c=MediaHttpBudget.openOwned(URL("https://video.example/file"),"thumbnail",{ Connection(it) },{}, { calls.incrementAndGet() })
        MediaHttpBudget.close(c); MediaHttpBudget.close(c)
        assertEquals(1,calls.get())
        try {
            MediaHttpBudget.openOwned(URL("https://video.example/file"),"thumbnail",{ throw IOException("factory failed") },{}, { calls.incrementAndGet() })
            fail("expected failure")
        } catch(_: IOException) {}
        assertEquals(2,calls.get()); assertEquals(4,MediaHttpBudget.available())
    }
    @Test fun failedConnectionFactoryReturnsItsPermit() {
        try {
            MediaHttpBudget.open(URL("https://video.example/file"), "test", { throw IOException("connect failed") })
            fail("expected failure")
        } catch (_: IOException) {}
        assertEquals(4, MediaHttpBudget.available())
        assertEquals(0, MediaHttpBudget.active())
    }
    @Test fun cancellingOneSourceLeavesPlayerAndOtherSourceOpen() {
        val url = URL("https://video.example/file")
        val a = MediaHttpBudget.open(url, "source-a", { Connection(it) })
        val b = MediaHttpBudget.open(url, "source-b", { Connection(it) })
        val player = MediaHttpBudget.open(url, "player", { Connection(it) })
        try {
            MediaHttpBudget.cancelOwner("source-a")
            assertEquals(2, MediaHttpBudget.active())
            MediaHttpBudget.close(a)
            assertEquals(2, MediaHttpBudget.active())
        } finally {
            MediaHttpBudget.close(a); MediaHttpBudget.close(b); MediaHttpBudget.close(player)
        }
        assertEquals(4, MediaHttpBudget.available())
    }
}
