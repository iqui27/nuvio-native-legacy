package space.nuvio.nativelegacy

internal class ThumbnailPlaybackGate {
    private var waiting = false
    fun shouldWait(creationWindow: Boolean, state: Int, bufferMs: Long, ageMs: Long): Boolean {
        if (creationWindow) { waiting = false; return false }
        if (state != 3 || bufferMs < 2000 || ageMs !in 0..2000) waiting = true
        else if (bufferMs >= 3000) waiting = false
        return waiting
    }
}
