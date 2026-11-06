package space.nuvio.nativelegacy

import org.junit.Assert.*
import org.junit.Test
import java.net.HttpURLConnection
import java.net.URL
import java.io.IOException

class ThumbnailHttpRedirectsTest {
    private class Response(url: URL, val code: Int, val location: String? = null) : HttpURLConnection(url) {
        var disconnected = false
        override fun connect() {}
        override fun disconnect() { disconnected = true }
        override fun usingProxy() = false
        override fun getResponseCode() = code
        override fun getHeaderField(name: String) = if (name == "Location") location else null
    }
    @Test fun crossOriginRedirectDoesNotForwardFakeCredentials() {
        val destination = com.sun.net.httpserver.HttpServer.create(java.net.InetSocketAddress("127.0.0.1", 0), 0)
        val source = com.sun.net.httpserver.HttpServer.create(java.net.InetSocketAddress("127.0.0.1", 0), 0)
        var sourceAuth: String? = null
        var destinationAuth: String? = null
        destination.createContext("/end") { exchange ->
            destinationAuth = exchange.requestHeaders.getFirst("Authorization")
            exchange.sendResponseHeaders(206, -1)
            exchange.close()
        }
        source.createContext("/start") { exchange ->
            sourceAuth = exchange.requestHeaders.getFirst("Authorization")
            exchange.responseHeaders.add("Location", "http://127.0.0.1:${destination.address.port}/end")
            exchange.sendResponseHeaders(302, -1)
            exchange.close()
        }
        source.start(); destination.start()
        try {
            val c = ThumbnailHttpRedirects.open("http://127.0.0.1:${source.address.port}/start",
                listOf("Authorization" to "synthetic-only"), "bytes=0-0", true, {})
            assertEquals(206, c.responseCode)
            MediaHttpBudget.close(c)
            assertEquals("synthetic-only", sourceAuth)
            assertNull(destinationAuth)
        } finally { source.stop(0); destination.stop(0) }
    }

    @Test fun repeatedRequestsReuseDestinationWithoutSourceCredentials() {
        val session = ThumbnailHttpRedirects.Session("https://source.invalid/start", listOf("Authorization" to "synthetic-only"))
        val requests = mutableListOf<Response>()
        val connect: (URL) -> HttpURLConnection = { url ->
            Response(url, if (requests.isEmpty()) 302 else 206,
                if (requests.isEmpty()) "https://cdn.invalid/file" else null).also { requests.add(it) }
        }
        session.open("bytes=0-0", true, connect)
        session.open("bytes=1-1", true, connect)
        assertEquals(3, requests.size)
        assertEquals("cdn.invalid", session.host())
        assertEquals("synthetic-only", requests[0].getRequestProperty("Authorization"))
        assertNull(requests[1].getRequestProperty("Authorization"))
        assertNull(requests[2].getRequestProperty("Authorization"))
    }

    @Test fun expiredDestinationRefreshesThroughOriginalSource() {
        val session = ThumbnailHttpRedirects.Session("https://source.invalid/start", listOf("Authorization" to "synthetic-only"))
        val requests = mutableListOf<Response>()
        val connect: (URL) -> HttpURLConnection = { url ->
            val step = requests.size
            Response(url, when (step) { 0, 3 -> 302; 2 -> 403; else -> 206 },
                if (step == 0 || step == 3) "https://cdn.invalid/file" else null).also { requests.add(it) }
        }
        session.open("bytes=0-0", true, connect)
        assertEquals(206, session.open("bytes=1-1", true, connect).responseCode)
        assertEquals(5, requests.size)
        assertEquals("synthetic-only", requests[3].getRequestProperty("Authorization"))
        assertNull(requests[4].getRequestProperty("Authorization"))
        assertTrue(requests[2].disconnected)
    }

    @Test fun staleConcurrentResponseCannotReplaceNewDestination() {
        val session = ThumbnailHttpRedirects.Session("https://source.invalid/start", listOf("Authorization" to "synthetic-only"))
        val waiting = java.util.concurrent.CountDownLatch(1)
        val release = java.util.concurrent.CountDownLatch(1)
        val connect: (URL) -> HttpURLConnection = { url ->
            if (url.host == "source.invalid") {
                val destination = if (Thread.currentThread().name == "old") "old.invalid" else "new.invalid"
                Response(url, 302, "https://$destination/file")
            } else {
                if (url.host == "old.invalid") { waiting.countDown(); check(release.await(5, java.util.concurrent.TimeUnit.SECONDS)) }
                Response(url, 206)
            }
        }
        var failure: Throwable? = null
        val old = Thread({ try { session.open("bytes=0-0", true, connect) } catch (t: Throwable) { failure = t } }, "old")
        old.start()
        try {
            assertTrue(waiting.await(5, java.util.concurrent.TimeUnit.SECONDS))
            session.open("bytes=1-1", true, connect)
        } finally { release.countDown(); old.join(5000) }
        assertNull(failure)
        assertFalse(old.isAlive)
        assertEquals("new.invalid", session.host())
    }

    @Test fun sameOriginKeepsHeadersButCrossOriginPermanentlyDropsThem() {
        val connections = mutableListOf<Response>()
        val response = ThumbnailHttpRedirects.open("https://source.invalid/start",
            listOf("Authorization" to "synthetic-only", "Cookie" to "synthetic-only", "Accept" to "video/*", "X-Token" to "synthetic-only"), "bytes=0-0", true, {}) { url ->
            val location = when (connections.size) {
                0 -> "/same"
                1 -> "https://other.invalid/other"
                2 -> "https://source.invalid/back"
                else -> null
            }
            Response(url, if (location == null) 206 else 302, location).also { connections.add(it) }
        }
        assertEquals(206, response.responseCode)
        for (c in connections.take(2)) {
            assertEquals("synthetic-only", c.getRequestProperty("Authorization"))
            assertEquals("synthetic-only", c.getRequestProperty("Cookie"))
        }
        for (c in connections.drop(2)) {
            assertNull(c.getRequestProperty("Authorization"))
            assertNull(c.getRequestProperty("Cookie"))
            assertNull(c.getRequestProperty("X-Token"))
            assertEquals("video/*", c.getRequestProperty("Accept"))
        }
    }

    @Test fun followsHttpsToHttp307AndPreservesRangeAndRequiredHeaders() {
        val connections = mutableListOf<Response>()
        val seen = mutableListOf<String>()
        val final = ThumbnailHttpRedirects.open("https://pengu.example/start", listOf("Referer" to "https://addon.example/", "Authorization" to "synthetic-only"),
            "bytes=100-199,300-399", true, { seen.add(it) }) { url ->
            Response(url, if (connections.isEmpty()) 307 else 206,
                if (connections.isEmpty()) "http://video.example/movie" else null).also { connections.add(it) }
        }
        assertEquals(2, connections.size)
        assertEquals("http", final.url.protocol)
        assertTrue(connections.first().disconnected)
        assertFalse(connections.last().disconnected)
        assertEquals(2, seen.size)
        for (c in connections) {
            assertEquals("bytes=100-199,300-399", c.getRequestProperty("Range"))
            if (c === connections.first()) assertEquals("https://addon.example/", c.getRequestProperty("Referer"))
            else assertNull(c.getRequestProperty("Referer"))
            if (c === connections.first()) assertEquals("synthetic-only", c.getRequestProperty("Authorization"))
            else assertNull(c.getRequestProperty("Authorization"))
            assertEquals("identity", c.getRequestProperty("Accept-Encoding"))
            assertEquals("close", c.getRequestProperty("Connection"))
            assertFalse(c.instanceFollowRedirects)
        }
    }
    @Test fun relativeRedirectUsesItsCurrentBaseAndAllHopsAreObserved() {
        val seen = mutableListOf<String>()
        var calls = 0
        val response = ThumbnailHttpRedirects.open("https://video.example/start", emptyList(), "bytes=0-0", false,
            { seen.add(it) }) { url ->
            when (calls++) {
                0 -> Response(url, 302, "/next")
                1 -> { assertEquals("/next", url.path); Response(url, 307, "https://nexus-130.neur.tb-cdn.st/file") }
                else -> Response(url, 206)
            }
        }
        assertEquals(206, response.responseCode)
        assertEquals(3, seen.size)
        assertTrue(seen.last().contains("tb-cdn.st"))
    }
    @Test fun redirectLoopIsBoundedAndClosesEveryHop() {
        val connections = mutableListOf<Response>()
        try {
            ThumbnailHttpRedirects.open("https://video.example/loop", emptyList(), "bytes=0-0", true, {}) { url ->
                Response(url, 307, "/loop").also { connections.add(it) }
            }
            fail("must reject redirect loop")
        } catch (_: IOException) {}
        assertEquals(10, connections.size)
        assertTrue(connections.all { it.disconnected })
    }
    @Test fun missingLocationAndNonHttpRedirectAreRejected() {
        for (location in listOf(null, "file:///movie.mp4")) {
            val c = Response(URL("https://video.example/file"), 307, location)
            try {
                ThumbnailHttpRedirects.open(c.url.toString(), emptyList(), "bytes=0-0", true, {}) { c }
                fail("invalid redirect must fail")
            } catch (_: IOException) {}
            assertTrue(c.disconnected)
        }
    }
    @Test fun httpErrorsAreReturnedToExistingAdmissionAndBackoffRules() {
        val c = Response(URL("https://video.example/file"), 429)
        assertSame(c, ThumbnailHttpRedirects.open(c.url.toString(), emptyList(), "bytes=0-0", true, {}) { c })
    }

    @Test fun playerPermitWaitDoesNotConsumeTheRedirectHeaderDeadline() {
        val held = (0 until 4).map {
            MediaHttpBudget.open(URL("https://player.example/$it"), "test", { url -> Response(url, 206) })
        }
        val releaser = Thread { Thread.sleep(6200); held.forEach { MediaHttpBudget.close(it) } }.apply { start() }
        var calls = 0
        var result: HttpURLConnection? = null
        try {
            result = ThumbnailHttpRedirects.open("https://video.example/start", emptyList(), "bytes=0-0", true, {}) { url ->
                MediaHttpBudget.open(url, "thumbnail-test", {
                    if (calls++ == 0) Response(it, 307, "/final") else Response(it, 206)
                })
            }
            assertEquals(206, result.responseCode)
            assertEquals(2, calls)
        } finally {
            MediaHttpBudget.close(result)
            releaser.join(8000)
            held.forEach { MediaHttpBudget.close(it) }
        }
        assertEquals(4, MediaHttpBudget.available())
    }
}
