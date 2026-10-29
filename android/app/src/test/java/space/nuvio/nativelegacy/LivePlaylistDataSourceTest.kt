package space.nuvio.nativelegacy

import android.net.Uri
import androidx.media3.common.C
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import org.junit.Assert.*
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.IOException

class LivePlaylistDataSourceTest {
    private fun playlist(sequence: Long = 100, extra: String = "", count: Int = 6): String =
        "#EXTM3U\n#EXT-X-TARGETDURATION:12\n#EXT-X-MEDIA-SEQUENCE:$sequence\n$extra" +
            (0 until count).joinToString("") { "#EXTINF:12.0,\nsegment-${sequence + it}.ts\n" }

    private class Source(private val response: (DataSpec) -> String) : DataSource {
        var input: ByteArrayInputStream? = null
        var location: Uri? = null
        var redirect: Uri? = null
        var contentType = "application/vnd.apple.mpegurl"
        var closes = 0
        override fun open(spec: DataSpec): Long {
            location = redirect ?: spec.uri
            val bytes = response(spec).toByteArray()
            input = ByteArrayInputStream(bytes)
            return bytes.size.toLong()
        }
        override fun read(buffer: ByteArray, offset: Int, length: Int): Int = input!!.read(buffer, offset, length)
        override fun getUri(): Uri? = location
        override fun getResponseHeaders(): Map<String, List<String>> = mapOf("Content-Type" to listOf(contentType))
        override fun addTransferListener(listener: TransferListener) = Unit
        override fun close() { closes++; input = null }
    }

    private class Harness {
        var now = 0L
        var body = ""
        var fresh = ""
        var fail = false
        var slow = false
        val requests = ArrayList<DataSpec>()
        val sources = ArrayList<Source>()
        val refreshSources = ArrayList<Source>()
        var contentType = "application/vnd.apple.mpegurl"
        var redirect: Uri? = null
        val factory = LivePlaylistDataSource.Factory(
            DataSource.Factory { Source { body }.also { it.contentType = contentType; sources += it } },
            DataSource.Factory { Source {
                requests += it
                if (fail) throw IOException("network unavailable")
                if (slow) now += 4000
                fresh
            }.also { it.redirect = redirect; refreshSources += it } },
            clock = { now },
        )
        fun read(spec: DataSpec = DataSpec(Uri.parse("https://example.test/live.m3u8?signature=keep"))): String {
            val source = factory.createDataSource()
            try {
                source.open(spec)
                val out = java.io.ByteArrayOutputStream()
                val buffer = ByteArray(4096)
                assertEquals(0, source.read(buffer, 0, 0))
                while (true) {
                    val n = source.read(buffer, 0, buffer.size)
                    if (n == C.RESULT_END_OF_INPUT) break
                    out.write(buffer, 0, n)
                }
                return out.toString("UTF-8")
            } finally { source.close() }
        }
    }

    @Test fun stalePlaylistIsRevalidatedBeforeBufferedPlaybackNeedsAnError() {
        val h = Harness(); h.body = playlist(); h.fresh = playlist(102)
        assertEquals(h.body, h.read())
        h.now = 12000; assertEquals(h.body, h.read()); assertTrue(h.requests.isEmpty())
        h.now = 18000; assertEquals(h.fresh, h.read())
        assertEquals(1, h.requests.size)
        assertEquals("no-cache, max-age=0", h.requests.single().httpRequestHeaders["Cache-Control"])
        assertEquals("https://example.test/live.m3u8?signature=keep", h.requests.single().uri.toString())
        assertTrue(h.sources.all { it.closes == 1 })
        assertTrue(h.refreshSources.all { it.closes == 1 })
    }

    @Test fun backwardCachedPlaylistCanBeReplacedWithoutRewritingAnySegments() {
        val h = Harness(); h.body = playlist(100); h.read()
        h.now = 1000; h.body = playlist(1); h.fresh = playlist(101)
        assertEquals(h.fresh, h.read()); assertEquals(1, h.requests.size)
    }

    @Test fun genuineOriginResetIsLeftForExistingPlayerRecovery() {
        val h = Harness(); h.body = playlist(100); h.read()
        h.now = 1000; h.body = playlist(1); h.fresh = h.body
        assertEquals(h.body, h.read()); assertEquals(1, h.requests.size)
    }

    @Test fun advancingNormalPlaylistsNeverMakeExtraRequests() {
        val h = Harness()
        repeat(30) { h.now = it * 12000L; h.body = playlist(100L + it); assertEquals(h.body, h.read()) }
        assertTrue(h.requests.isEmpty())
    }

    @Test fun retriesAreBoundedAcrossAllRenditionsAndResumeAfterOneMinute() {
        val h = Harness(); h.body = playlist(); h.fresh = h.body; h.read()
        for (time in listOf(18000L, 19000L, 33000L, 48000L, 60000L)) { h.now = time; h.read() }
        assertEquals(2, h.requests.size)
        h.now = 78000; h.read(); assertEquals(3, h.requests.size)
    }

    @Test fun failedRefreshPreservesTheSuccessfulOriginalResponse() {
        val h = Harness(); h.body = playlist(); h.read()
        h.now = 18000; h.fail = true
        assertEquals(h.body, h.read()); assertEquals(1, h.refreshSources.single().closes)
    }

    @Test fun slowRefreshPreservesTheOriginalAndClosesItsConnection() {
        val h = Harness(); h.body = playlist(); h.read()
        h.now = 18000; h.slow = true; h.fresh = playlist(101)
        assertEquals(h.body, h.read()); assertEquals(1, h.refreshSources.single().closes)
    }

    @Test fun authAndSignedUrlArePreservedButConditionalCacheHeadersAreRemoved() {
        val h = Harness(); h.body = playlist(); h.fresh = playlist(101)
        val spec = DataSpec.Builder().setUri("https://example.test/live.m3u8?token=signed")
            .setHttpRequestHeaders(mapOf("Authorization" to "Bearer test", "Referer" to "https://example.test/",
                "If-None-Match" to "old", "if-modified-since" to "old", "cache-control" to "max-age=60"))
            .build()
        h.read(spec); h.now = 18000; h.read(spec)
        val request = h.requests.single()
        assertEquals(spec.uri, request.uri)
        assertEquals("Bearer test", request.httpRequestHeaders["Authorization"])
        assertEquals("https://example.test/", request.httpRequestHeaders["Referer"])
        assertFalse(request.httpRequestHeaders.keys.any { it.startsWith("if-", true) })
    }

    @Test fun vodMasterAndLowLatencyPlaylistsAreUnchanged() {
        for (body in listOf(playlist(extra = "#EXT-X-ENDLIST\n"), playlist(extra = "#EXT-X-PLAYLIST-TYPE:VOD\n"),
            playlist(extra = "#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES\n"), playlist(extra = "#EXT-X-PART:DURATION=0.5,URI=\"part\"\n"),
            playlist(extra = "#EXT-X-SKIP:SKIPPED-SEGMENTS=2\n"), "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000\nvariant.m3u8\n")) {
            val h = Harness(); h.body = body; h.read(); h.now = 60000
            assertEquals(body, h.read()); assertTrue(h.requests.isEmpty())
        }
    }

    @Test fun largeManifestsAreStreamedCompletelyWithoutRevalidation() {
        val h = Harness(); h.body = playlist() + "#".repeat(300000)
        assertEquals(h.body, h.read()); h.now = 60000
        assertEquals(h.body, h.read()); assertTrue(h.requests.isEmpty())
    }

    @Test fun mediaBytesAndRangeRequestsAreNotBufferedOrRefreshed() {
        val h = Harness(); h.body = playlist(); h.contentType = "video/mp2t"
        val media = DataSpec(Uri.parse("https://example.test/segment.ts"))
        h.read(media); h.now = 60000; assertEquals(h.body, h.read(media))
        val range = DataSpec.Builder().setUri("https://example.test/live.m3u8").setPosition(1).setLength(10).build()
        h.read(range); h.now = 120000; h.read(range); assertTrue(h.requests.isEmpty())
    }

    @Test fun extensionlessManifestIsRecognizedByResponseType() {
        val h = Harness(); h.body = playlist(); h.fresh = playlist(101)
        val spec = DataSpec(Uri.parse("https://example.test/live?token=test"))
        h.read(spec); h.now = 18000; assertEquals(h.fresh, h.read(spec)); assertEquals(1, h.requests.size)
    }

    @Test fun refreshWithDifferentRedirectBaseCannotChangeRelativeSegmentResolution() {
        val h = Harness(); h.body = playlist(); h.fresh = playlist(101); h.read()
        h.now = 18000; h.redirect = Uri.parse("https://other.test/different/live.m3u8")
        assertEquals(h.body, h.read())
    }

    @Test fun olderRevalidationCannotReplaceNewerSegments() {
        val h = Harness(); h.body = playlist(); h.fresh = playlist(1); h.read()
        h.now = 18000; assertEquals(h.body, h.read())
    }

    @Test fun invalidOrOversizedRefreshCannotReplaceOriginal() {
        for (body in listOf("<html>temporarily unavailable</html>", playlist(101) + "#".repeat(300000))) {
            val h = Harness(); h.body = playlist(); h.fresh = body; h.read()
            h.now = 18000; assertEquals(h.body, h.read())
        }
    }
}
