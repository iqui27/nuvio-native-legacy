#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
dir=$(mktemp -d)
trap 'rm -rf "$dir"' EXIT
NV_DV7_FIXTURE_OUT="$dir" cargo test --locked --manifest-path tools/dv7/Cargo.toml
NUVIO_DV7_ROOT="$PWD/build/dv7-host" bash tools/build-dv7.sh
audio_extra=();truehd_opts=();test_flags=()
if [ "${NV_TEST_FIXED_AUDIO:-0}" = 1 ]; then export NV_TEST_TRUEHD=1; fi
if [ "${NV_TEST_TRUEHD:-0}" = 1 ]; then
  audio_extra=(-map 1:a);truehd_opts=(-c:a:3 truehd -ac:a:3 6);test_flags=(-DNV_DV_TRUEHD_TEST)
fi
if [ "${NV_TEST_FIXED_AUDIO:-0}" = 1 ]; then
  test_flags+=(-DNV_DV_FIXED_AUDIO_TEST)
  audio_extra+=(-map 1:a -map 1:a -map 1:a)
  truehd_opts+=(-c:a:4 ac3 -ac:a:4 6 -c:a:5 ac3 -ac:a:5 2 -c:a:6 ac3 -ac:a:6 1)
fi
if [ "${NV_TEST_CACHE_REUSE:-0}" = 1 ]; then test_flags+=(-DNV_NATIVE_SEEK_TEST); fi
if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then test_flags+=(-DNV_DV7_MAPPING_TEST);fi
ffmpeg -hide_banner -loglevel error -f lavfi -i color=size=64x64:rate=25:duration=0.8 \
  -f lavfi -i sine=frequency=300:sample_rate=48000:duration=0.8 -c:v libx265 \
  -x265-params 'bframes=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
  -map 0:v -map 1:a -map 1:a -map 1:a "${audio_extra[@]}" -c:a eac3 -ac 2 -c:a:2 dca -strict -2 -b:a:2 768k \
  "${truehd_opts[@]}" "$dir/plain.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/plain.mkv" > "$dir/plain.json"
python3 tests/dv7_fixture.py "$dir"
# Real MP4 container, native P8.1 configuration and converted RPU packets.
ffmpeg -hide_banner -loglevel error -i "$dir/p8.mkv" -map 0:v:0 -map 0:a:0 -c copy \
  -strict unofficial -movflags +faststart "$dir/p8.mp4"
cc -std=gnu11 -Wall -Wextra -Werror -DNV_DTS_FFMPEG ${NV_TEST_DTS_RECOVERY_CFLAGS--DNV_DTS_WEBOS_RECOVERY} -DNV_DV7_CONVERSION "${test_flags[@]}" \
  -I"$prefix/include" src/dts/dts_engine.c tests/dv7_engine.c -o "$dir/engine" \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil \
  -Wl,--end-group build/dv7-host/libnuvio_dv7.a -lm -lpthread -ldl
for kind in fel mel;do
 extra=();if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then extra=(env "NV_DV7_EXPECT_RPU=$dir/$kind-mapping-output.nal");fi
 "${extra[@]}" "$dir/engine" "$dir/$kind.mkv" "$dir/$kind-bad.mkv" "$dir/p8.mp4"
done
