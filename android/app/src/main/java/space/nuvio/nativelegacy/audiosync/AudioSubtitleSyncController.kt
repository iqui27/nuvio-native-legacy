@file:OptIn(androidx.media3.common.util.UnstableApi::class)

package space.nuvio.nativelegacy.audiosync

import android.content.Context
import android.net.ConnectivityManager
import android.net.Uri
import android.os.Process
import android.os.SystemClock
import androidx.media3.common.C
import androidx.media3.common.Format
import androidx.media3.common.Tracks
import androidx.media3.datasource.DataSource
import androidx.media3.exoplayer.audio.AudioSink
import androidx.media3.extractor.ExtractorsFactory
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import kotlin.math.roundToInt

/**
 * Keeps an external subtitle in sync with the audio of the playing stream. Ported from
 * NuvioTV-Reshaped (branch audio-sync-fallback-stable, GPL-3.0 like Nuvio) without its speech
 * recognition (owner, 06/11: no 74 MB model on the device) and without its status panel.
 *
 * Compressed audio is copied as ExoPlayer demuxes it (see [AudioSyncExtractorsFactory]), decoded on
 * a background thread, turned into a speech timeline by Silero VAD, and aligned against the
 * subtitle cues. A few dialogue spots across the film are sampled over their own connections
 * ([AudioSpotSampler]) so evidence arrives in seconds. The other subtitles of the same language are
 * tested against the same audio ([SubtitleCandidatePool]) and one that fits can replace the chosen
 * one. The mapping found is exposed through [currentModel]; the C side applies it to the subtitle
 * it draws (src/legsync.c). One instance lives per player.
 */
internal class AudioSubtitleSyncController(
    context: Context,
    /** User-visible progress, called from background threads. */
    private val onStatus: (AudioSyncStatus) -> Unit = {},
    /** Asks for the subtitle [url] to be shown instead of the current one (it fits the audio). */
    private val requestSwitch: (url: String) -> Unit = {},
    /** Downloads and parses a subtitle into (startMs, endMs, text) cues; empty on failure. Blocking. */
    private val fetchCues: (url: String) -> List<Triple<Long, Long, String>> = { emptyList() },
) : AudioSampleSink, PlaybackPcmListener {
    private val appContext = context.applicationContext
    private val timeline = SpeechTimeline()
    private val aligner: ExecutorService = Executors.newSingleThreadExecutor { runnable ->
        Thread({
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
            runnable.run()
        }, "NuvioAudioSyncAlign").apply { isDaemon = true }
    }
    private val alignRunning = AtomicBoolean(false)
    private val sessionRequest = AtomicInteger(0)

    @Volatile
    private var decoder: AudioSyncDecoder? = null

    @Volatile
    private var decoderUnavailable = false

    private class Session(
        val key: String,
        val language: String,
        val track: SubtitleSpeechTrack,
    ) {
        val tracker = AudioSyncTracker(track)
    }

    /** Another subtitle of the chosen one's language, an alternative the audio may prefer. */
    data class ReferenceCandidate(
        val url: String,
        val language: String,
        /** Name shown to the user. */
        val label: String = "",
    )

    private val fetchPool: ExecutorService = Executors.newFixedThreadPool(3) { runnable ->
        Thread({
            Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
            runnable.run()
        }, "NuvioAudioSyncFetch").apply { isDaemon = true }
    }

    @Volatile
    private var candidates: List<ReferenceCandidate> = emptyList()

    /** Parsed dialogue of alternative subtitles by url; empty list = failed. */
    private val parsedReferences = ConcurrentHashMap<String, List<Triple<Long, Long, String>>>()
    private val pendingReferenceFetches = ConcurrentHashMap.newKeySet<String>()

    @Volatile
    private var lockMethod: String? = null

    @Volatile
    private var estimated = false

    @Volatile
    private var liveOnly = false

    @Volatile
    private var problem: String? = null
    private var lastPublishAtMs = 0L

    @Volatile
    private var sourceKey: String = ""
    private val preferences by lazy { appContext.getSharedPreferences(PREFERENCES, Context.MODE_PRIVATE) }

    @Volatile
    private var session: Session? = null

    /** Other subtitles in the chosen one's language, tested against the audio while it is unsynced. */
    @Volatile
    private var pool: SubtitleCandidatePool? = null
    private var poolSession: Session? = null
    private val poolRequested = ConcurrentHashMap.newKeySet<String>()

    /** Subtitles switched away from on this stream; picking one again is respected. */
    private val switchedAway = ConcurrentHashMap.newKeySet<String>()

    /** Mapping of the subtitle being switched to, adopted as soon as its session starts. */
    private class Handover(val key: String, val model: SubtitleSyncModel, val method: String)

    @Volatile
    private var handover: Handover? = null

    @Volatile
    private var mediaDurationMs = 0L

    /** The stream, as the player reads it, for sampling audio away from the playhead. */
    private class SpotSource(
        val sourceKey: String,
        val uri: Uri,
        val dataSourceFactory: DataSource.Factory,
        val extractorsFactory: ExtractorsFactory,
    )

    @Volatile
    private var spotSource: SpotSource? = null
    private val spotSamplingStarted = AtomicBoolean(false)

    @Volatile
    private var spotStatus = ""

    /** (source key, reason) when this stream is not sampled at all. */
    @Volatile
    private var spotUnavailable: Pair<String, String>? = null

    @Volatile
    private var model: SubtitleSyncModel? = null

    @Volatile
    private var lockedAtElapsedMs = 0L

    /** When the current session started listening, to show how long syncing took. */
    @Volatile
    private var sessionStartedAtMs = 0L

    /** Whether spots may be sampled on metered (mobile) networks too; the user's setting. */
    @Volatile
    var samplingOnMobileData: Boolean = false
        set(value) {
            field = value
            // Declined on mobile data earlier in this stream: try again now that it is allowed.
            if (value && spotDeclinedOnMobileData) {
                spotDeclinedOnMobileData = false
                spotStatus = ""
                spotSamplingStarted.set(false)
            }
        }

    @Volatile
    private var spotDeclinedOnMobileData = false

    @Volatile
    var enabled: Boolean = true
        set(value) {
            field = value
            if (!value) model = null
        }

    @Volatile
    private var playbackPositionMs = 0L

    @Volatile
    private var selectedAudioFormat: Format? = null

    @Volatile
    private var provisionalAudioFormat: Format? = null

    @Volatile
    private var released = false

    private var lastAlignedSession: Session? = null
    private var lastAlignVersion = -1L
    private var lastAlignAtMs = 0L

    /** Wraps [factory] so audio is tapped while it is demuxed. */
    fun wrap(factory: ExtractorsFactory): ExtractorsFactory = AudioSyncExtractorsFactory(factory, this)

    /** Wraps the player's audio output so its decoded audio can be used when demux-time capture can't. */
    fun wrapAudioSink(sink: AudioSink): AudioSink = PlaybackAudioTap(sink, this)

    /**
     * Whether audio is analysed before any subtitle session exists, so a session that starts later
     * finds audio already heard. Off: nothing is decoded until a session starts.
     */
    @Volatile
    var listensBeforeSession: Boolean = true

    /** The mapping currently applied to the session's subtitle (subtitle time -> media time), if any. */
    fun currentModel(): SubtitleSyncModel? = model.takeIf { enabled && session != null }

    /** Whether [currentModel] is still an early, unconfirmed estimate. */
    fun isEstimate(): Boolean = estimated

    /** Called on the main thread from the player's periodic snapshot; [durationMs] <= 0 when unknown. */
    fun onPlaybackPosition(positionMs: Long, durationMs: Long = 0L) {
        playbackPositionMs = positionMs.coerceAtLeast(0L)
        if (durationMs > 0 && durationMs != mediaDurationMs) {
            mediaDurationMs = durationMs
            pool?.mediaDurationMs = durationMs
        }
        scheduleAlignment()
        maybeSampleSpots()
        val now = SystemClock.elapsedRealtime()
        if (now - lastPublishAtMs >= DIAGNOSTICS_INTERVAL_MS) {
            lastPublishAtMs = now
            logState()
        }
    }

    private var lastLoggedState = ""

    /** Logs what Reshaped's status panel would show, whenever it changes. */
    private fun logState() {
        val current = session ?: return
        if (!enabled || released) return
        val synced = model
        val phase = when {
            synced != null && estimated -> "estimated"
            synced != null -> "synced"
            decoderUnavailable -> "unavailable"
            else -> "listening"
        }
        val mapping = synced?.segments?.joinToString("|") {
            "${it.fromMediaMs / 1_000}:${"%.5f".format(java.util.Locale.US, it.scale)}:${Math.round(it.shiftMs / 500.0)}"
        }.orEmpty()
        val state = "state: $phase mapping=${mapping.ifEmpty { "-" }} method=${lockMethod ?: "-"}" +
            " liveOnly=$liveOnly others=${pool?.summary.orEmpty()}" +
            " sampling=${spotStatus.ifEmpty { spotUnavailable?.takeIf { it.first == sourceKey }?.second.orEmpty() }}" +
            (problem?.let { " problem=$it" } ?: "") + " subtitle=${current.key.hashCode()}"
        if (state == lastLoggedState) return
        lastLoggedState = state
        SyncLog.i(state)
    }

    fun onSourceChanged(newSourceKey: String) {
        sourceKey = newSourceKey
        timeline.clear()
        decoder?.discontinuity()
        selectedAudioFormat = null
        provisionalAudioFormat = null
        stopPool()
        switchedAway.clear()
        handover = null
        spotSamplingStarted.set(false)
        spotDeclinedOnMobileData = false
        spotStatus = ""
        model = null
        lockedAtElapsedMs = 0L
        lastAlignVersion = -1L
        session?.let { current ->
            val restarted = Session(current.key, current.language, current.track)
            session = restarted
            startPool(restarted)
        }
    }

    /**
     * The stream the player is reading for the source [forSourceKey], so short stretches of audio
     * from across the film can be sampled ahead of playback (see [AudioSpotSampler]).
     */
    fun setSpotSource(
        forSourceKey: String,
        url: String,
        dataSourceFactory: DataSource.Factory,
        extractorsFactory: ExtractorsFactory,
        localEngine: Boolean,
    ) {
        val uri = runCatching { Uri.parse(url) }.getOrNull()
        val path = uri?.path.orEmpty().lowercase()
        val unavailable = when {
            localEngine -> "not used for torrent streams (would slow playback)"
            uri == null || (uri.scheme != "http" && uri.scheme != "https") -> "not available for this stream"
            path.endsWith(".m3u8") || path.endsWith(".mpd") -> "not available for playlist streams"
            else -> null
        }
        spotSource = if (unavailable == null) SpotSource(forSourceKey, uri!!, dataSourceFactory, extractorsFactory) else null
        spotUnavailable = unavailable?.let { forSourceKey to it }
    }

    /**
     * Once playback runs with some buffer, samples a few dialogue spots across the film while the
     * chosen subtitle is still unsynced. Once per stream; on metered networks only when allowed.
     */
    private fun maybeSampleSpots() {
        if (spotSamplingStarted.get()) return
        val source = spotSource ?: return
        val current = session ?: return
        if (!enabled || released || source.sourceKey != sourceKey || (model != null && !estimated)) return
        if (mediaDurationMs < MIN_SAMPLED_FILM_MS) return
        if (timeline.knownFrameCount() < SAMPLE_AFTER_FRAMES) return
        if (!spotSamplingStarted.compareAndSet(false, true)) return
        if (!samplingOnMobileData && !isUnmetered(appContext)) {
            SyncLog.i("not sampling audio across the film on a metered network")
            spotStatus = "off on mobile data"
            spotDeclinedOnMobileData = true
            return
        }
        Thread({ sampleSpots(source, current) }, "NuvioAudioSyncSpots").apply {
            isDaemon = true
            start()
        }
    }

    private fun sampleSpots(source: SpotSource, current: Session) {
        Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
        val weights = runCatching { loadWeights(appContext) }.getOrNull() ?: return
        createDecoder()
        // One decoder per worker: each reads a different place, so their audio must not mix.
        val decoders = List(SPOT_WORKERS) {
            val analyzer = SpeechAnalyzer(SileroVad(weights), timeline)
            AudioSyncDecoder(analyzer, analyzer, allowVendorDecoders = false)
        }
        try {
            // Every subtitle at hand shows roughly where people talk, even with its timing off.
            val tracks = listOf(current.track) + candidates
                .mapNotNull { parsedReferences[it.url]?.takeIf { cues -> cues.isNotEmpty() } }
                .map(SubtitleSpeechTrack::fromCues)
            // A fresh start covers the opening itself; after a resume any part of the film helps.
            val coveredMs = timeline.segments(fromFrame = SpeechTimeline.frameForTimeUs(playbackPositionMs * 1_000L))
                .firstOrNull()?.let { (it.toFrame * SpeechTimeline.FRAME_DURATION_MS).toLong() } ?: 0L
            val notBeforeMs = if (playbackPositionMs < RESUME_THRESHOLD_MS) coveredMs + SPOT_MS else 0L
            val spots = DialogueSpotPlanner.plan(tracks, mediaDurationMs, SPOT_COUNT, SPOT_MS, notBeforeMs)
            if (spots.isEmpty()) return
            SyncLog.i("sampling audio at ${spots.map { it / 1_000 }}s")
            spotStatus = "sampling ${spots.size} dialogue spots…"
            val startedAt = SystemClock.elapsedRealtime()
            // On mobile data the target is also the cap; on other networks a high-bitrate file may
            // use more so each spot still holds whole sentences.
            val maxBytes = if (isUnmetered(appContext)) MAX_SPOT_BYTES else TARGET_SPOT_BYTES
            val sampler = AudioSpotSampler(source.uri, source.dataSourceFactory, TARGET_SPOT_BYTES, maxBytes, MIN_SPOT_AUDIO_MS)
            val result = sampler.run(
                spotsMs = spots,
                spotMs = SPOT_MS,
                workers = decoders.map { AudioSyncExtractorsFactory(source.extractorsFactory, SpotSink(it)) },
                isCancelled = {
                    released || !enabled || sourceKey != source.sourceKey || session == null ||
                        (model != null && !estimated)
                },
            ) { sampled, bytes ->
                val seconds = (SystemClock.elapsedRealtime() - startedAt) / 1_000
                spotStatus = "sampled $sampled of ${spots.size} dialogue spots in ${seconds}s (${bytes / 1_000_000} MB)"
            }
            SyncLog.i("sampled ${result.sampled} spots, ${result.bytes / 1_000_000} MB, failure=${result.failure}")
            if (result.failure != null && result.sampled == 0) spotStatus = "not possible for this stream"
            decoders.forEach { it.awaitDrained(DRAIN_TIMEOUT_MS) }
        } catch (error: Throwable) {
            SyncLog.w("audio sampling failed: ${error.message}")
        } finally {
            decoders.forEach(AudioSyncDecoder::release)
        }
    }

    /** Feeds sampled audio of the playing track to [decoder], waiting when it is busy. */
    private inner class SpotSink(private val decoder: AudioSyncDecoder) : AudioSampleSink {
        override fun wantsSamples(format: Format): Boolean {
            if (!enabled || released) return false
            val playing = selectedAudioFormat ?: provisionalAudioFormat ?: return false
            return matches(format, playing)
        }

        override fun onSample(format: Format, timeUs: Long, data: ByteArray, offset: Int, size: Int) {
            while (!decoder.offer(format, timeUs, data, offset, size)) {
                if (!decoder.accepts(format)) return
                Thread.sleep(SPOT_BACKOFF_MS)
            }
        }

        override fun onDiscontinuity() {
            decoder.discontinuity()
        }
    }

    fun onAudioTrackSelected(format: Format?) {
        if (format == null || selectedAudioFormat?.let { matches(it, format) } == true) return
        selectedAudioFormat = format
        SyncLog.d("audio track selected: ${describe(format)}")
    }

    /**
     * Starts syncing the subtitle identified by [key] ([language] as the add-on gave it) with its
     * dialogue cues (startMs, endMs, text). The dialogue track is built and the model loaded on the
     * background thread; the session becomes active afterwards unless another subtitle was chosen
     * meanwhile.
     */
    fun startSession(key: String, language: String, dialogue: List<Triple<Long, Long, String>>) {
        val request = sessionRequest.incrementAndGet()
        session = null
        stopPool()
        val handedOver = handover?.takeIf { it.key == key }
        handover = null
        model = null
        lockedAtElapsedMs = 0L
        lockMethod = null
        estimated = false
        problem = null
        try {
            aligner.execute {
                val track = SubtitleSpeechTrack.fromCues(dialogue)
                if (sessionRequest.get() != request || released) return@execute
                if (track.size < MIN_TRACK_CUES) {
                    SyncLog.i("subtitle has only ${track.size} dialogue cues; audio sync skipped")
                    problem = "too few dialogue lines (${track.size})"
                    notify(AudioSyncStatus.TooFewLines(track.size))
                    return@execute
                }
                createDecoder()
                if (sessionRequest.get() != request) return@execute
                val started = Session(key, language, track)
                sessionStartedAtMs = SystemClock.elapsedRealtime()
                session = started
                SyncLog.i("sync session started with ${track.size} dialogue cues")
                if (!enabled) return@execute
                when {
                    handedOver != null -> adoptHandover(started, handedOver)
                    applyRemembered(started) -> Unit
                    else -> {
                        notify(AudioSyncStatus.Listening)
                        startPool(started)
                    }
                }
            }
        } catch (_: Exception) {
            // Executor already shut down: the player is being released.
        }
    }

    fun stopSession() {
        sessionRequest.incrementAndGet()
        stopPool()
        lockedAtElapsedMs = 0L
        if (session != null) SyncLog.i("sync session stopped")
        session = null
        model = null
    }

    fun activeSessionKey(): String? = session?.key

    fun release() {
        released = true
        session = null
        stopPool()
        model = null
        decoder?.release()
        aligner.shutdownNow()
        fetchPool.shutdownNow()
    }

    fun setReferenceSubtitles(list: List<ReferenceCandidate>) {
        candidates = list
        // Subtitles listed after the pick join the running search.
        session?.let { current -> if (model == null || estimated) startPool(current) }
    }

    /**
     * Starts (or extends) testing the other subtitles in the chosen one's language against the
     * audio, so a file that fits can replace one that never will.
     */
    private fun startPool(current: Session) {
        if (!enabled || released || session !== current || current.key in switchedAway) return
        val language = SubtitleCandidatePool.languageKey(
            candidates.firstOrNull { it.url == current.key }?.language ?: current.language,
        ) ?: return
        val alternatives = candidates
            .filter { it.url != current.key && SubtitleCandidatePool.languageKey(it.language) == language }
            .distinctBy { it.url }
            .take(MAX_ALTERNATIVES)
        if (alternatives.isEmpty()) return
        val active = synchronized(poolRequested) {
            if (poolSession !== current) {
                poolRequested.clear()
                poolSession = current
                pool = SubtitleCandidatePool(current.track) { SyncLog.i(it) }.apply { mediaDurationMs = this@AudioSubtitleSyncController.mediaDurationMs }
                SyncLog.i("testing ${alternatives.size} other $language subtitles against the audio")
            }
            pool
        } ?: return
        for (alternative in alternatives) {
            if (!poolRequested.add(alternative.url)) continue
            fetchReference(alternative) { dialogue ->
                if (pool === active) active.addCandidate(alternative.url, dialogue)
            }
        }
    }

    private fun stopPool() {
        synchronized(poolRequested) {
            pool = null
            poolSession = null
            poolRequested.clear()
        }
    }

    /** Scores the alternatives while the chosen subtitle is unconfirmed; switches to one that fits. */
    private fun updatePool(current: Session) {
        val alternatives = pool ?: return
        if (session !== current || (model != null && !estimated)) return
        val winner = alternatives.update(timeline, SystemClock.elapsedRealtime()) ?: return
        if (session !== current || !enabled || released) return
        val label = candidates.firstOrNull { it.url == winner.key }?.label?.takeIf { it.isNotBlank() } ?: "another file"
        SyncLog.i("SWITCHING subtitle to $label via ${winner.method}: ${winner.model}")
        switchedAway += current.key
        stopPool()
        remember(winner.key, winner.model)
        handover = Handover(winner.key, winner.model, winner.method)
        requestSwitch(winner.key)
    }

    /** Applies the mapping found for the subtitle that was switched to. */
    private fun adoptHandover(current: Session, handedOver: Handover) {
        current.tracker.adopt(handedOver.model)
        model = handedOver.model
        estimated = false
        lockMethod = handedOver.method
        lockedAtElapsedMs = SystemClock.elapsedRealtime()
        SyncLog.i("adopted mapping of switched-to subtitle ${handedOver.model}")
        notify(
            AudioSyncStatus.Synced(
                offsetMs = handedOver.model.delayUsAt(playbackPositionMs * 1_000L) / 1_000L,
                rateCorrected = handedOver.model.segments.first().scale != 1.0,
            ),
        )
    }

    /** Subtitle services throttle bursts (HTTP 429): the C download path retries; the pool tries once. */
    private fun fetchReference(candidate: ReferenceCandidate, onReady: ((List<Triple<Long, Long, String>>) -> Unit)?) {
        parsedReferences[candidate.url]?.let { cached ->
            if (cached.isNotEmpty()) onReady?.invoke(cached)
            return
        }
        if (!pendingReferenceFetches.add(candidate.url) && onReady == null) return
        try {
            fetchPool.execute {
                val dialogue = parsedReferences[candidate.url] ?: runCatching { fetchCues(candidate.url) }.getOrElse {
                    SyncLog.w("reference download failed for ${candidate.url}: ${it.message}")
                    emptyList()
                }
                parsedReferences[candidate.url] = dialogue
                pendingReferenceFetches.remove(candidate.url)
                if (dialogue.isNotEmpty()) onReady?.invoke(dialogue)
            }
        } catch (_: Exception) {
            // Released.
        }
    }

    /** Versioned: syncs saved by earlier builds could hold a 0.1% stretch found on too little audio. */
    private fun rememberKey(subtitleKey: String): String = "v2:${sourceKey.hashCode()}:${subtitleKey.hashCode()}"

    /** Stored as "fromMs,scale,shiftMs" per segment, separated by "|". */
    private fun remember(subtitleKey: String, synced: SubtitleSyncModel) {
        val stored = synced.segments.joinToString("|") { "${it.fromMediaMs},${it.scale},${it.shiftMs}" }
        runCatching { preferences.edit().putString(rememberKey(subtitleKey), stored).apply() }
    }

    private fun parseRemembered(stored: String): SubtitleSyncModel? {
        val segments = stored.split('|').map { entry ->
            val parts = entry.split(',')
            SubtitleSyncSegment(
                fromMediaMs = parts.getOrNull(0)?.toLongOrNull() ?: return null,
                scale = parts.getOrNull(1)?.toDoubleOrNull() ?: return null,
                shiftMs = parts.getOrNull(2)?.toDoubleOrNull() ?: return null,
            )
        }
        return segments.takeIf { it.isNotEmpty() }?.let(::SubtitleSyncModel)
    }

    /** Applies the mapping found the last time this subtitle played on this stream. */
    private fun applyRemembered(current: Session): Boolean {
        val stored = runCatching { preferences.getString(rememberKey(current.key), null) }.getOrNull() ?: return false
        val restored = parseRemembered(stored) ?: return false
        val scale = restored.segments.first().scale
        current.tracker.adopt(restored)
        model = restored
        estimated = false
        lockMethod = "remembered from last time"
        SyncLog.i("restored remembered sync $restored")
        notify(AudioSyncStatus.Synced(offsetMs = restored.delayUsAt(playbackPositionMs * 1_000L) / 1_000L, rateCorrected = scale != 1.0))
        return true
    }

    /** Before any subtitle is picked, listen to the first minutes so a pick can sync at once. */
    private fun listening(): Boolean =
        session != null || (listensBeforeSession && timeline.knownFrameCount() < PRE_SESSION_FRAMES)

    override fun wantsSamples(format: Format): Boolean {
        if (!enabled || released || decoderUnavailable || !listening()) return false
        val selected = selectedAudioFormat
        if (selected != null) return matches(format, selected)
        val provisional = provisionalAudioFormat ?: format.also { provisionalAudioFormat = it }
        return matches(format, provisional)
    }

    override fun onSample(format: Format, timeUs: Long, data: ByteArray, offset: Int, size: Int) {
        if (!inDutyWindow(timeUs)) return
        val decoder = decoder ?: createDecoder() ?: return
        decoder.offer(format, timeUs, data, offset, size)
    }

    override fun onDiscontinuity() {
        decoder?.discontinuity()
    }

    /**
     * Playback audio is only analysed where the look-ahead capture left the timeline unknown: codecs
     * without a usable second decoder, HLS/DASH streams, or gaps. Otherwise it is skipped cheaply.
     */
    override fun wantsPlaybackPcm(mediaTimeUs: Long, durationUs: Long): Boolean {
        if (!enabled || released || decoder == null || !listening()) return false
        if (mediaTimeUs < 0 || !inDutyWindow(mediaTimeUs)) return false
        val from = SpeechTimeline.frameForTimeUs(mediaTimeUs)
        val to = SpeechTimeline.frameForTimeUs(mediaTimeUs + durationUs) + 1
        return timeline.knownFramesIn(from, to) < to - from - 1
    }

    override fun onPlaybackPcm(mono: FloatArray, frames: Int, sampleRate: Int, mediaTimeUs: Long) {
        decoder?.offerPlaybackPcm(mono, frames, sampleRate, mediaTimeUs)
    }

    /**
     * Once a lock has been refined over a long stretch, analyse one minute in three: enough to follow
     * later jumps while saving most of the CPU.
     */
    private fun inDutyWindow(timeUs: Long): Boolean {
        if (session?.tracker?.model == null || lockedAtElapsedMs == 0L) return true
        if (SystemClock.elapsedRealtime() - lockedAtElapsedMs < FULL_ANALYSIS_AFTER_LOCK_MS) return true
        return (timeUs / DUTY_WINDOW_US) % DUTY_CYCLE == 0L
    }

    @Synchronized
    private fun createDecoder(): AudioSyncDecoder? {
        decoder?.let { return it }
        if (decoderUnavailable || released) return null
        val weights = runCatching { loadWeights(appContext) }.onFailure {
            SyncLog.w("could not load VAD weights: ${it.message}")
        }.getOrNull()
        if (weights == null) {
            decoderUnavailable = true
            notify(AudioSyncStatus.Unavailable)
            return null
        }
        val analyzer = SpeechAnalyzer(SileroVad(weights), timeline)
        val liveAnalyzer = SpeechAnalyzer(SileroVad(weights), timeline)
        return AudioSyncDecoder(analyzer, liveAnalyzer) { mime ->
            SyncLog.i("look-ahead capture unavailable for $mime; syncing from playback audio instead")
            liveOnly = true
            if (session != null) notify(AudioSyncStatus.LiveOnly(mime))
        }.also { decoder = it }
    }

    private fun scheduleAlignment() {
        val current = session ?: return
        if (!enabled || released) return
        val now = SystemClock.elapsedRealtime()
        val version = timeline.version
        // Check often while searching so an early estimate lands quickly; relax once confirmed.
        val interval = if (current.tracker.model == null) SEARCH_INTERVAL_MS else ALIGN_INTERVAL_MS
        if (current === lastAlignedSession && (version == lastAlignVersion || now - lastAlignAtMs < interval)) {
            return
        }
        if (!alignRunning.compareAndSet(false, true)) return
        lastAlignedSession = current
        lastAlignVersion = version
        lastAlignAtMs = now
        val position = playbackPositionMs
        try {
            aligner.execute {
                try {
                    align(current, position)
                } catch (error: Throwable) {
                    SyncLog.w("alignment failed: ${error.message}")
                } finally {
                    alignRunning.set(false)
                }
            }
        } catch (_: Exception) {
            alignRunning.set(false)
        }
    }

    private fun align(current: Session, positionMs: Long) {
        val startedAt = SystemClock.elapsedRealtime()
        val outcome = current.tracker.update(timeline, positionMs)
        if (session !== current) return
        val elapsed = SystemClock.elapsedRealtime() - startedAt
        when (outcome) {
            is AudioSyncTracker.Outcome.Provisional -> {
                val first = model == null
                model = outcome.model
                estimated = true
                lockMethod = "speech detection"
                SyncLog.i("provisional ${outcome.model} ${describe(outcome.estimate)} in ${elapsed}ms")
                if (first) {
                    notify(
                        AudioSyncStatus.Estimated(
                            offsetMs = outcome.model.delayUsAt(positionMs * 1_000L) / 1_000L,
                        ),
                    )
                }
            }
            is AudioSyncTracker.Outcome.Retracted -> {
                model = null
                estimated = false
                lockMethod = null
                SyncLog.i("early estimate withdrawn ${describe(outcome.estimate)}")
                notify(AudioSyncStatus.Withdrawn)
            }
            is AudioSyncTracker.Outcome.Locked -> {
                lockedAtElapsedMs = SystemClock.elapsedRealtime()
                model = outcome.model
                estimated = false
                lockMethod = "speech detection"
                SyncLog.i("LOCKED ${outcome.model} ${describe(outcome.estimate)} in ${elapsed}ms" +
                    " (${(lockedAtElapsedMs - sessionStartedAtMs) / 1_000}s after the session started)")
                notify(
                    AudioSyncStatus.Synced(
                        offsetMs = outcome.model.delayUsAt(positionMs * 1_000L) / 1_000L,
                        rateCorrected = outcome.estimate.scale != 1.0,
                    ),
                )
            }
            is AudioSyncTracker.Outcome.Refined -> {
                model = outcome.model
                SyncLog.i("refined ${outcome.model} ${describe(outcome.estimate)}")
            }
            is AudioSyncTracker.Outcome.Jumped -> {
                model = outcome.model
                SyncLog.i("jump detected ${outcome.model} ${describe(outcome.estimate)}")
                notify(AudioSyncStatus.Adjusted(offsetMs = outcome.model.delayUsAt(positionMs * 1_000L) / 1_000L))
            }
            is AudioSyncTracker.Outcome.Searching -> SyncLog.d(
                "searching ${outcome.estimate?.let(::describe) ?: "-"} known=${timeline.knownFrameCount()} in ${elapsed}ms",
            )
            else -> Unit
        }
        updatePool(current)
    }

    private fun notify(status: AudioSyncStatus) {
        try {
            onStatus(status)
        } catch (_: Throwable) {
        }
    }

    private fun describe(estimate: SubtitleAudioAligner.Estimate): String =
        "scale=${"%.5f".format(java.util.Locale.US, estimate.scale)} shift=${estimate.shiftMs.roundToInt()}ms " +
            "peak=${"%.3f".format(java.util.Locale.US, estimate.peak)} prominence=${"%.3f".format(java.util.Locale.US, estimate.prominence)} " +
            "cues=${estimate.cueCount} speech=${estimate.speechSeconds.roundToInt()}s"

    private fun describe(format: Format): String =
        "${format.sampleMimeType} ${format.channelCount}ch ${format.sampleRate}Hz lang=${format.language} id=${format.id}"

    companion object {
        private const val MIN_TRACK_CUES = 20
        private const val ALIGN_INTERVAL_MS = 8_000L
        private const val SEARCH_INTERVAL_MS = 3_000L
        private const val DIAGNOSTICS_INTERVAL_MS = 1_000L
        private const val PREFERENCES = "nuvio_audio_sync"
        private const val MAX_ALTERNATIVES = 12

        // Sampling audio across the film.
        private const val SPOT_COUNT = 4
        /** One connection per spot, so every spot arrives in the same round. */
        private const val SPOT_WORKERS = SPOT_COUNT
        private const val SPOT_MS = 30_000L
        private const val TARGET_SPOT_BYTES = 150L * 1_000_000L
        private const val MAX_SPOT_BYTES = 400L * 1_000_000L

        /** Enough for a few whole sentences. */
        private const val MIN_SPOT_AUDIO_MS = 8_000L
        private const val MIN_SAMPLED_FILM_MS = 20 * 60_000L
        private const val RESUME_THRESHOLD_MS = 10 * 60_000L
        private const val SPOT_BACKOFF_MS = 5L
        private const val DRAIN_TIMEOUT_MS = 10_000L

        /** Playback has started and buffered this much before sampling competes for bandwidth. */
        private val SAMPLE_AFTER_FRAMES = (5_000 / SpeechTimeline.FRAME_DURATION_MS).toInt()

        /** 10 minutes of audio analysed before any subtitle is chosen. */
        private val PRE_SESSION_FRAMES = (10 * 60 * 1_000 / SpeechTimeline.FRAME_DURATION_MS).toInt()
        private const val FULL_ANALYSIS_AFTER_LOCK_MS = 10 * 60_000L
        private const val DUTY_WINDOW_US = 60_000_000L
        private const val DUTY_CYCLE = 3L

        @Volatile
        private var cachedWeights: SileroVadWeights? = null

        private fun loadWeights(context: Context): SileroVadWeights {
            cachedWeights?.let { return it }
            return synchronized(this) {
                // res/raw/silero_vad_v5_16k.bin, looked up by name: the host tests compile this
                // package with kotlinc alone, without the generated R class.
                @Suppress("DiscouragedApi")
                val id = context.resources.getIdentifier("silero_vad_v5_16k", "raw", context.packageName)
                check(id != 0) { "silero_vad_v5_16k missing from res/raw" }
                cachedWeights ?: context.resources.openRawResource(id).use {
                    SileroVadWeights.read(it)
                }.also { cachedWeights = it }
            }
        }

        /** Wi-Fi or Ethernet (not metered): Reshaped's AsrModel.isUnmetered. */
        fun isUnmetered(context: Context): Boolean = runCatching {
            val manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
            manager.isActiveNetworkMetered.not()
        }.getOrDefault(false)

        private val periodPrefix = Regex("^(\\d+:)+")

        /**
         * Same audio track, tolerating the Format differences between the extractor output and the
         * player's track groups (MergingMediaSource prefixes ids with the child index, e.g. "0:2").
         */
        private fun matches(a: Format, b: Format): Boolean {
            if (a == b) return true
            if (a.sampleMimeType != b.sampleMimeType) return false
            val aId = a.id?.replace(periodPrefix, "")
            val bId = b.id?.replace(periodPrefix, "")
            if (!aId.isNullOrEmpty() && !bId.isNullOrEmpty()) return aId == bId
            return a.language == b.language && a.channelCount == b.channelCount && a.sampleRate == b.sampleRate
        }
    }
}

/** Progress worth showing to the user. */
internal sealed interface AudioSyncStatus {
    /** A subtitle was picked and the audio is being analysed. */
    data object Listening : AudioSyncStatus

    /** No second decoder for this codec: syncing uses the playing audio only, without look-ahead. */
    data class LiveOnly(val mimeType: String) : AudioSyncStatus

    /** The speech detector could not be loaded: nothing can be synced to the audio here. */
    data object Unavailable : AudioSyncStatus

    /** The subtitle has too few dialogue lines to sync. */
    data class TooFewLines(val lines: Int) : AudioSyncStatus

    /** The early estimate could not be confirmed; subtitles are back on the file's own timing. */
    data object Withdrawn : AudioSyncStatus

    /** An early, unconfirmed estimate was applied; it keeps adjusting until confirmed. */
    data class Estimated(val offsetMs: Long) : AudioSyncStatus

    /** Subtitles now follow the audio; [offsetMs] is the applied delay at the playhead. */
    data class Synced(val offsetMs: Long, val rateCorrected: Boolean) : AudioSyncStatus

    /** The subtitle timing changed partway through (a different cut). */
    data class Adjusted(val offsetMs: Long) : AudioSyncStatus
}

/** Format of the audio track the player currently has selected, if any. */
internal fun Tracks.selectedAudioFormat(): Format? {
    for (group in groups) {
        if (group.type != C.TRACK_TYPE_AUDIO || !group.isSelected) continue
        for (index in 0 until group.length) {
            if (group.isTrackSelected(index)) return group.getTrackFormat(index)
        }
    }
    return null
}
