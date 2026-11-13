#!/bin/bash
set -euo pipefail
if [[ $(uname -s) == Darwin ]]; then
  AV_LIBS="-lavformat -lavcodec -lswresample -lavutil"
else
  AV_LIBS="-Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group"
fi
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
NV_DV7_FIXTURE_OUT="$dir" cargo test --locked --manifest-path tools/dv7/Cargo.toml
NUVIO_DV7_ROOT="$PWD/build/dv7-host" bash tools/build-dv7.sh
ffmpeg -hide_banner -loglevel error -f lavfi -i color=size=64x64:rate=25:duration=0.8 \
  -f lavfi -i sine=frequency=300:sample_rate=48000:duration=0.8 -c:v libx265 \
  -x265-params 'bframes=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
  -map 0:v -map 1:a -map 1:a -map 1:a -map 1:a -c:a eac3 -ac 2 \
  -c:a:1 truehd -ac:a:1 6 -strict -2 -c:a:2 ac3 -c:a:3 dca -ac:a:3 6 "$dir/plain.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/plain.mkv" > "$dir/plain.json"
python3 tests/dv7_fixture.py "$dir"
if [ "$(uname)" = Darwin ] && [ ! -d "$prefix" ]; then
  # Mac host: Homebrew FFmpeg dylibs; host proof only, not TV/native ABI proof.
  cc -std=gnu11 -Wall -Wextra -Werror -Wno-implicit-const-int-float-conversion -DNV_DTS_FFMPEG \
    ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DNV_DV7_CONVERSION -I/opt/homebrew/include \
    src/dts/dts_engine.c tests/dv7_engine.c -o "$dir/engine" -L/opt/homebrew/lib \
    -lavformat -lavcodec -lswresample -lavutil build/dv7-host/libnuvio_dv7.a -lm -lpthread
else
  cc -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DNV_DV7_CONVERSION \
    -I"$prefix/include" src/dts/dts_engine.c tests/dv7_engine.c -o "$dir/engine" \
    -L"$prefix/lib" $AV_LIBS build/dv7-host/libnuvio_dv7.a -lm -lpthread -ldl
fi
for kind in fel mel; do
  "$dir/engine" "$dir/$kind.mkv" "$dir/$kind-bad.mkv"
  NV_DV7_EXPECT_RPU="$dir/$kind-mapping-output.nal" NV_TEST_P7_MAPPING=1 \
    "$dir/engine" "$dir/$kind.mkv" "$dir/$kind-bad.mkv"
done
# FEL mapping differs from standard 8.1: the default path must not emit it.
if NV_DV7_EXPECT_RPU="$dir/fel-mapping-output.nal" "$dir/engine" "$dir/fel.mkv" "$dir/fel-bad.mkv" >/dev/null 2>&1; then
  echo "standard P7 path produced mapping-preserved RPU" >&2; exit 1
fi
echo "DV7 engine: standard and opt-in mapping RPU paths passed"
