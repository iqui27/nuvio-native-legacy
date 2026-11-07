package space.nuvio.nativelegacy

internal fun thumbnailOverlapBufferHealthy(ready: Boolean, playing: Boolean,
    bufferMs: Long, ageMs: Long, allocatedBytes: Long, targetBytes: Long, stable: Boolean): Boolean {
    if (!ready || !playing || ageMs !in 0..2000) return false
    return bufferMs >= 8000 || (bufferMs >= 3000 && stable && targetBytes > 0 &&
        allocatedBytes >= targetBytes * 90 / 100)
}
