#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-webos-hls-ffmpeg}
work=$(mktemp -d)
origin=''
trap 'if [ -n "$origin" ]; then kill "$origin" 2>/dev/null || true; wait "$origin" 2>/dev/null || true; fi; rm -rf "$work"' EXIT
NV_DV7_FIXTURE_OUT="$work" cargo test --locked --manifest-path tools/dv7/Cargo.toml
dv7=${NUVIO_DV7_ROOT:-"$work/dv7"}
NUVIO_DV7_ROOT="$dv7" bash tools/build-dv7.sh

ffmpeg -v error -f lavfi -i testsrc2=s=64x64:r=25:d=7 \
  -f lavfi -i sine=sample_rate=48000:d=7 -ac 6 -c:a dca -strict experimental -b:a 768k \
  -c:v libx264 -preset ultrafast -g 25 -sc_threshold 0 "$work/dts.mkv"
ffmpeg -v error -f lavfi -i testsrc2=s=64x64:r=25:d=7 \
  -f lavfi -i sine=sample_rate=48000:d=7 -ac 6 -c:a ac3 \
  -c:v libx265 -x265-params 'bframes=0:pools=1:frame-threads=1:log-level=error:keyint=25:scenecut=0' \
  "$work/plain.mkv"
# Distinct AAC tones in an MP4 donor identify the explicitly chosen stream.
ffmpeg -v error -f lavfi -i sine=frequency=330:sample_rate=48000:d=7 \
  -f lavfi -i sine=frequency=880:sample_rate=48000:d=7 \
  -map 0:a -map 1:a -c:a aac -metadata:s:a:0 language=eng -metadata:s:a:1 language=pol \
  -movflags +faststart "$work/donor.m4a"
ffmpeg -v error -f lavfi -i color=s=64x64:r=25:d=7 -i "$work/donor.m4a" \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 25 -c:a copy \
  -metadata:s:a:1 title="Polski lektor" "$work/donor.mkv"
# Reproduce the C5 log: the first track is TrueHD, followed by AC-3.
ffmpeg -v error -i "$work/plain.mkv" -f lavfi -i sine=frequency=550:sample_rate=48000:d=7 \
  -map 0:v -map 1:a -map 0:a -c:v copy -c:a:0 truehd -ac:a:0 6 -c:a:1 copy \
  -strict experimental "$work/dv-source.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$work/dv-source.mkv" > "$work/plain.json"
python3 tests/dv7_fixture.py "$work" --indexed
# Remux adds Matroska Cues and retains the fixture's Dolby Vision track mapping.
ffmpeg -v error -i "$work/fel.mkv" -map 0 -c copy -strict unofficial "$work/indexed.mkv"

# More than 60 seconds without a video keyframe, with enough encoded data to
# exceed the fMP4 fragment budget when everything is staged at once.
ffmpeg -v error -f lavfi -i testsrc2=s=256x144:r=10:d=82 \
  -f lavfi -i sine=sample_rate=48000:d=82 -ac 6 -c:a dca -strict experimental -b:a 768k \
  -c:v libx264 -preset ultrafast -g 1000 -sc_threshold 0 -force_key_frames 0,3 \
  -b:v 6000k -maxrate 6000k -bufsize 6000k -x264-params nal-hrd=cbr:filler=1 "$work/long-tail.mkv"
# Overstate the segment's duration, then also overstate the tracks' duration
# in a second fixture to simulate a source cut short relative to its metadata.
python3 - "$work" <<'PY_FIXTURE'
from pathlib import Path
import re
import struct
import sys
root = Path(sys.argv[1])
data = bytearray((root / 'long-tail.mkv').read_bytes())
duration = data.index(b'\x44\x89\x88', 0, 1024) + 3
data[duration:duration+8] = struct.pack('>d', 180000.0)
(root / 'overstated-duration.mkv').write_bytes(data)
header, count = re.subn(rb'(DURATION\x44\x87.)\d\d:\d\d:\d\d\.\d{9}',
                        rb'\g<1>00:03:00.000000000', data[:4096])
assert count == 2, count
data[:4096] = header
(root / 'short-source.mkv').write_bytes(data)
PY_FIXTURE

"${CC:-cc}" -std=gnu11 -O1 -g -I"$prefix/include" -Isrc \
  -DNV_WEBOS -DNV_WEBOS_HLS_TEST -DNV_DTS_FFMPEG -DNV_DV7_CONVERSION \
  tests/webos_hls.c src/dts/dts_hls_tpk.c src/dts/dts_remux.c src/dts/dts_engine.c src/rede.c src/redeurl.c \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
  "$dv7/libnuvio_dv7.a" -lz -lm -lpthread -ldl -o "$work/server"
python3 tests/dts_tpk_servidor_origem.py "$work" "$work/port" "$work/origin.log" &
origin=$!
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
python3 tests/webos_hls.py "$work"
