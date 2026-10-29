# HTTP read-ahead buffer from custom 2.0.7

This branch isolates the progressive HTTP/HTTPS RAM buffer for review against
upstream 2.0.8 (`87c8b8909f0d605f1d8351147fe5afb9de233b8d`). The source is the
buffer implementation used in the custom 2.0.7 build, separated from its
formatter, Dolby Vision, DTS recovery and UI changes.

## Behavior

- The existing 64/128/256 MiB setting controls a single-connection RAM ring.
- A loopback HTTP server serves the buffered bytes to the normal LG player.
- Recheck `MemAvailable` at load time using the unchanged official RAM policy:
  retain the 100 MiB floor and 4 MiB allowance; reduce to 128/64 MiB or Auto
  when necessary. There is no option to bypass this check.
- Disable custom native `bufferControl` when the RAM proxy is active, avoiding
  a second requested 64/128/256 MiB native buffer on top of the RAM ring.
- Auto, live modes, known HLS/DASH URLs, loopback URLs and unsupported origins
  retain their existing native playback/fallback paths.
- Cancel the RAM reader and join its workers when the native session closes.
- The ordinary buffered rail can include an estimate from measured queued
  bytes and average file bitrate; VBR means this is not an exact timestamp.
- DTS conversion publishes its existing range queue from the owning worker to
  the player status. The rail combines frames already fed to LG with a byte
  estimate that stops at the first missing range. Pausing still refreshes the
  snapshot; seeking, changing audio or closing clears it.
- DTS allocation, decoding, P7 selection and recovery remain those of official
  2.0.8. Reporting works with P7 disabled and adds no second DTS buffer.

## Player presentation

`src/player.c` is unchanged. This branch adds no buffer MiB counters, status
labels, markers, RAM overlay or other player widgets. Existing settings,
formatter behavior and DTS/Dolby Vision policies are unchanged.

## Tests

- `bash tests/moviebuffer.sh` — actual HTTP fixtures for all three budgets,
  exact byte comparisons, seeks, concurrent ranges, header/redirect handling,
  playlists, unsupported origins and bounded cancellation. Requires the minimal
  host FFmpeg build (`NUVIO_DTS_ROOT`).
- `bash tests/video_moviebuffer.sh` — native and DTS buffered rails, range gaps,
  fed timestamps, duration clamp, fallback and shutdown after invalidation.
- `bash tests/dts_buffer_reporting.sh` — real SDR/DTS HTTP prefetch at
  64/128/256 MiB with P7 disabled, out-of-order ranges and seek reporting.
  Requires host FFmpeg CLI and the minimal build (`NUVIO_DTS_ROOT`).
- `bash tests/dts_playback.sh` — controller regressions and buffer snapshots
  while paused and during a blocked seek/reopen, with DV/P7 disabled.
- `bash tests/video_http_buffer.sh` — native payload and official RAM clamp.
  Its old 192 MiB-floor expectations are aligned with the existing 100 MiB floor.
- `bash tests/video_http_buffer_build.sh` — compile the touched webOS units.
- `bash tests/video_acb_lg.sh` — existing native load/dispatcher regression.
- `tests/buffer_tamanho.c` — unchanged official safety thresholds.

The matching implementation was confirmed working on a physical LG in the
2.0.8 diagnostic IPK. This branch retains that buffering and rail behavior but
omits its numeric counter, diagnostic logging and test-only player overlay.
