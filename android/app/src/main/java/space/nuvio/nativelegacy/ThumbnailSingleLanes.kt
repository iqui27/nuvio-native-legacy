package space.nuvio.nativelegacy

import java.io.IOException
import java.util.concurrent.ExecutionException
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.Future

// PARALLEL SINGLE RANGES for servers that refuse multi-range (05/11/2026,
// Cloudflare R2 behind PenguPlay "4KHDHub Â· R2", Dune Part Two 64.9 GB):
// R2 answers a multi-range GET with HTTP 400 (application/xml), so each
// keyframe is its own request, and R2 took ~840 ms (median, p90 1050 ms)
// before its headers versus ~160 ms for the 1.2 MB body. One request at a time
// that is ~1 thumbnail per second (995 targets ~ 17 min) while the link idles.
//
// TorBox's conclusion (one multipart stream beats more lanes) is about a
// bandwidth-bound link; this one is latency-bound, so a few requests in
// flight hide the wait. Only used when multi-range is off AND the pacing is
// FAST (healthy, stable playback buffer); MediaHttpBudget still caps the
// thumbnail + player connections at 4 and the playback gate still holds HTTP.
internal object ThumbnailSingleLanes {
    const val LANES = 3
    // With its own slots (MediaHttpBudget.EXTRA) the storage host's ~865 ms wait is hidden by more requests in
    // flight: ~6 x 1.4 MB per ~1.05 s is ~65 Mbit/s next to the player, and FAST pacing drops this to 1 the
    // moment the playback buffer stops being stable.
    // 06/11 R2: 8 own slots (12 connections with the player) got HTTP 403 within 20 s; 6 (<= 10) never did. 9 = the
    // 6 own slots + up to 3 borrowed idle player slots (MediaHttpBudget.BORROW_MAX): still <= 10 connections in all.
    const val LANES_OWN = 9

    private val pool: ExecutorService by lazy {
        Executors.newFixedThreadPool(LANES_OWN) { r -> Thread(r, "thumbnail-range").apply { isDaemon = true } }
    }

    fun lanes(multiRangeOk: Boolean, fastPacing: Boolean, ownSlots: Boolean = false, learnedCap: Int? = null): Int =
        if (multiRangeOk || !fastPacing) 1
        else if (ownSlots) minOf(LANES_OWN, learnedCap?.coerceAtLeast(MIN_CAP) ?: LANES_OWN)
        else LANES

    // LEARNED LANE LIMIT (06/11/2026, R2: HTTP 403 once 8 lanes + the player's 4 were open). A 403/429 is read as
    // "too many connections" only when it cannot be a dead link: >= 3 thumbnail requests in flight AND one to the
    // same host succeeded within the last 10 s. Then one lane fewer than were in flight, never below MIN_CAP; null
    // means a real refusal (the title stops, as before).
    const val MIN_CAP = 2
    fun capAfterRefusal(inFlight: Int, msSinceLastOk: Long): Int? =
        if (inFlight >= 3 && msSinceLastOk in 0..10_000) maxOf(MIN_CAP, inFlight - 1) else null

    /** A 403/429 read as a connection limit (see [capAfterRefusal]): retried, not a refusal of the title. */
    class LaneLimit(code: Int) : IOException("http $code (connection limit)")

        /** In-flight limit for the async look-ahead that can be lowered while requests run. */
    class Gate(limit: Int) {
        private var limit = limit
        private var active = 0
        @Synchronized fun active() = active
        @Synchronized fun limit() = limit
        @Synchronized fun setLimit(n: Int) { limit = maxOf(1, n); (this as Object).notifyAll() }
        fun <T> run(f: () -> T): T {
            synchronized(this) {
                while (active >= limit) (this as Object).wait()
                active++
            }
            try { return f() } finally { synchronized(this) { active--; (this as Object).notifyAll() } }
        }
    }

    // Keep what fits in the source's RAM cache, so a range is not evicted
    // before the decoder reads it; always at least one.
    fun capBytes(ranges: List<LongArray>, maxBytes: Long): List<LongArray> {
        val out = ArrayList<LongArray>()
        var sum = 0L
        for (r in ranges) {
            val n = r[1] - r[0] + 1
            if (out.isNotEmpty() && sum + n > maxBytes) break
            out += r; sum += n
        }
        return out
    }

    // Results in the order of `ranges`. The first failure cancels the rest and
    // is rethrown as the IOException the fetch raised.
    fun <T> fetchAll(ranges: List<LongArray>, executor: ExecutorService = pool, lanes: Int = LANES_OWN,
                     fetch: (LongArray) -> T): List<T> {
        // At most `lanes` in flight although the shared pool has LANES_OWN threads.
        val gate = java.util.concurrent.Semaphore(lanes)
        val futures = ranges.map { r -> executor.submit<T> {
            gate.acquire()
            try { fetch(r) } finally { gate.release() }
        } }
        try {
            return futures.map { f -> unwrap(f) }
        } finally {
            for (f in futures) if (!f.isDone) f.cancel(true)
        }
    }

    // One range on the shared lanes pool, for the look-ahead that keeps running while the batch decodes.
    fun <T> submit(executor: ExecutorService = pool, fetch: () -> T): Future<T> = executor.submit<T> { fetch() }
    fun <T> await(f: Future<T>): T = unwrap(f)

    private fun <T> unwrap(f: Future<T>): T = try { f.get() } catch (e: ExecutionException) {
        when (val c = e.cause) {
            is IOException -> throw c
            is RuntimeException -> throw c
            is Error -> throw c
            else -> throw IOException(c)
        }
    } catch (e: java.util.concurrent.CancellationException) {
        // A look-ahead cancelled by close(): the callers only expect IOException.
        throw java.io.InterruptedIOException("thumbnail range cancelled")
    } catch (e: InterruptedException) {
        Thread.currentThread().interrupt()
        throw java.io.InterruptedIOException("thumbnail range lanes interrupted")
    }
}
