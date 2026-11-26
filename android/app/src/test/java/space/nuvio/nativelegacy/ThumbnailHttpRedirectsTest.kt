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
    @Test fun followsHttpsToHttp307AndPreservesRangeAndRequiredHeaders() {
        val connections = mutableListOf<Response>()
        val seen = mutableListOf<String>()
        val final = ThumbnailHttpRedirects.open("https://pengu.example/start", listOf("Referer" to "https://addon.example/"),
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
            assertEquals("https://addon.example/", c.getRequestProperty("Referer"))
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
