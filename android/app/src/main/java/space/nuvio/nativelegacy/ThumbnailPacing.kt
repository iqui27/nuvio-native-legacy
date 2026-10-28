package space.nuvio.nativelegacy

/** Experimental source-neutral pacing. Connection count remains owned by MediaHttpBudget. */
internal class ThumbnailPacing {
    data class Policy(val intervalMs: Long, val bytesPerSecond: Long)
    private var fast = false
    private var healthySince = -1L
    private var blockedUntil = 0L
    fun failure(now: Long, cooldownMs: Long = 60_000) { blockedUntil = maxOf(blockedUntil, now + cooldownMs); healthySince = -1; fast = false }
    fun policy(now: Long, enabled: Boolean, ready: Boolean, playing: Boolean,
               bufferMs: Long, sampleAgeMs: Long, allocatedBytes: Long = -1,
               targetBytes: Long = -1, stable: Boolean = false): Policy {
        if (!enabled) { fast = false; return NORMAL }
        val byteFull = targetBytes > 0 && allocatedBytes >= targetBytes * (if (fast) 80 else 90) / 100
        val byteHealthy = byteFull && stable && bufferMs >= (if (fast) 2500 else 3000)
        if (!ready || !playing || bufferMs < 2000 || sampleAgeMs !in 0..2000) {
            failure(now, 15_000)
            return NORMAL
        }
        if (bufferMs < 8000 && !byteHealthy) {
            fast = false; healthySince = -1
            return NORMAL
        }
        if (healthySince < 0) healthySince = now
        return if (now >= blockedUntil && now - healthySince >= 10_000 && (bufferMs >= (if (fast) 8000 else 10_000) || byteHealthy))
            FAST.also { fast = true } else NORMAL
    }
    companion object {
        val NORMAL = Policy(1000, 5_000_000)
        val FAST = Policy(125, 20_000_000)
    }
}
