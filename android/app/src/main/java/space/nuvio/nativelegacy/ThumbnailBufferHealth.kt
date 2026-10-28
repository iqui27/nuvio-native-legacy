package space.nuvio.nativelegacy

internal class ThumbnailBufferHealth {
    private val samples = java.util.ArrayDeque<Pair<Long, Long>>()
    fun sample(now: Long, bufferMs: Long, playing: Boolean): Boolean {
        if (!playing) { samples.clear(); return false }
        if (samples.isNotEmpty() && now - samples.last.first > 2000) samples.clear()
        samples.addLast(now to bufferMs)
        while (samples.size > 1 && now - samples.first.first > 5000) samples.removeFirst()
        return now - samples.first.first >= 4000 && bufferMs >= samples.first.second - 1000
    }
}
