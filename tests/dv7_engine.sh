#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
NV_DV7_FIXTURE_OUT="$dir" cargo test --locked --manifest-path tools/dv7/Cargo.toml
NUVIO_DV7_ROOT="$PWD/build/dv7-host" bash tools/build-dv7.sh
ffmpeg -hide_banner -loglevel error -f lavfi -i color=size=64x64:rate=25:duration=0.8 \
  -f lavfi -i sine=frequency=300:sample_rate=48000:duration=0.8 -c:v libx265 \
  -x265-params 'bframes=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
  -c:a eac3 -ac 2 "$dir/plain.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/plain.mkv" > "$dir/plain.json"
python3 tests/dv7_fixture.py "$dir"
cc -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DNV_DV7_CONVERSION \
  -I"$prefix/include" src/dts/dts_engine.c tests/dv7_engine.c -o "$dir/engine" \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil \
  -Wl,--end-group build/dv7-host/libnuvio_dv7.a -lm -lpthread -ldl
"$dir/engine" "$dir/fel.mkv" "$dir/fel-bad.mkv"
"$dir/engine" "$dir/mel.mkv" "$dir/mel-bad.mkv"
