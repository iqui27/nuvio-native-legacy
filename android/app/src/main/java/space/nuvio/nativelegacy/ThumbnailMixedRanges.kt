package space.nuvio.nativelegacy

/** Probes retain priority; add only complete known payloads, within existing batch bounds. */
internal fun thumbnailMixedRanges(probes: List<LongArray>, payloads: List<LongArray>): List<LongArray> {
    val result = probes.toMutableList()
    var bytes = probes.sumOf { it[1] - it[0] + 1 }
    for (r in payloads) {
        val size = r[1] - r[0] + 1
        if (size <= 0 || result.size >= 32 || size > (8L shl 20) - bytes) continue
        if (result.any { r[0] >= it[0] && r[1] <= it[1] }) continue
        result.add(r); bytes += size
    }
    return result.sortedBy { it[0] }
}

internal data class ThumbnailCurrentRangesPlan(
    val ranges: List<LongArray>,
    val optionalProbeIndices: List<Int>,
    val currentPayloadCount: Int,
)

/** Current work owns the existing bounds before speculative header lookahead. */
internal fun thumbnailCurrentRanges(
    requiredProbes: List<LongArray>,
    currentPayloads: List<LongArray>,
    optionalProbes: List<LongArray>,
    byteLimit: Long = 8L shl 20,
): ThumbnailCurrentRangesPlan? {
    val limit = byteLimit.coerceIn(0, 8L shl 20)
    val ranges = ArrayList<LongArray>()
    var bytes = 0L
    fun size(range: LongArray): Long? {
        if (range.size < 2 || range[0] < 0 || range[1] < range[0]) return null
        val difference = range[1] - range[0]
        return if (difference == Long.MAX_VALUE) null else difference + 1
    }
    fun add(range: LongArray): Boolean {
        val count = size(range) ?: return false
        if (ranges.any { range[0] >= it[0] && range[1] <= it[1] }) return true
        if (ranges.size >= 32 || count > limit - bytes) return false
        ranges.add(range)
        bytes += count
        return true
    }
    // Never return a partial mandatory plan. Caller retains its required-only
    // parsing/fallback path if malformed or out-of-bound mandatory input arrives.
    for (probe in requiredProbes) if (!add(probe)) return null
    var payloads = 0
    for (payload in currentPayloads) if (add(payload)) payloads++
    val optional = ArrayList<Int>()
    // An oversized pending payload must not be held behind a future-only request.
    // With no current work at all, there is likewise no reason to dispatch probes.
    if (payloads > 0 || (currentPayloads.isEmpty() && requiredProbes.isNotEmpty())) {
        for ((index, probe) in optionalProbes.withIndex()) if (add(probe)) optional.add(index)
    }
    return ThumbnailCurrentRangesPlan(ranges.sortedBy { it[0] }, optional, payloads)
}
