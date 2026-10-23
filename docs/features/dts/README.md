# DTS audio fallback on webOS

Native LG webOS builds include DTS audio fallback by default. Playback first
uses the TV's native URI pipeline. If it reports unsupported audio (error 200)
for the selected DTS track, the app converts that track to stereo AAC-LC
(48 kHz, 192 kbps) and feeds it alongside the original compressed video into
LG's hardware pipeline. TVs that accept DTS keep native playback; there is no
software video decoding or output-format setting.

## Architecture and limits

The implementation lives in `src/dts/`, with firmware adapters in
`src/dts/adapter/`. FFmpeg demuxes MKV/MP4 and converts only the selected DTS
track. HTTP input uses validated 64-bit ranges, certificate verification and
cancellation; four workers prefetch up to 33 MiB of compressed data. Native
backpressure and roughly six seconds of A/V lead bound feeding.

Audio selection, pause, seek, volume and subtitles are preserved. Seeks recreate
the native pipeline; stalls allow two recovery rebuilds. Decode/CPU failure may
retry once using an available DTS core. Stereo conversion is lossy and loses
DTS:X objects and lossless DTS-HD audio.

The path supports unencrypted MKV/MP4 films and episodes on webOS 3+.
Live TV, DRM and webOS 1/2 are excluded. H.264/HEVC/VP9/AV1 and HDR depend on the
TV's hardware. Dolby Vision requires single-layer HEVC profile 5 or 8 with base
layer/RPU and no enhancement layer; profile 7 is refused. Text and PGS/DVD
subtitles are included; ASS plain-text fallback loses styling. Host tests do not
establish playback compatibility on every physical model.

## Build

Use the [main build recipe](../../../README.md#building); rebuild older SDK
images to include FFmpeg. DTS is enabled automatically in both SDK and app.

`--build` produces local artifacts; omitting it deploys and launches on the TV.
DTS requires no launch environment variable. `NUVIO_DTS_FFMPEG=0` explicitly
omits it from an app build; `--build-arg NUVIO_DTS_FFMPEG=0` omits it from the SDK.
`local.properties` supplies server configuration; `NUVIO_PROPERTIES` selects
another file. The SDK prefix defaults to `/opt/nuvio-dts` (`NUVIO_DTS_ROOT`).

Packages include `lib/dts-starfish-webos3.so` (legacy C++ ABI) and
`lib/dts-starfish-webos4.so` (modern ABI), plus FFmpeg notices in `licenses/dts/`.
The adapters probe firmware symbols at runtime; missing or incompatible firmware
leaves fallback unavailable. See [adapter details](../../../tools/dts-pipeline/README.md)
and [isolated DTS Debug builds](../../../tools/dts-pipeline/DEBUG.md).

## Verification and troubleshooting

Build host libraries and run the focused fixtures:

```sh
CC=gcc NUVIO_DTS_HOST=1 NUVIO_DTS_ROOT=/tmp/nuvio-dts-host \
  NUVIO_DTS_BUILD=/tmp/nuvio-dts-host-build sh tools/build-dts-ffmpeg.sh
bash tests/dts_range.sh
bash tests/dts_engine.sh
bash tests/dts_pipeline.sh
bash tests/dts_playback.sh
bash tests/dts_subtitles.sh
bash tests/dts_debug.sh
python3 tests/dts_debug_build.py
```

Fixtures require C/C++ compilers, make, curl, tar/xz, sha256sum, Python 3,
OpenSSL, runtime libcurl and an FFmpeg CLI with `libopenh264`, `libvpx-vp9`,
`libaom-av1` and `dca` encoders. These fixture video encoders are not shipped.
For TV reports include model, firmware, revision, selected track and output;
check native DTS, conversion, seeks, track changes and A/V sync using the
[DTS Debug app](../../../tools/dts-pipeline/DEBUG.md).

## Samsung .tpk (Tizen 6+)

The Samsung build converts with its own path: the TV's `Tizen.Multimedia.Player`
reads the converted stream from a local HTTP server inside the app
(`src/dts/dts_hls_tpk.c`; converter `src/dts/dts_remux.c`). Video is copied byte
for byte; DTS decodes the core, TrueHD its 5.1 presentation, output AC3 5.1 at
640 kb/s (stereo sources 2.0), or AAC 2.0 with the Stereo setting. Settings has
one row on the .tpk: Off / 5.1 / Stereo (`dtsTpkLocal`).

Delivery is HLS VOD on 127.0.0.1 with a random token: the playlist comes from the
keyframe index (keyframes grouped to at least 2 s per segment), each MPEG-TS
segment is made when the player asks for it (chunked body, Range ignored), and
the player gets a 1 s prebuffer
(`X-Nuvio-Prebuffer`, applied by `tizen-tpk/Video.cs`). Seeking, position and
duration stay with the player. An MKV without Cues switches to one continuous TS
that restarts at `?t=` on seek. MKV sources decide from their track header; MP4
sources (announced as MP4, VOD) are probed by the converter beside the original,
which keeps playing until a DTS/TrueHD track is found. Only HEVC and H.264 video
are converted. Each playback logs a `[dts-tpk] mem:` line (app RSS peak, lowest
MemAvailable, reader rings).

Host test of the server with a .tpk FFmpeg build: `tests/dts_tpk_servidor.sh`.

## Dependency provenance and distribution

The static FFmpeg libraries (`avformat`, `avcodec`, `swresample`, `avutil`) use
LGPL 2.1 or later. `tools/build-dts-ffmpeg.sh` pins **7.1.5**, its source checksum
and configure flags. Software video decoding, network protocols, external
encoders, GPL and nonfree options are disabled. Packages include
`COPYING.LGPLv2.1` and `SOURCE.txt` in `licenses/dts/`.

Binary releases also need exact corresponding FFmpeg source/configuration/patches
and application source with a working relink recipe or suitable object files.
The helpers stage notices only. Preserve GPLv3 and existing third-party terms;
see [FFmpeg guidance](https://ffmpeg.org/legal.html) and
[LGPL section 6](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html).
LG firmware libraries are supplied by the TV and are not redistributed.
