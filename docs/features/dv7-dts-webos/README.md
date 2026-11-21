# Focused webOS DV7 and DTS topic branch

Based on official master `eb520363755ece93aba8aff52396e0b624be064e`.
Only two feature groups are carried here; this branch is independent of the
owner's full custom build.

## DV7

An independent, device-local setting enables P7 MKV -> P8.1 on LG webOS.
It defaults to **Off** and is shown only when `NV_DV7_CONVERSION` is compiled.
The converter rewrites RPU metadata and keeps the base layer; EL/FEL is
discarded, without software reconstruction of FEL. Existing P5/P8 selection,
RPU validation and crash-guard behavior remain in place.

The webOS build links the pinned libdovi bridge and its dependency licenses.
The official Rust MKV core remains enabled; `external-personality` prevents a
duplicate Rust personality when both archives are linked. Standalone MKV-core
builds retain their original personality stub.

## DTS stability

- Retry the same failed HTTP byte range without discarding good prefetch.
- Abort/retry an actively awaited, stalled range early, while leaving startup,
  progressing transfers and speculative reads alone.
- Pad only bounded, short forward audio timestamp gaps; reject backward,
  oversized or sustained discontinuities.
- Bound native timeline recovery attempts and preserve pause/track state.
- Reuse a healthy open source for explicit seeks, with full-reopen fallback.
- Invalidate closing native callbacks before replacing a DTS session.

New HTTP retry/padding behavior is enabled by `NV_DTS_WEBOS_RECOVERY`, added
by the webOS ARM build. Without this flag, failed range reads receive the
original single fetch and timestamp discontinuities retain the original
rejection. Samsung/Android builds do not opt in. The webOS controller is already
platform-gated. Numeric recovery logs are included; no QR export or RAM overlay
is added.

## Scope and validation

The official native HTTP buffer (including current upstream updates), buffer
setting, memory policy, Android cache actions, Samsung DNS changes and P2P-tail
startup safeguard are retained. No custom movie proxy, RAM overlay, filename
wrapping, general log exporter or old buffer-setting migration is included.

Host checks run in `.github/workflows/dv7-dts-webos.yml` on this branch.
They cover default/no-recovery behavior, webOS recovery, the existing native
HTTP payload/fallback, controller state, real HTTP faults and DTS conversion,
P2P startup, DV7 fixtures, settings isolation and both Rust link modes.
These checks do not replace ARM linking or physical LG/Samsung/Android tests.
No IPK is built and no upstream pull request is opened by this branch.

## Experimental HLS on webOS

Under **Playback → More options → Audio and video on this TV**, set
**HLS (experimental)** to **On** or **Off**. The switch is device-local,
defaults to Off and applies when opening a video. Enable the existing DTS
and Dolby Vision P7 conversion options for the formats you want to test.
With HLS Off, those options use the existing converter.

With HLS On, the enabled conversions use the Samsung loopback server and
fMP4 with AC-3 audio, up to 5.1. P7 MKV is converted to single-layer P8.1.
The HLS variant advertises its HEVC base as `hvc1` and signals P8.1 through
`SUPPLEMENTAL-CODECS` and the Dolby Vision configuration in the init segment.
Enable both conversions for a P7 source whose selected audio track is DTS.
The stream carries one audio track, always as AC-3 5.1 (mono and stereo
fill the front channels). Choosing another track of the same file keeps the
HLS session: the next segments use that track and the player seeks to the
current position, so the picture stops briefly, as for a seek. Other Dolby Vision sources (P5, P8, or P7
without the P7 conversion) keep the existing converter when HLS is On.

HLS requires a seekable source with a keyframe index. If a webOS MKV's last
indexed keyframe is far from the reported end, the probe checks a bounded
portion of the tail for later keyframes or video that reaches the end. A long
HLS segment can carry several fMP4 fragments to limit the muxer's memory use.
A startup or stream error stops playback and shows an HLS error. Turn HLS Off and reopen the
video to use the existing converter. On an LG C1 or C5, check audio, the
TV's Dolby Vision indicator, seek/resume and memory. **Show buffer data**
shows the HLS source buffer, HTTP readers, and measured download rate once
playback starts. The rate measures bytes received from the source readers.

The follow-up HLS build uses the existing **Buffer size** presets (64, 128 or
256 MiB), with the same free-memory clamp. Automatic uses a 16 MiB input
budget. With **From another source**, the selected audio and video share that
budget. The separate **Other version buffer** preference determines the audio
reader's share, capped at half the budget. The player and segment generation
use memory outside the input rings; the free-memory clamp reserves 96 MiB for
the remux queue and one staged fMP4 fragment before it sizes the rings.

For HLS with **From another source**, choose the audio source as usual.
Enable DTS conversion and HLS for H.264/HEVC playback, or Dolby Vision P7
conversion and HLS for interleaved P7 MKV. The selected track, saved language,
manual offset and stored speed are used for the AC-3 stream. Changing the source,
track or offset reloads HLS at the current position. Continuous automatic
alignment and **Match automatically** are available with HLS Off.
