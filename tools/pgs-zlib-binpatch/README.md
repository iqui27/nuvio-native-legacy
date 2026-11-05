# PGS zlib binary patch for Nuvio 2.0.8 (webOS)

Stopgap for one TV. It makes zlib-compressed PGS subtitles reach the decoder in
the released `space.nuvio.native.legacy_2.0.8_arm.ipk` binary. The real fix is
the source change in `src/mkvass.c`.

A source build needs the `local.properties` values compiled into the release,
and those aren't public. This patch keeps the released binary and its embedded
configuration unchanged.

## What it changes

Three regions of `nuvio-proto`, with the file size unchanged:

| File offset | Change |
|---|---|
| `0xa4` | RX `PT_LOAD` size grows to cover the new code |
| `0x2fc5a8` | The only `bl pgs_decodificar` now calls the hook |
| `0x1a32340` | `hook.c` (542 bytes), in previously empty padding |

The hook inflates a block that starts with a zlib header, then calls the
original decoder. Uncompressed blocks pass straight through, because `0x78` is
not a PGS segment type. Each inflated block logs a `[pgs-zlib]` line to
`/tmp/nuvio.log`.

## Limits

- Handles zlib only, not header stripping or other encodings.
- Applies only to the path through `mkvass.c`. PGS decoded by FFmpeg, on the DTS
  and Dolby Vision playback path, is unchanged. The webOS FFmpeg build has no
  zlib.
- An in-app update replaces the patched binary.

## Build

```sh
TC=<sdk>/bin/arm-webos-linux-gnueabi
python3 patch.py <original nuvio-proto> build/nuvio-proto "$TC" <sdk>/arm-webos-linux-gnueabi/sysroot
```

`patch.py` refuses any input other than the 2.0.8 binary:

- Input SHA-256: `7d5c996190c00e76cd3aa37d5760f7a4aba4dd1a882718cd5accdc1d0950180b`
- Output SHA-256: `899f333abd60ff31250d71def0d4bcde03dc22cc69f6aed56073d05478ea2a1a`

## Roll back

The original is kept next to the patched binary. Close the app, then run this on
the TV:

```sh
cd /media/developer/apps/usr/palm/applications/space.nuvio.native.legacy
mv nuvio-proto nuvio-proto.patched && mv nuvio-proto.orig nuvio-proto
```
