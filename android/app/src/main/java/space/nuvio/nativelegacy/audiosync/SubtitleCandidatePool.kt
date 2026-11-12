package space.nuvio.nativelegacy.audiosync

import java.util.Locale

/**
 * The other subtitles in the chosen subtitle's language, tested against the same audio evidence so
 * a file that fits the audio can replace one that never will (another cut, another frame rate, a
 * badly timed rip). Ported from NuvioTV-Reshaped without its speech-recognition step (owner, 06/11:
 * no 74 MB model on the device).
 *
 * Cheapest checks first:
 * 1. on arrival, copies of the chosen file or of each other (same timing) are dropped, and so are
 *    files whose length cannot fit the video at any common frame rate;
 * 2. every [update] scores a few remaining files in turn against one shared transform of the
 *    detected speech, drops those that clearly do not follow the dialogue and keeps the best few.
 * A file wins when it meets the same confirmation rules as a normal lock. The caller only asks while
 * the chosen subtitle's own tracker has not locked.
 *
 * [addCandidate] may be called from any thread; [update] from one thread.
 */
internal class SubtitleCandidatePool(
    private val target: SubtitleSpeechTrack,
    private val log: (String) -> Unit = {},
) {
    class Winner(val key: String, val model: SubtitleSyncModel, val method: String)

    private class Candidate(val key: String, val track: SubtitleSpeechTrack) {
        /** Frame-rate ratios that fit the video's length; null until the length is known. */
        var scales: DoubleArray? = null
        var misses = 0
        var score = 0.0

        /** Scored at least once with enough audio to be ruled out. */
        var ranked = false
    }

    private val lock = Any()
    private val active = ArrayList<Candidate>()
    private val fingerprints = hashSetOf(fingerprint(target))
    private var arrived = 0
    private var ruledOut = 0

    /** A file won; nothing left to do. */
    @Volatile
    var finished = false
        private set

    @Volatile
    var mediaDurationMs = 0L

    private var lastUpdateAtMs: Long? = null
    private var lastVersion = -1L
    private var cursor = 0

    /** Short progress line, e.g. "testing 4 · 7 ruled out". */
    val summary: String
        get() = synchronized(lock) {
            when {
                arrived == 0 -> ""
                active.isEmpty() -> "none of $arrived fit the audio"
                else -> "testing ${active.size}" + if (ruledOut > 0) " · $ruledOut ruled out" else ""
            }
        }

    /** Adds an alternative subtitle file; false when it was a copy or obviously unusable. */
    fun addCandidate(key: String, cues: List<Triple<Long, Long, String>>): Boolean {
        val track = SubtitleSpeechTrack.fromCues(cues)
        synchronized(lock) {
            if (finished) return false
            arrived++
            val accepted = track.size >= MIN_CUES && fingerprints.add(fingerprint(track))
            if (!accepted) {
                ruledOut++
                return false
            }
            active += Candidate(key, track)
        }
        return true
    }

    /** Scores the candidates on the evidence so far. Returns one confirmed to fit the audio, or null. */
    fun update(timeline: SpeechTimeline, nowMs: Long): Winner? {
        if (finished) return null
        val version = timeline.version
        if (version == lastVersion) return null
        lastUpdateAtMs?.let { if (nowMs - it < UPDATE_INTERVAL_MS) return null }
        lastUpdateAtMs = nowMs
        lastVersion = version
        val candidates = synchronized(lock) {
            applyShapeCheck()
            active.toList()
        }
        if (candidates.isEmpty()) return null
        return fromSpeech(timeline, candidates)
    }

    /** Drops files that cannot span the video at any common frame rate, once its length is known. */
    private fun applyShapeCheck() {
        val durationMs = mediaDurationMs
        if (durationMs <= 0) return
        val iterator = active.iterator()
        while (iterator.hasNext()) {
            val candidate = iterator.next()
            if (candidate.scales != null) continue
            val lastEndMs = candidate.track.endsMs.max()
            val scales = SubtitleAudioAligner.CANDIDATE_SCALES.filter { scale ->
                val mappedEndMs = lastEndMs * scale
                mappedEndMs <= durationMs + AudioSyncTracker.MAX_SHIFT_MS &&
                    mappedEndMs >= durationMs * MIN_LENGTH_COVERAGE
            }.toDoubleArray()
            if (scales.isEmpty()) {
                iterator.remove()
                ruledOut++
                log("candidate ${candidate.key} ruled out: ends at ${lastEndMs / 1_000}s, video ${durationMs / 1_000}s")
            } else {
                candidate.scales = scales
            }
        }
    }

    /**
     * Speech detection: a few candidates per call (in turn) against one transform of the latest
     * detected speech. The chosen subtitle is left to its own tracker, which searches more audio.
     */
    private fun fromSpeech(timeline: SpeechTimeline, candidates: List<Candidate>): Winner? {
        val segments = timeline.segments(maxFrames = WINDOW_FRAMES)
        val knownFrames = segments.sumOf { it.knownFrames }
        if (knownFrames < MIN_SCORING_FRAMES) return null
        val speech = SubtitleAudioAligner.prepare(
            segments = segments,
            minShiftMs = -AudioSyncTracker.MAX_SHIFT_MS,
            maxShiftMs = AudioSyncTracker.MAX_SHIFT_MS,
        ) ?: return null

        val pruning = knownFrames >= PRUNE_AFTER_FRAMES
        val dropped = ArrayList<Candidate>()
        var best: Pair<Candidate, SubtitleAudioAligner.Estimate>? = null
        val batch = List(minOf(BATCH_SIZE, candidates.size)) { candidates[(cursor + it) % candidates.size] }
        cursor = (cursor + batch.size) % candidates.size
        for (candidate in batch) {
            val scales = candidate.scales ?: SubtitleAudioAligner.CANDIDATE_SCALES
            val estimate = speech.estimate(candidate.track, scales)
            candidate.score = estimate?.prominence ?: 0.0
            candidate.ranked = candidate.ranked || pruning
            if (estimate != null && AudioSyncTracker.isLockable(estimate, segments, knownFrames, candidate.track)) {
                if (best == null || estimate.prominence > best.second.prominence) best = candidate to estimate
                continue
            }
            val weak = estimate == null || estimate.atSearchEdge || estimate.prominence < PRUNE_PROMINENCE
            candidate.misses = if (weak && pruning) candidate.misses + 1 else 0
            if (candidate.misses >= PRUNE_AFTER_MISSES) dropped += candidate
        }
        best?.let { (candidate, estimate) ->
            log("candidate ${candidate.key} locked by speech detection: $estimate")
            return win(candidate, estimate.scale, estimate.shiftMs, "speech detection")
        }
        synchronized(lock) {
            active.removeAll(dropped.toSet())
            ruledOut += dropped.size
            // Keep the best few once every candidate has been scored on enough audio.
            if (active.size > MAX_ACTIVE && active.all { it.ranked }) {
                val kept = active.sortedByDescending { it.score }.take(MAX_ACTIVE).toSet()
                ruledOut += active.size - kept.size
                active.retainAll(kept)
            }
        }
        if (dropped.isNotEmpty()) log("ruled out ${dropped.size} candidates on speech timing")
        return null
    }

    private fun win(candidate: Candidate, scale: Double, shiftMs: Double, method: String): Winner {
        finished = true
        return Winner(candidate.key, SubtitleSyncModel(listOf(SubtitleSyncSegment(0L, scale, shiftMs))), method)
    }

    companion object {
        private const val MIN_CUES = 20
        private const val UPDATE_INTERVAL_MS = 8_000L
        private val FRAMES_PER_SECOND = 1_000.0 / SpeechTimeline.FRAME_DURATION_MS
        private val MIN_SCORING_FRAMES = (60 * FRAMES_PER_SECOND).toInt()

        /** Known audio candidates are scored on; enough to confirm a lock, half the tracker's cost. */
        private val WINDOW_FRAMES = (10 * 60 * FRAMES_PER_SECOND).toInt()

        /** Candidates scored per call, so the cost per call stays flat however many there are. */
        private const val BATCH_SIZE = 4

        /** Candidates are only ruled out on speech timing after this much known audio. */
        private val PRUNE_AFTER_FRAMES = (180 * FRAMES_PER_SECOND).toInt()
        private const val PRUNE_PROMINENCE = 0.04
        private const val PRUNE_AFTER_MISSES = 2
        private const val MAX_ACTIVE = 4

        /** A file for this video covers at least this share of it (end credits have no dialogue). */
        private const val MIN_LENGTH_COVERAGE = 0.5

        /** Identical timing, whatever the text or encoding, means the same file. */
        private fun fingerprint(track: SubtitleSpeechTrack): Long {
            var hash = track.size.toLong()
            for (i in 0 until track.size) hash = hash * 31 + track.startsMs[i] / 100
            return hash
        }

        private val bibliographic = mapOf(
            "alb" to "sqi", "arm" to "hye", "baq" to "eus", "bur" to "mya", "chi" to "zho", "cze" to "ces",
            "dut" to "nld", "fre" to "fra", "geo" to "kat", "ger" to "deu", "gre" to "ell", "ice" to "isl",
            "mac" to "mkd", "mao" to "mri", "may" to "msa", "per" to "fas", "rum" to "ron", "mol" to "ron",
            "slo" to "slk", "tib" to "bod", "wel" to "cym", "scc" to "srp", "scr" to "hrv", "pob" to "por",
        )

        private val languageNames: Map<String, String> by lazy {
            Locale.getISOLanguages().associateBy(
                { Locale(it).getDisplayLanguage(Locale.ENGLISH).lowercase() },
                { runCatching { Locale(it).isO3Language }.getOrDefault(it) },
            )
        }

        /** ISO 639-2/T code for a language code or English name ("ro", "rum", "Romanian" -> "ron"). */
        fun languageKey(language: String?): String? {
            val value = language?.trim()?.lowercase()?.takeIf { it.isNotEmpty() } ?: return null
            val code = value.split('-', '_', ' ').first()
            bibliographic[code]?.let { return it }
            if (code.length == 2 || code.length == 3) {
                val iso3 = runCatching { Locale(code).isO3Language }.getOrNull()
                if (!iso3.isNullOrEmpty()) return bibliographic[iso3] ?: iso3
            }
            val name = value.substringBefore('(').trim()
            return languageNames[name]
                ?: languageNames.entries.firstOrNull { it.key.length >= 4 && name.startsWith(it.key) }?.value
                ?: value
        }
    }
}
