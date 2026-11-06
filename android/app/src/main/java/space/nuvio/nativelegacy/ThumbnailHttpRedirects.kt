package space.nuvio.nativelegacy

import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.net.SocketTimeoutException

/** HttpURLConnection does not automatically follow HTTPS -> HTTP. Match the player's GET behavior. */
internal object ThumbnailHttpRedirects {
    class Session(url: String, headers: List<Pair<String, String>>) {
        private val initial = url to headers.toList()
        @Volatile private var endpoint = initial
        fun open(range: String, closeSocket: Boolean, connect: (URL) -> HttpURLConnection): HttpURLConnection {
            fun request(snapshot: Pair<String, List<Pair<String, String>>>): HttpURLConnection =
                ThumbnailHttpRedirects.open(snapshot.first, snapshot.second, range, closeSocket, {}, { target, allowed ->
                    synchronized(this) {
                        if (endpoint === snapshot) endpoint = target to allowed
                    }
                }, connect)
            val snapshot = endpoint
            val first = request(snapshot)
            if (first.responseCode !in listOf(401, 403) || snapshot === initial) return first
            MediaHttpBudget.close(first)
            // An expired signed destination can be renewed only by starting a fresh source request.
            val retry = synchronized(this) {
                if (endpoint === snapshot) { endpoint = initial; initial } else endpoint
            }
            return request(retry)
        }
        fun host(): String? = try { URL(endpoint.first).host } catch (_: Exception) { null }
    }

    fun open(url: String, headers: List<Pair<String, String>>, range: String, closeSocket: Boolean,
             observe: (String) -> Unit,
             onSuccess: (String, List<Pair<String, String>>) -> Unit = { _, _ -> },
             connect: (URL) -> HttpURLConnection = { MediaHttpBudget.open(it, "thumbnail") }): HttpURLConnection {
        var target = URL(url)
        var allowedHeaders = headers
        val deadline = System.nanoTime() + 6_000_000_000L
        val permitBaseline = MediaHttpBudget.threadWaitNanos()
        repeat(10) {
            if (target.protocol.lowercase() !in setOf("http", "https")) throw IOException("unsupported redirect protocol")
            observe(target.toString())
            // Waiting for the player to release a slot is not CDN/header latency.
            val remaining = ((deadline - System.nanoTime() +
                MediaHttpBudget.threadWaitNanos() - permitBaseline) / 1_000_000).toInt()
            if (remaining <= 0) throw SocketTimeoutException("redirect header deadline")
            val c = connect(target)
            try {
                c.instanceFollowRedirects = false
                c.connectTimeout = remaining
                c.readTimeout = remaining
                for ((key, value) in allowedHeaders) c.setRequestProperty(key, value)
                c.setRequestProperty("Accept-Encoding", "identity")
                c.setRequestProperty("Range", range)
                if (closeSocket) c.setRequestProperty("Connection", "close")
                val code = ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.HEADER).use { c.responseCode }
                if (code in setOf(301, 302, 303, 307, 308)) {
                    val location = c.getHeaderField("Location")
                    if (location.isNullOrBlank()) throw IOException("redirect without location")
                    val next = URL(target, location)
                    if (!target.protocol.equals(next.protocol, ignoreCase = true) ||
                        !target.host.equals(next.host, ignoreCase = true) ||
                        (if (target.port == -1) target.defaultPort else target.port) !=
                        (if (next.port == -1) next.defaultPort else next.port)) {
                        // Only literal media preferences are known not to contain source credentials.
                        allowedHeaders = allowedHeaders.filter { (key, value) ->
                            key.equals("Accept", ignoreCase = true) && value in setOf("*/*", "video/*", "application/octet-stream")
                        }
                    }
                    target = next
                    MediaHttpBudget.close(c)
                } else {
                    c.readTimeout = 6000 // retain the existing body-read timeout
                    if (code == 200 || code == 206) {
                        onSuccess(target.toString(), allowedHeaders)
                        ThumbnailPipelineTimeline.bodyStart(c)
                    }
                    return c
                }
            } catch (e: Exception) {
                MediaHttpBudget.close(c)
                throw e
            }
        }
        throw IOException("too many redirects")
    }
}
