package space.nuvio.nativelegacy

internal data class ThumbnailSeparatedHints(val payloads: LongArray, val headers: LongArray)

/** Explicit arrays prevent a short payload list from promoting header-only work into downloads. */
internal fun thumbnailSeparatedHints(payloads: LongArray, headers: LongArray) =
    ThumbnailSeparatedHints(payloads.copyOf(minOf(16, payloads.size)), headers.copyOf(minOf(16, headers.size)))

internal fun thumbnailBridgeHints(future: LongArray, denseCandidates: LongArray,
                                 cueAt: (Long) -> Int, known: (Int) -> Boolean): ThumbnailSeparatedHints {
    // Retain the existing admitted-stage lookahead before filling spare header slots.
    val headers = thumbnailUnknownHeaderHints(future.drop(16).take(16).toLongArray() + denseCandidates,
        cueAt, known)
    return thumbnailSeparatedHints(future, headers)
}

/** At most 64 candidate timestamps are inspected; only 16 unknown distinct cues become probes. */
internal fun thumbnailUnknownHeaderHints(candidates: LongArray, cueAt: (Long) -> Int,
                                        known: (Int) -> Boolean): LongArray {
    val cues = HashSet<Int>()
    val result = ArrayList<Long>(16)
    for (time in candidates.take(64)) {
        if (time < 0) continue
        val cue = cueAt(time)
        if (cue < 0 || !cues.add(cue) || known(cue)) continue
        result.add(time)
        if (result.size == 16) break
    }
    return result.toLongArray()
}
