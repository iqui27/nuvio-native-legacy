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
ffprobe -v error -show_streams -show_packets -show_data -of json "$work/plain.mkv" > "$work/plain.json"
python3 tests/dv7_fixture.py "$work" --indexed
# Remux adds Matroska Cues and retains the fixture's Dolby Vision track mapping.
ffmpeg -v error -i "$work/fel.mkv" -c copy -strict unofficial "$work/indexed.mkv"

"${CC:-cc}" -std=gnu11 -O1 -g -I"$prefix/include" -Isrc \
  -DNV_WEBOS -DNV_WEBOS_HLS_TEST -DNV_DTS_FFMPEG -DNV_DV7_CONVERSION \
  tests/webos_hls.c src/dts/dts_hls_tpk.c src/dts/dts_remux.c src/dts/dts_engine.c src/rede.c src/redeurl.c \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
  "$dv7/libnuvio_dv7.a" -lz -lm -lpthread -ldl -o "$work/server"
python3 tests/dts_tpk_servidor_origem.py "$work" "$work/port" "$work/origin.log" &
origin=$!
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
python3 tests/webos_hls.py "$work"
