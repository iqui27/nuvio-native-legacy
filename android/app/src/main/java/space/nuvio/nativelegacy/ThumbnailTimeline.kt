package space.nuvio.nativelegacy

import java.net.HttpURLConnection
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicBoolean

/** One exclusive wall clock; activity scopes may overlap across threads. */
internal class ThumbnailTimeline(private val now: () -> Long = System::nanoTime) {
    enum class State { BODY, HEADER, PACING, PLAYBACK_GATE, PERMIT, DECODE_ONLY, DISK, SCHEDULER_IDLE }
    private val active = IntArray(State.entries.size)
    private val totals = LongArray(State.entries.size)
    private var started = false
    private var start = 0L
    private var last = 0L
    @Synchronized fun start(elapsedMs: Long = 0) {
        totals.fill(0)
        last = now(); start = last - elapsedMs * 1_000_000
        totals[State.SCHEDULER_IDLE.ordinal] = elapsedMs * 1_000_000
        started = true
    }
    private fun flush(t: Long) {
        if (!started) return
        val state = active.indexOfFirst { it > 0 }.takeIf { it >= 0 } ?: State.SCHEDULER_IDLE.ordinal
        totals[state] += (t - last).coerceAtLeast(0)
        last = t
    }
    @Synchronized fun enter(state: State): AutoCloseable {
        flush(now()); active[state.ordinal]++
        val closed = AtomicBoolean()
        return AutoCloseable {
            if (closed.compareAndSet(false,true)) synchronized(this) {
                flush(now()); check(active[state.ordinal] > 0); active[state.ordinal]--
            }
        }
    }
    @Synchronized fun snapshot(): LongArray {
        val t = now(); flush(t)
        return totals.copyOf() + if (started) t - start else 0L
    }
}

internal object ThumbnailPipelineTimeline {
    val clock = ThumbnailTimeline()
    private val bodies = ConcurrentHashMap<HttpURLConnection,AutoCloseable>()
    fun enter(state: ThumbnailTimeline.State) = clock.enter(state)
    fun bodyStart(c: HttpURLConnection) {
        val token = enter(ThumbnailTimeline.State.BODY)
        if (bodies.putIfAbsent(c,token) != null) token.close()
    }
    fun bodyEnd(c: HttpURLConnection?) { if (c != null) bodies.remove(c)?.close() }
    fun summary(nativeMs: Long): String {
        val values = clock.snapshot()
        val wall = values.last()
        val sum = values.dropLast(1).sum()
        val error = if (nativeMs > 0) kotlin.math.abs(wall / 1_000_000.0 - nativeMs) / nativeMs * 100 else 0.0
        return ThumbnailTimeline.State.entries.mapIndexed { i,s -> "${s.name.lowercase()}Ms=${values[i]/1_000_000}" }.joinToString(" ") +
            " sumMs=${sum/1_000_000} wallMs=${wall/1_000_000} nativeWallMs=$nativeMs accountingErrorNs=${sum-wall} nativeErrorPct=%.3f pass=${sum==wall && error<=1.0}".format(error)
    }
}
