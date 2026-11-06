package space.nuvio.nativelegacy

/** Playback admission shared by native batches and the adaptive HTTP pacing policy. */
internal object ThumbnailHeadroom {
    const val UNAVAILABLE = -1
    const val BLOCKED = 0
    const val READY = 1

    data class Snapshot(
        val sampledAtMs: Long,
        val sourceEpoch: Long,
        val state: Int,
        val playing: Boolean,
        val playWhenReady: Boolean,
        val bufferMs: Long,
        val allocatedBytes: Long,
        val targetBytes: Long,
        val stable: Boolean,
    )

    fun fresh(ageMs: Long): Boolean = ageMs in 0..2000

    /** Preserve the pacing policy's stricter entry and more tolerant FAST continuation. */
    fun byteHealthy(bufferMs: Long, allocatedBytes: Long, targetBytes: Long,
                    stable: Boolean, continuingFast: Boolean = false): Boolean {
        val percent = if (continuingFast) 80 else 90
        return targetBytes > 0 && allocatedBytes >= targetBytes * percent / 100 &&
            stable && bufferMs >= if (continuingFast) 2500 else 3000
    }

    fun evaluate(nowMs: Long, sample: Snapshot?, desiredBufferMs: Long, latestIncidentAtMs: Long = -1,
                 currentSourceEpoch: Long = sample?.sourceEpoch ?: -1): Int {
        if (sample == null) return UNAVAILABLE
        if (sample.sourceEpoch != currentSourceEpoch || !fresh(nowMs - sample.sampledAtMs) ||
            sample.state != 3 || sample.bufferMs < 2000)
            return BLOCKED
        // Reject the old READY observation immediately when buffering/error/audio callbacks arrive.
        // A subsequent healthy sample can proceed; native and HTTP pacing own their recovery delays.
        if (latestIncidentAtMs >= 0 && latestIncidentAtMs >= sample.sampledAtMs) return BLOCKED
        // An explicit READY pause consumes no buffer; a transient stop during a seek does.
        if (!sample.playing) return if (!sample.playWhenReady) READY else BLOCKED
        return if (sample.bufferMs >= desiredBufferMs.coerceAtLeast(2000) ||
            byteHealthy(sample.bufferMs, sample.allocatedBytes, sample.targetBytes, sample.stable)) READY
        else BLOCKED
    }
}
