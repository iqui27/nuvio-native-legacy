# Media3 1.8.0 HLS backport

`HlsChunkSource.java` is copied from AndroidX Media 1.8.0 (Apache-2.0).
Only the empty `segments` guard in `getNextMediaSequenceAndPartIndex` is
backported from upstream: use trailing parts when no full segment exists.

Original: https://github.com/androidx/media/blob/1.8.0/libraries/exoplayer_hls/src/main/java/androidx/media3/exoplayer/hls/HlsChunkSource.java

Upstream guard: https://github.com/androidx/media/blob/release/libraries/exoplayer_hls/src/main/java/androidx/media3/exoplayer/hls/HlsChunkSource.java

Observed on Shield, 2026-11-01: PenguPlay live HLS failed after 19.35 seconds
with IndexOutOfBoundsException in HlsChunkSource.java:910 (empty segment list).
This prevents that exception; it cannot make unavailable upstream segments play.

The Gradle patchHls task removes the original class and its nested classes from
the version-locked AAR so this source is the sole definition. Other HLS classes
and Media3/FFmpeg decoder versions remain unchanged. When upgrading Media3,
remove this overlay and task after verifying the upstream fix is included.
