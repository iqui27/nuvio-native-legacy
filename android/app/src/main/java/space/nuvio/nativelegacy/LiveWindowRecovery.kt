package space.nuvio.nativelegacy

import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.hls.playlist.HlsPlaylistTracker

// Recover the existing live player before asking native code to replace its URL.
// The source's default live position respects its advertised live window/holdback.
@androidx.annotation.OptIn(UnstableApi::class)
internal class LiveWindowRecovery(
    private val schedule: (Runnable, Long) -> Unit,
    private val unschedule: (Runnable) -> Unit,
    private val active: () -> Boolean,
    private val status: (Long) -> Unit = {},
) {
    private var pending: Runnable? = null
    private var windowStart = -1L
    private var attempts = 0

    companion object {
        // Same bounded HLS policy as nv_ls_buffer_prazo in the native watchdog.
        fun timeoutForSegment(segmentMs: Int): Long = when {
            segmentMs <= 0 -> 8_000L
            segmentMs >= 20_000 -> 60_000L
            else -> maxOf(30_000L, segmentMs * 3L)
        }
    }

    fun recover(player: Player, error: PlaybackException, nowMs: Long, timeoutMs: Long = 8_000L,
                failed: () -> Unit): Boolean {
        if (!active()) return false
        // Media3 releases the source/tracker on a fatal I/O error. Preparing the
        // same player then reads a fresh playlist, without replacing the URL.
        // Only the typed reset is recoverable here, not arbitrary I/O failures.
        val reset = error.errorCode == PlaybackException.ERROR_CODE_IO_UNSPECIFIED &&
            generateSequence(error.cause) { it.cause }.take(8)
                .any { it is HlsPlaylistTracker.PlaylistResetException }
        if (error.errorCode != PlaybackException.ERROR_CODE_BEHIND_LIVE_WINDOW &&
            !(reset && player.isCurrentMediaItemLive)) return false
        // A second error before READY is a failed recovery, not another hidden retry.
        if (pending != null) return false
        if (windowStart < 0 || nowMs - windowStart >= 60_000L) {
            windowStart = nowMs
            attempts = 0
        }
        if (attempts >= 2) return false
        attempts++
        val waitMs = timeoutMs.coerceIn(8_000L, 60_000L)
        val deadline = object : Runnable {
            override fun run() {
                if (pending !== this) return
                pending = null
                if (active()) {
                    // READY may already be visible before its listener callback.
                    // A real timeout must stop the still-preparing player before
                    // reporting failure, or it can resume behind the error UI.
                    if (player.playbackState == Player.STATE_READY) status(0L)
                    else {
                        try { player.stop() } finally { status(0L); failed() }
                    }
                }
            }
        }
        pending = deadline
        try {
            status(waitMs)
            // No setMediaItem, stop, release, play or URL lookup: retain the
            // session, surface, track choices, volume and requested pause state.
            player.seekToDefaultPosition()
            player.prepare()
            if (pending === deadline) schedule(deadline, waitMs)
        } catch (_: Exception) {
            cancel()
            return false
        }
        return true
    }

    fun ready(): Boolean {
        val recovering = pending != null
        cancel()
        return recovering
    }

    fun cancel() {
        val task = pending ?: return
        pending = null
        unschedule(task)
        status(0L)
    }
}
