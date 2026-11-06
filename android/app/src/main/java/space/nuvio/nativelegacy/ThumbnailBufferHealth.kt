package space.nuvio.nativelegacy

internal class ThumbnailBufferHealth {
    private val samples = java.util.ArrayDeque<Pair<Long, Long>>()
    var reason="unobserved"; private set
    var referenceBufferMs=0L; private set
    var windowMs=0L; private set
    var sampleGapMs=0L; private set
    fun sample(now: Long, bufferMs: Long, playing: Boolean): Boolean {
        if (!playing) { samples.clear();reason="not-playing";windowMs=0;return false }
        sampleGapMs=if(samples.isNotEmpty()) now-samples.last.first else 0
        val stale=samples.isNotEmpty() && sampleGapMs>2000
        if (stale) samples.clear()
        samples.addLast(now to bufferMs)
        while (samples.size > 1 && now - samples.first.first > 5000) samples.removeFirst()
        referenceBufferMs=samples.first.second;windowMs=now-samples.first.first
        reason=when {stale -> "stale-history";windowMs<4000 -> "short-history";bufferMs<referenceBufferMs-1000 -> "buffer-draining";else -> "stable"}
        return reason=="stable"
    }
}
