package space.nuvio.nativelegacy

import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.net.SocketTimeoutException

/** HttpURLConnection does not automatically follow HTTPS -> HTTP. Match the player's GET behavior. */
internal object ThumbnailHttpRedirects {
    fun open(url: String, headers: List<Pair<String, String>>, range: String, closeSocket: Boolean,
             observe: (String) -> Unit,
             connect: (URL) -> HttpURLConnection = { MediaHttpBudget.open(it, "thumbnail") }): HttpURLConnection {
        var target = URL(url)
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
                for ((key, value) in headers) c.setRequestProperty(key, value)
                c.setRequestProperty("Accept-Encoding", "identity")
                c.setRequestProperty("Range", range)
                if (closeSocket) c.setRequestProperty("Connection", "close")
                val code = ThumbnailPipelineTimeline.enter(ThumbnailTimeline.State.HEADER).use { c.responseCode }
                if (code in setOf(301, 302, 303, 307, 308)) {
                    val location = c.getHeaderField("Location")
                    if (location.isNullOrBlank()) throw IOException("redirect without location")
                    target = URL(target, location)
                    MediaHttpBudget.close(c)
                } else {
                    c.readTimeout = 6000 // retain the existing body-read timeout
                    if (code == 200 || code == 206) ThumbnailPipelineTimeline.bodyStart(c)
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
