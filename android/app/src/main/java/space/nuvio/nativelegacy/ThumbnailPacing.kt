package space.nuvio.nativelegacy

/** Experimental source-neutral pacing. Connection count remains owned by MediaHttpBudget. */
internal class ThumbnailPacing {
    data class Policy(val intervalMs: Long, val bytesPerSecond: Long)
    private var fast = false
    private var healthySince = -1L
    private var blockedUntil = 0L
    private var everHealthy = false
    var reason = "init"; private set   // why the last policy() answered what it did (logged on each change)   // a buffer still FILLING at startup is not a drain
    fun failure(now: Long, cooldownMs: Long = 60_000) { blockedUntil = maxOf(blockedUntil, now + cooldownMs); healthySince = -1; fast = false }
    fun policy(now: Long, enabled: Boolean, ready: Boolean, playing: Boolean,
               bufferMs: Long, sampleAgeMs: Long, allocatedBytes: Long = -1,
               targetBytes: Long = -1, stable: Boolean = false): Policy {
        if (!enabled) { fast = false; reason = "disabled"; return NORMAL }
        val byteHealthy = ThumbnailHeadroom.byteHealthy(bufferMs, allocatedBytes, targetBytes, stable, fast)
        // Not playing YET (startup) or paused is not an incident: it only resets the healthy window. It used to
        // count as a failure, so every title began with a 15 s block before FAST could start.
        if (!ready || !playing) { fast = false; healthySince = -1; reason = if (!ready) "not-ready" else "not-playing"; return NORMAL }
        if (bufferMs < 2000 || !ThumbnailHeadroom.fresh(sampleAgeMs)) {
            // 06/11/2026 Rick and Morty: the first second of fill (< 2 s) blocked FAST for 15 s, 5.4 s of a 15.8 s run.
            reason = if (bufferMs < 2000) "buffer-low" else "stale-sample ageMs=$sampleAgeMs"
            if (everHealthy) failure(now, 15_000) else { fast = false; healthySince = -1 }
            return NORMAL
        }
        if (bufferMs >= 8000) everHealthy = true
        if (bufferMs < 8000 && !byteHealthy) {
            fast = false; healthySince = -1; reason = "shallow"
            return NORMAL
        }
        if (healthySince < 0) healthySince = now
        // A deep buffer (>= 20 s, see ThumbnailPlaybackGate) needs no 10 s warm-up; incidents still block.
        if (now >= blockedUntil && bufferMs >= ThumbnailPlaybackGate.DEEP_BUFFER_MS) { reason = "deep"; return FAST.also { fast = true } }
        reason = if (now < blockedUntil) "blocked ${blockedUntil - now} ms" else "warming"
        return if (now >= blockedUntil && now - healthySince >= 10_000 && (bufferMs >= (if (fast) 8000 else 10_000) || byteHealthy))
            FAST.also { fast = true } else NORMAL
    }
    companion object {
        val NORMAL = Policy(1000, 5_000_000)
        // Controlled TorBox range replay (Shield, 91.7 GB Dune source): an
        // 8 MiB scattered multipart body averaged ~352 Mbps at 125 ms between
        // requests, ~292 Mbps when byte pacing stretched that gap to 400 ms,
        // and ~215 Mbps at 1 s. Keep the request interval, but do not add a
        // second post-body byte delay while the health gate owns FAST mode.
        // Any unstable buffer/incident immediately returns the 5 MB/s policy.
        val FAST = Policy(125, Long.MAX_VALUE)
    }
}
