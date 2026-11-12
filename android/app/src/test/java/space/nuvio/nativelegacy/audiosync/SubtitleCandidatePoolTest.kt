package space.nuvio.nativelegacy.audiosync

import kotlin.math.abs
import kotlin.math.roundToInt
import kotlin.random.Random
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertNull
import kotlin.test.assertTrue

// Ported from NuvioTV-Reshaped; the speech-recognition cases are gone with recognition (06/11).
class SubtitleCandidatePoolTest {
    private val durationMs = 20 * 60_000L

    @Test
    fun findsTheFileThatFitsWhenTheChosenOneNeverWill() {
        val spoken = cues(Random(500), durationMs)
        val speech = speechFor(spoken, shiftMs = 0.0, random = Random(501))
        val chosen = cues(Random(502), durationMs)
        val pool = SubtitleCandidatePool(SubtitleSpeechTrack.fromCues(chosen)).apply { mediaDurationMs = durationMs }
        repeat(4) { pool.addCandidate("decoy$it", cues(Random(510 + it), durationMs)) }
        assertTrue(!pool.addCandidate("copy", chosen), "a copy of the chosen file is not tested again")
        pool.addCandidate("short", cues(Random(520), 8 * 60_000L))
        pool.addCandidate("good", spoken.map { (a, b, t) -> Triple(a - 3_000L, b - 3_000L, t) })

        val (winner, atMs) = assertNotNull(replay(pool, speech), "no candidate found")
        assertEquals("good", winner.key)
        val delayMs = winner.model.delayUsAt(600_000_000L) / 1_000.0
        assertTrue(abs(delayMs - 3_000.0) < 80.0, "delay $delayMs")
        println("found ${winner.key} after ${atMs / 1_000}s of playback (${pool.summary})")
        assertTrue(atMs <= 5 * 60_000L, "took until ${atMs / 1_000}s")
        assertTrue("ruled out" in pool.summary, pool.summary)
    }

    @Test
    fun staysQuietWhenTheChosenFileFits() {
        val spoken = cues(Random(600), durationMs)
        val speech = speechFor(spoken, shiftMs = 1_500.0, random = Random(601))
        val pool = SubtitleCandidatePool(SubtitleSpeechTrack.fromCues(spoken)).apply { mediaDurationMs = durationMs }
        repeat(3) { pool.addCandidate("decoy$it", cues(Random(610 + it), durationMs)) }
        pool.addCandidate("alsoGood", spoken.map { (a, b, t) -> Triple(a + 700L, b + 700L, t) })
        val tracker = AudioSyncTracker(SubtitleSpeechTrack.fromCues(spoken))
        assertNull(replay(pool, speech, tracker))
        assertNotNull(tracker.model, "the chosen subtitle locks on its own")
    }

    @Test
    fun neverPicksAnUnrelatedFile() {
        repeat(3) { seed ->
            val speech = speechFor(cues(Random(700 + seed), durationMs), shiftMs = 0.0, random = Random(710 + seed))
            val pool = SubtitleCandidatePool(SubtitleSpeechTrack.fromCues(cues(Random(720 + seed), durationMs)))
                .apply { mediaDurationMs = durationMs }
            repeat(8) { pool.addCandidate("decoy$it", cues(Random(730 + seed * 10 + it), durationMs)) }
            assertNull(replay(pool, speech), "false switch with seed $seed")
        }
    }

    @Test
    fun scoringTwelveFilesStaysCheap() {
        val spoken = cues(Random(900), durationMs)
        val speech = speechFor(spoken, shiftMs = 0.0, random = Random(901))
        fun pool() = SubtitleCandidatePool(SubtitleSpeechTrack.fromCues(cues(Random(902), durationMs))).apply {
            repeat(12) { addCandidate("decoy$it", cues(Random(910 + it), durationMs)) }
        }
        pool().update(speech, nowMs = 0L) // JIT warm-up
        val measured = pool()
        val started = System.nanoTime()
        measured.update(speech, nowMs = 0L)
        val elapsedMs = (System.nanoTime() - started) / 1_000_000
        println("scoring 12 candidates on 20 min of audio took ${elapsedMs}ms (${measured.summary})")
        assertTrue(elapsedMs < 5_000, "took ${elapsedMs}ms")
    }

    /**
     * Plays [full] back with a 60 s look-ahead, updating every 10 s. Like the controller, the pool is
     * only asked while the chosen subtitle's [tracker] (when given) has not locked.
     */
    private fun replay(
        pool: SubtitleCandidatePool,
        full: SpeechTimeline,
        tracker: AudioSyncTracker? = null,
    ): Pair<SubtitleCandidatePool.Winner, Long>? {
        val range = full.knownRange()!!
        val all = full.snapshot(0, range.last + 1)
        val live = SpeechTimeline()
        var copied = 0
        val endMs = ((range.last + 1) * SpeechTimeline.FRAME_DURATION_MS).toLong()
        var positionMs = 0L
        while (positionMs <= endMs) {
            val horizon = minOf(all.size, ((positionMs + 60_000L) / SpeechTimeline.FRAME_DURATION_MS).toInt())
            while (copied < horizon) {
                if (!all[copied].isNaN()) live.record(copied, all[copied])
                copied++
            }
            tracker?.update(live, positionMs)
            if (tracker?.model != null) return null
            pool.update(live, nowMs = positionMs)?.let { return it to positionMs }
            positionMs += 10_000L
        }
        return null
    }

    /** Dialogue-like cues: 0.8-4.5 s lines separated by 0.2-9 s gaps. */
    private fun cues(random: Random, durationMs: Long): List<Triple<Long, Long, String>> {
        val cues = mutableListOf<Triple<Long, Long, String>>()
        var t = 5_000L + random.nextLong(20_000L)
        while (t < durationMs - 60_000L) {
            val length = 800L + random.nextLong(3_700L)
            cues += Triple(t, t + length, "line ${cues.size}")
            t += length + 200L + random.nextLong(if (random.nextInt(5) == 0) 9_000L else 2_500L)
        }
        return cues
    }

    /** Noisy detector output for speech where [cues] land when shifted by [shiftMs]. */
    private fun speechFor(cues: List<Triple<Long, Long, String>>, shiftMs: Double, random: Random): SpeechTimeline {
        val frameMs = SpeechTimeline.FRAME_DURATION_MS
        val frames = ((cues.maxOf { it.second } + shiftMs + 30_000) / frameMs).toInt()
        val speaking = BooleanArray(frames)
        val latencyMs = SubtitleAudioAligner.DETECTOR_BIAS_MS
        for ((start, stop, _) in cues) {
            val from = ((start + shiftMs + latencyMs) / frameMs).roundToInt().coerceIn(0, frames)
            val to = ((stop + shiftMs + latencyMs) / frameMs).roundToInt().coerceIn(0, frames)
            for (f in from until to) speaking[f] = true
        }
        val timeline = SpeechTimeline()
        for (f in 0 until frames) {
            val p = if (speaking[f]) {
                if (random.nextFloat() < 0.25f) random.nextFloat() * 0.3f else 0.7f + random.nextFloat() * 0.3f
            } else {
                if (random.nextFloat() < 0.08f) 0.4f + random.nextFloat() * 0.5f else random.nextFloat() * 0.1f
            }
            timeline.record(f, p)
        }
        return timeline
    }

}
