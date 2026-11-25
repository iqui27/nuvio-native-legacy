package space.nuvio.nativelegacy

internal data class ThumbnailSelectedCue(val index: Int, val timeNs: Long, val requestMs: Long) {
    val presentationTimeUs: Long get() = timeNs / 1000
}

/** Direct packet selection; extractor/copy seeks retain their previous-sync rule. */
internal class ThumbnailCueSelection(private val cues: LongArray) {
    private val count = cues.size / 3
    private val valid = cues.isNotEmpty() && cues.size % 3 == 0 &&
        (0 until count).all { cues[it * 3] >= 0 && (it == 0 || cues[it * 3] >= cues[(it - 1) * 3]) }

    fun matches(source: LongArray): Boolean = cues === source

    // Same at-or-before convention used by the packet reader, including its
    // first-cue clamp. This verifies the identity after the JNI millisecond trip.
    private fun previous(timeNs: Long): Int {
        var lo = 0
        var hi = count - 1
        while (lo < hi) {
            val mid = (lo + hi + 1) / 2
            if (cues[mid * 3] <= timeNs) lo = mid else hi = mid - 1
        }
        return lo
    }

    fun nearest(targetMs: Long): ThumbnailSelectedCue? {
        if (!valid || targetMs < 0 || targetMs > Long.MAX_VALUE / NS_PER_MS) return null
        val targetNs = targetMs * NS_PER_MS
        var index = previous(targetNs)
        if (targetMs == 0L) {
            // The existing decoder has a zero-PTS startup workaround. Keep slot
            // zero visible using the earliest positive decoder PTS, or report a gap.
            var lo = 0
            var hi = count
            while (lo < hi) {
                val mid = (lo + hi) / 2
                if (cues[mid * 3] < 1000) lo = mid + 1 else hi = mid
            }
            if (lo == count) return null
            index = lo
        } else if (cues[index * 3] <= targetNs && index + 1 < count &&
            cues[(index + 1) * 3] - targetNs < targetNs - cues[index * 3]) {
            index++
        }
        val timeNs = cues[index * 3]
        val distance = if (timeNs >= targetNs) timeNs - targetNs else targetNs - timeNs
        if (distance > MAX_DRIFT_NS) return null

        // Flooring 22.397xxx ms sends the next previous-sync lookup to an older
        // cue. Ceiling is a transport token only; the packet retains its true PTS.
        val requestMs = timeNs / NS_PER_MS + if (timeNs % NS_PER_MS == 0L) 0 else 1
        if (requestMs > Long.MAX_VALUE / NS_PER_MS || previous(requestMs * NS_PER_MS) != index) return null
        return ThumbnailSelectedCue(index, timeNs, requestMs)
    }

    companion object {
        private const val NS_PER_MS = 1_000_000L
        private const val MAX_DRIFT_NS = 5_000_000_000L
    }
}
