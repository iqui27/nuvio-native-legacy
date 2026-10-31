# Buffer / DTS / Dolby Vision connection experiment

Base: upstream `iqui27/nuvio-native-legacy` master
`7cac4f25d11931b56c41cfefb952e4e183c45c1a`.
Playback port: `Matis94/nuvio-native-legacy` `build/playback-v17-no-qr`
`eff64fa3b1db4580b7e5af9ff92c44abfa0cccd5`.

Only playback modules, buffer/playback settings, their build dependencies and
regressions are ported. Other upstream application changes are preserved.
No diagnostic QR or source formatter changes are imported. The upstream folder
hero setting keeps its enum position and persisted key; playback settings append
after it. The native asynchronous load and conversion URL copies accept the
upstream `NV_MEDIA_URL_MAX` limit.

## Settings

Playback > Image and sound > Buffer size, with two adjacent selectors:

| Selector | Default | Choices |
| --- | --- | --- |
| Standard native-player buffering | Standard (1) | Standard (1), Parallel 2, 3, 4 |
| DTS/TrueHD and DV7 buffering | Parallel 4 | Standard (1), Parallel 2, 3, 4 |

Both preferences are device-local, independent, and apply to the next playback
session. They do not propagate through account sync or device backup. Invalid
values recover the defaults. Each engine pins its connection limit, so changing
a setting cannot alter an in-progress playback or another reader.

The ordinary default keeps the existing single sequential HTTP ring reader.
Parallel native modes use the existing bounded range queue and one local player
client; changing HTTP concurrency does not multiply RAM capacity. Buffer size
remains 64/128/256 MiB with the official available-RAM floor and automatic
reductions. If native Parallel is explicitly selected with Automatic buffer size,
the app requests the smallest 64 MiB preset through the same safety checks.
When memory is insufficient or a source is not cacheable, the normal native
fallback remains active. Live/HLS/DASH keep their existing playback path.

Conversion Standard (1) uses one HTTP range worker, rather than introducing a
new long-lived streaming transport. Other conversion modes use 2/3/4 range
workers. The defaults keep the existing four-worker conversion behavior.
DTS/TrueHD output, P7 conversion/mapping and audio routing are separate choices.

## Validation

- Real HTTP counter: native, DTS-range and DV-range readers at 1, 2, 3 and 4;
  byte continuity, forward/backward seeks, fixed session snapshots, cancellation,
  buffer capacity and cross-origin native header stripping.
- Real DTS conversion with one worker: MKV/MP4, AAC/AC3, track changes, seeks,
  HTTP authentication and sparse offsets beyond 2 GiB.
- Real DV7/TrueHD conversion at one and four workers: FFmpeg + pinned libdovi,
  P7 to P8.1, mapping, passthrough, output audio changes, repeated seeks and
  corrupt-RPU recovery. This verifies output behavior, not television frame rate.
- Existing native sequential buffer, resolver/429 recovery, cache reuse, adapter,
  asynchronous load/fallback and settings regressions remain in CI.
- Settings exercise all 16 independent combinations through disk reload.

One worker can be correct while still too slow for a high-bitrate remux on a
particular CDN/TV. The experimental choices need playback testing on the LG;
four workers remain the conversion default. Numeric `[http-buffer]` logs identify
native mode and worker count without logging a URL or credentials.
