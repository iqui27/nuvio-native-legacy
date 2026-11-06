package space.nuvio.nativelegacy

internal object ThumbnailProbeAdmission {
    fun allowed(sourceOpen: Boolean, playWhenReady: Boolean, loading: Boolean, healthAt: Long,
                now: Long, sourceEpoch: Long, expectedEpoch: Long): Boolean =
        sourceEpoch == expectedEpoch && (!sourceOpen ||
            (!playWhenReady && !loading && healthAt > 0 && now-healthAt in 0..2500))
}
