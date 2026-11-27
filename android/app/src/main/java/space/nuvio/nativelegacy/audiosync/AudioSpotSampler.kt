@file:OptIn(androidx.media3.common.util.UnstableApi::class)

package space.nuvio.nativelegacy.audiosync

import android.media.MediaFormat
import android.net.Uri
import android.os.Process
import androidx.media3.datasource.DataSource
import androidx.media3.datasource.DataSpec
import androidx.media3.datasource.TransferListener
import androidx.media3.exoplayer.MediaExtractorCompat
import androidx.media3.extractor.ExtractorsFactory
import java.util.concurrent.atomic.AtomicInteger
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import kotlin.math.abs

/**
 * Reads a few short stretches of a film's audio far from the playhead, over its own connection, so
 * sync has evidence from across the film within seconds instead of only what playback has reached.
 *
 * The audio itself is delivered by the extractors factories (wrapped with an audio tap) while this
 * class only seeks and advances. Each worker reads spots over its own connection, so two far-apart
 * spots arrive together. Containers interleave audio with video, so each spot costs its share of
 * the whole stream: a spot stops at its share of [targetBytes]. On a high-bitrate file that share
 * can hold only a few seconds, too little for whole sentences, so a spot keeps reading until it has
 * [minSpotMs] of audio when the connection is clearly faster than the stream (playback keeps its
 * bandwidth), never past [maxBytes] in total. Blocking; run it on a background thread.
 */
internal class AudioSpotSampler(
    private val uri: Uri,
    dataSourceFactory: DataSource.Factory,
    private val targetBytes: Long,
    private val maxBytes: Long,
    private val minSpotMs: Long,
) {
    class Result(val sampled: Int, val bytes: Long, val failure: String?)

    private val bytes = AtomicLong()
    private val countingFactory = DataSource.Factory {
        dataSourceFactory.createDataSource().apply { addTransferListener(ByteCounter(bytes)) }
    }

    /**
     * Reads [spotMs] of audio from each of [spotsMs], taken in order by one worker per entry of
     * [workers] (each an extractors factory feeding its own decoder), until done, [isCancelled] or
     * out of budget. [onSpot] reports progress after each spot, from any worker thread.
     */
    fun run(
        spotsMs: List<Long>,
        spotMs: Long,
        workers: List<ExtractorsFactory>,
        isCancelled: () -> Boolean,
        onSpot: (sampled: Int, bytes: Long) -> Unit,
    ): Result {
        val next = AtomicInteger(0)
        val sampled = AtomicInteger(0)
        val failure = AtomicReference<String?>(null)
        val stop = { isCancelled() || failure.get() != null || bytes.get() >= maxBytes }
        val startedNs = System.nanoTime()

        fun work(extractorsFactory: ExtractorsFactory) {
            val extractor = MediaExtractorCompat(extractorsFactory, countingFactory)
            try {
                extractor.setDataSource(uri, 0L)
                var audioTracks = 0
                for (index in 0 until extractor.trackCount) {
                    val mime = extractor.getTrackFormat(index).getString(MediaFormat.KEY_MIME).orEmpty()
                    if (mime.startsWith("audio/")) {
                        extractor.selectTrack(index)
                        audioTracks++
                    }
                }
                if (audioTracks == 0) {
                    failure.compareAndSet(null, "no audio track")
                    return
                }
                while (!stop()) {
                    val index = next.getAndIncrement()
                    if (index >= spotsMs.size) return
                    val spotStartMs = spotsMs[index]
                    val share = (targetBytes - bytes.get()).coerceAtLeast(0L) / (spotsMs.size - index)
                    val spotStartBytes = bytes.get()
                    extractor.seekTo(spotStartMs * 1_000L, MediaExtractorCompat.SEEK_TO_PREVIOUS_SYNC)
                    val firstUs = extractor.sampleTime
                    if (firstUs < 0 || abs(firstUs / 1_000L - spotStartMs) > MAX_SEEK_MISS_MS) {
                        failure.compareAndSet(null, "stream cannot seek")
                        return
                    }
                    val endUs = (spotStartMs + spotMs) * 1_000L
                    while (!stop()) {
                        val timeUs = extractor.sampleTime
                        if (timeUs < 0 || timeUs >= endUs) break
                        val spotBytes = bytes.get() - spotStartBytes
                        if (spotBytes >= share && !wantsMore(spotBytes, timeUs - firstUs, startedNs)) break
                        if (!extractor.advance()) break
                    }
                    onSpot(sampled.incrementAndGet(), bytes.get())
                }
            } catch (error: Exception) {
                failure.compareAndSet(null, error.message ?: error.javaClass.simpleName)
            } finally {
                runCatching { extractor.release() }
            }
        }

        val helpers = workers.drop(1).map { extractorsFactory ->
            Thread({
                Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND)
                work(extractorsFactory)
            }, "NuvioAudioSyncSpots").apply {
                isDaemon = true
                start()
            }
        }
        workers.firstOrNull()?.let(::work)
        helpers.forEach(Thread::join)
        return Result(sampled.get(), bytes.get(), failure.get())
    }

    /**
     * Whether a spot past its share keeps reading: only while it has less than [minSpotMs] of audio
     * ([readUs] so far, costing [spotBytes]) and all workers together download at least
     * [MIN_SPEED_RATIO] times faster than the stream plays.
     */
    private fun wantsMore(spotBytes: Long, readUs: Long, startedNs: Long): Boolean {
        if (readUs <= 0 || readUs >= minSpotMs * 1_000L) return false
        val streamBytesPerSec = spotBytes * 1_000_000.0 / readUs
        val elapsedSec = (System.nanoTime() - startedNs) / 1e9
        val downloadBytesPerSec = if (elapsedSec > 0) bytes.get() / elapsedSec else 0.0
        return downloadBytesPerSec >= MIN_SPEED_RATIO * streamBytesPerSec
    }

    private class ByteCounter(private val bytes: AtomicLong) : TransferListener {
        override fun onTransferInitializing(source: DataSource, dataSpec: DataSpec, isNetwork: Boolean) = Unit

        override fun onTransferStart(source: DataSource, dataSpec: DataSpec, isNetwork: Boolean) = Unit

        override fun onBytesTransferred(source: DataSource, dataSpec: DataSpec, isNetwork: Boolean, bytesTransferred: Int) {
            bytes.addAndGet(bytesTransferred.toLong())
        }

        override fun onTransferEnd(source: DataSource, dataSpec: DataSpec, isNetwork: Boolean) = Unit
    }

    private companion object {
        /** A seek landing further than this from the target means the stream has no usable index. */
        const val MAX_SEEK_MISS_MS = 30_000L

        /** Download speed over stream bitrate needed to read past a spot's share. */
        const val MIN_SPEED_RATIO = 3.0
    }
}
