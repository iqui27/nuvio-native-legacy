package space.nuvio.nativelegacy

internal class ThumbnailPlaybackGate {
    private var waiting = false
    private var startupAdmitted = false
    fun shouldWait(creationWindow: Boolean, state: Int, bufferMs: Long, ageMs: Long,
                   stable: Boolean = false): Boolean {
        if (creationWindow) { waiting = false; return false }
        // Creating the codec before playback starts is harmless, but do not let its
        // HTTP/decode loop race the player's initial fill.  A merely large buffer is
        // insufficient: on the failed Dune replay it fell from 13 s to zero while
        // thumbnails were already active.  ThumbnailBufferHealth's `stable` means a
        // fresh, non-draining history spanning at least four seconds.
        // A DEEP buffer is admitted at once (06/11/2026, Rick and Morty 2160p: 50 s buffered, yet ~4 s held for the
        // stability history; a quarter of a 19 s thumbnail run). 20 s is past the 13 s that drained on Dune, and the
        // running check below (< 2 s waits) still applies.
        if (!startupAdmitted) {
            if (state == 3 && ageMs in 0..2000 && (bufferMs >= DEEP_BUFFER_MS || (bufferMs >= 8000 && stable)))
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
    companion object { const val DEEP_BUFFER_MS = 20_000L }
}
