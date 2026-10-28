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
