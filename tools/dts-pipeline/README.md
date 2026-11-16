# Starfish packet adapters

`src/dts/adapter/` bridges `nuvio_dts_adapter_v2` to LG firmware via
`dlopen`/`dlsym`. Missing symbols leave fallback unavailable. No firmware library
is bundled. `tools/arm.sh` builds/stages both C++ string ABIs automatically:
`dts-starfish-webos3.so` (legacy) and `dts-starfish-webos4.so` (modern).

Standalone build: `bash tools/build-dts-pipeline.sh --output build/dts-pipeline`.
Defaults: Podman, `localhost/nuvio-webos-sdk:latest`; override with
`NUVIO_CONTAINER_RUNTIME`/`NUVIO_DTS_SDK_IMAGE`. Runtime lookup uses `lib/`
beside the executable; `NUVIO_DTS_ADAPTER_DIR` supports isolated fixtures.

The worker serializes controls, feeds preroll after Load, and plays once both
streams and loadCompleted are ready. Production seeks recreate the pipeline.
The isolated `NV_NATIVE_SEEK_TEST` application build also tries the public
`StarfishMediaAPIs::Seek` on healthy DTS/DV playback with unchanged output audio
format. The native protocol receives decimal milliseconds and acknowledges with
`SEEKDONE`; the adapter suppresses old frame clocks until that acknowledgement
and reapplies play/pause on the worker after fresh A/V preroll. Missing/refused
Seek or lack of progressing presentation clocks triggers one fresh-Load fallback
and disables native reuse for that session. The validated demux and bounded HTTP
cache survive healthy seeks and audio changes, including the fallback.

Protocol reference: LG's [webOS Chromium BUFFERSTREAM implementation](https://github.com/webosose/chromium84-temp/blob/df04aa361ad493d47003a80ebfa3034dfc9de6e0/src/media/neva/webos/media_platform_api_webos_smp.cc)
(`Seek`, `SEEKDONE` and `MakeLoadData`). The adapter uses an optional exported
method; it does not inspect firmware player/pipeline layouts or scan memory.
Actual decoder continuity and Dolby Vision signalling still need LG TV testing.

 Feed copies
packets: `Ok` releases them, `BufferFull` retries, and `Pending`/unknown replies
fail while retaining bytes through destruction. Ownership follows
[Kodi's reference](https://github.com/xbmc/xbmc/blob/c94ca95c20ce273cc76bda6e2ab9cadaa8504b20/xbmc/cores/VideoPlayer/MediaPipelineWebOS.cpp)
and requires physical-firmware verification.

`bash tests/dts_pipeline.sh` tests both ABIs against a fake native library.
See the [DTS guide](../../docs/features/dts/README.md) for limits and licenses,
and [DTS Debug builds](DEBUG.md) for a separate test app.

The TV test primes one borrowed engine packet while the existing DV player
is paused, before issuing native Seek. It uses the existing pending-frame slot,
not another packet queue. A four-second DV no-progress guard allows bounded
post-seek preroll; DTS retains its two-second guard. Startup audio selection
reuses the validated demux and makes one final resume seek, with full reopen on
reconfiguration failure. Native DV MP4 stays on URI playback; native P5/P8 MKV
respects the separate MKV preference. The v6 forced native-DV handoff was
withdrawn after ambiguous audio mapping caused unnecessary startup reloads.
The UI holds the selected seek position through stale clocks/keyframe preroll,
with a bounded timeout; the native clock itself is not rewritten.

In v8, initial DTS resume seconds or an account percentage are handed to the
converter before its first native Load. Percentages use the demux duration;
the main player acknowledges that initial position once rather than starting
at zero and issuing another resume Load. Missing duration/failed preparation
keeps the normal native fallback. A later audio switch uses the current clock,
and an explicit pending seek takes precedence over remembered startup data.
The opening artwork/loading state now remains until native playback starts
and the LG video-plane bind finishes,
which also keeps the converted-audio notice from appearing during preroll.
This does not remove the time needed to fetch/probe the source or initialize
the audio converter. Cache byte limits and RAM safeguards are unchanged.

In v9 the converted-DTS island notice is delivered once per source. Seeking,
temporary loading and native reloads cannot restart it; failures still retain
their separate warnings. A new source can announce conversion again.

For the TV experiment only, a DV range actively blocking playback gets a
five-second guard in addition to the existing idle guard on the first two
attempts. This catches a slow trickle which otherwise evades the idle guard
and holds later cached bytes behind a 15-second transport timeout. The same
validated interval is retried; other queue slots remain allocated. Speculative
ranges keep their normal timeout, and the final retry retains the full timeout.
Completed valid responses are accepted even at the budget boundary. Byte caps,
HTTP validation, cancellation and the SDR/DTS transport behavior are unchanged.

In v10, the SDR/DTS opening cover ends once playback and the LG video-plane
bind are ready. Later seek pauses and same-source decoder reloads retain the
open video hole without restoring the poster or the "opening source" island.
A new source still gets the full startup cover; errors, actual rebuffering and
the DV path retain their previous behavior. This is a UI state change only:
packet seeking, conversion, HTTP buffering and RAM limits are unchanged.
When the SDR converter takes over from the URI player, its retired
bufferingStart stamp is also cleared: the old player cannot send bufferingEnd
after unload. New converter buffering events still control the real indicator.

The v10 TV build also opts into `NV_DV_TRUEHD_TEST`. A selected TrueHD track
stays on the DV packet path instead of handing the full source back to the
native URI player (HDR10). Only its audio is decoded and encoded as stereo
AAC or AC3 5.1 using the existing output preference; TrueHD losslessness and
Atmos metadata are not preserved. Exact track mapping, P7 RPU conversion,
bounded HTTP buffering and fallback on a real conversion error are retained.
Codec changes may need a new native Load to renegotiate the audio format.

The SDK must be built with `--build-arg NUVIO_DV_TRUEHD=1`; this adds the
TrueHD decoder and MLP parser without changing normal SDK defaults. The test
workflow uses a separate SDK cache namespace to prevent reuse of a decoder-less
image. `NV_TEST_TRUEHD=1` enables dedicated LG routing/controller tests and
the real FFmpeg MEL/FEL engine fixture: TrueHD 5.1 to AAC/AC3, initial track
selection, seeks and switches back to EAC3 must retain P8.1 RPU packets.
The RAM overlay starts hidden; the existing green-button toggle is unchanged.

In v11 the completed-opening latch also covers DV packet playback, but only
after the TV has confirmed DolbyVision and the video-plane bind has finished.
DV seek pauses, decoder recovery and audio changes then keep the startup poster
and opening card closed. Real errors and each new source retain their cover.
Selected, prepared TrueHD conversion gets its own localized island notice once
per source, after native playback/bind and parental-card deferral, without a DTS
badge or repeat announcements on seeks/reloads.

Changing AC3 5.1 passthrough to stereo AAC still requires native audio-format
renegotiation through Load. The v10 TV log shows the retained source/cache
(`reused=1`, source seek 2 ms) and confirmed DV8 afterward. A prior no-progress
seek had also disabled native reuse for that session. v11 preserves both format
validation and that recovery safeguard; hiding the startup poster does not
claim to eliminate the decoder interruption required by a format change.

In v12, the TV experiment queues a track change and its position atomically;
it no longer wakes the worker with an independent Seek first. Selecting the
already selected track is a no-op. A compatible audio metadata revision keeps
the native plane/playing observation instead of rebinding it; a separate Load
generation still resets and binds a genuinely replaced renderer (including a
reused firmware media ID).

For a DV audio-format reload, the old renderer is paused while the existing
demux/audio chain is reconfigured and one normal borrowed packet is fetched.
Only then is that renderer unloaded and the new codec configuration loaded.
The packet uses the existing feed slot: no captured DV image, second player,
extra queue, audio downmix of otherwise supported tracks, or buffer-limit change
is introduced. Refused Pause retains the previous Load path; cancellation and
decode/core recovery remain bounded. An expected format change no longer
permanently disables compatible native seeks, while a real refused/stalled
Seek still does so for the rest of that session.

Diagnostics distinguish `audio-format-reload`, `dv-audio-reload-prime` and
`dv-audio-prepared-before-unload` from a failed native seek. Host fixtures check
that the old renderer survives a blocked preparation, cancellation closes it,
AC3/AAC format changes retain DV/cache, pause and recovery guards survive, and
metadata-only switches do not rebind the picture. Actual black-frame duration
and firmware frame retention still require TV comparison with v11: a changed
audio format continues to need a native Load, so zero visible interruption is
not guaranteed. Native URI playback is unchanged.
