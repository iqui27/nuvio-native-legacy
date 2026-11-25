package space.nuvio.nativelegacy

internal class ThumbnailPlaybackGate {
    private var waiting = false
    private var startupAdmitted = false
    fun shouldWait(creationWindow: Boolean, state: Int, bufferMs: Long, ageMs: Long,
                   stable: Boolean = false, bufferFull: Boolean = false): Boolean {
        if (creationWindow) { waiting = false; return false }
        // Creating the codec before playback starts is harmless, but do not let its
        // HTTP/decode loop race the player's initial fill.  A merely large buffer is
        // insufficient: on the failed Dune replay it fell from 13 s to zero while
        // thumbnails were already active.  ThumbnailBufferHealth's `stable` means a
        // fresh, non-draining history spanning at least four seconds.
        // A DEEP buffer is admitted at once (06/11/2026, Rick and Morty 2160p: 50 s buffered, yet ~4 s held for the
        // stability history; a quarter of a 19 s thumbnail run). 20 s is past the 13 s that drained on Dune, and the
        // running check below (< 2 s waits) still applies.
        // A FULL buffer holds fewer seconds on a high bitrate and cannot reach 8 s (09/11/2026, Se7en 2160p at 113 Mbps:
        // 80 MB is 5 s, full and steady for 202 s of a 288 s run, then 760 thumbnails in 39 s). The player is not short
        // of data when it has stopped loading, so full and steady admits from 4 s.
        if (!startupAdmitted) {
            if (state == 3 && ageMs in 0..2000 && (bufferMs >= DEEP_BUFFER_MS || (bufferMs >= 8000 && stable) ||
                    (bufferMs >= FULL_BUFFER_MIN_MS && stable && bufferFull)))
                startupAdmitted = true
            else {
                waiting = true
                return true
            }
        }
        if (state != 3 || bufferMs < 2000 || ageMs !in 0..2000) waiting = true
        else if (bufferMs >= 3000) waiting = false
        return waiting
    }
    companion object {
        const val DEEP_BUFFER_MS = 20_000L
        const val FULL_BUFFER_MIN_MS = 4_000L
        // Allocation wobbles around the target while playing (Se7en: 83.0-85.7 MB around 83.9 MB), so 95% counts as full.
        fun isFull(allocatedBytes: Long, targetBytes: Long): Boolean =
            targetBytes > 0 && allocatedBytes >= targetBytes / 100 * 95
    }
}
