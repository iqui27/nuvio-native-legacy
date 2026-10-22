package space.nuvio.nativelegacy

// Fill the safe window; the native canvas uses its aspect to keep a uniform scale.
// The Activity applies the same physical bounds to SDL, video and text entry.
internal object TouchViewport {
    data class Bounds(val left: Int, val top: Int, val width: Int, val height: Int)

    fun fit(width: Int, height: Int, left: Int, top: Int, right: Int, bottom: Int): Bounds? {
        val safeWidth = width - left - right
        val safeHeight = height - top - bottom
        if (safeWidth <= 0 || safeHeight <= 0) return null
        return Bounds(left, top, safeWidth, safeHeight)
    }
}
