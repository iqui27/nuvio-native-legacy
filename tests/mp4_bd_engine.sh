#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
dir=${NV_MP4_FIXTURE_DIR:-$(mktemp -d)}
mkdir -p "$dir"
if [[ -z ${NV_MP4_FIXTURE_DIR:-} ]];then trap 'rm -rf "$dir"' EXIT;fi
NV_DV7_FIXTURE_OUT="$dir" cargo test --locked --manifest-path tools/dv7/Cargo.toml
NUVIO_DV7_ROOT="$PWD/build/dv7-host" bash tools/build-dv7.sh
ffmpeg -y -hide_banner -loglevel error -f lavfi -i color=size=64x64:rate=25:duration=0.8 \
 -f lavfi -i sine=frequency=300:sample_rate=48000:duration=0.8 -c:v libx265 \
 -x265-params 'bframes=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
 -map 0:v -map 1:a -map 1:a -map 1:a -map 1:a -map 1:a -map 1:a -map 1:a \
 -c:a eac3 -ac 2 -c:a:2 dca -strict -2 -b:a:2 768k -c:a:3 truehd -ac:a:3 6 \
 -c:a:4 ac3 -ac:a:4 6 -c:a:5 ac3 -ac:a:5 2 -c:a:6 ac3 -ac:a:6 1 "$dir/plain.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/plain.mkv" > "$dir/plain.json"
python3 tests/dv7_fixture.py "$dir"
ffmpeg -y -hide_banner -loglevel error -i "$dir/p8.mkv" -map 0:v:0 -map 0:a:0 -c copy -strict unofficial -movflags +faststart "$dir/p8.mp4"
ffmpeg -y -hide_banner -loglevel error \
 -f lavfi -i color=size=64x64:rate=25:duration=0.8 \
 -f lavfi -i color=size=32x32:rate=25:duration=0.8 \
 -f lavfi -i sine=frequency=300:sample_rate=48000:duration=0.8 \
 -map 0:v -map 1:v -map 2:a -map 2:a -c:v libx265 -pix_fmt yuv420p10le \
 -x265-params 'bframes=2:b-adapt=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
 -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc -c:a ac3 -ac 2 -b:a 192k "$dir/split.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/split.mkv" > "$dir/split.json"
ffmpeg -y -hide_banner -loglevel error \
 -f lavfi -i color=size=64x64:rate=25:duration=8 \
 -f lavfi -i color=size=32x32:rate=25:duration=8 \
 -f lavfi -i sine=frequency=300:sample_rate=48000:duration=8 \
 -map 0:v -map 1:v -map 2:a -map 2:a -c:v libx265 -pix_fmt yuv420p10le \
 -x265-params 'bframes=2:b-adapt=0:pools=1:frame-threads=1:log-level=error:keyint=25:min-keyint=25:scenecut=0' \
 -color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc -c:a ac3 -ac 2 -b:a 192k "$dir/split-long.mkv"
ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/split-long.mkv" > "$dir/split-long.json"
for colour in sdr hlg;do
 primaries=bt2020;transfer=arib-std-b67;matrix=bt2020nc
 if [[ $colour == sdr ]];then primaries=bt709;transfer=bt709;matrix=bt709;fi
 ffmpeg -y -hide_banner -loglevel error -i "$dir/split.mkv" -map 0 -c copy \
  -c:v libx265 -pix_fmt yuv420p10le \
  -x265-params 'bframes=2:b-adapt=0:pools=1:frame-threads=1:log-level=error:keyint=10' \
  -color_primaries "$primaries" -color_trc "$transfer" -colorspace "$matrix" "$dir/split-$colour.mkv"
 ffprobe -v error -show_streams -show_packets -show_data -of json "$dir/split-$colour.mkv" > "$dir/split-$colour.json"
done
python3 tests/mp4_bd_fixture.py "$dir"
flags=(-DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -DNV_DV7_CONVERSION -DNV_MP4_BD_TEST -DNV_DV_FIXED_AUDIO_TEST -DNV_DV_TRUEHD_TEST)
if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then flags+=(-DNV_DV7_MAPPING_TEST);fi
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror "${flags[@]}" -I"$prefix/include" \
 src/dts/dts_engine.c tests/mp4_bd_engine.c -o "$dir/engine" -L"$prefix/lib" \
 -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
 build/dv7-host/libnuvio_dv7.a -lm -lpthread -ldl
for kind in mel fel;do
 rpu="$dir/$kind-output.nal";extra=()
 if [[ ${NV_TEST_P7_MAPPING:-0} == 1 ]];then rpu="$dir/$kind-mapping-output.nal";extra=(env "NV_DV7_EXPECT_RPU=$rpu");fi
 "${extra[@]}" "$dir/engine" "$dir/$kind.mkv" "$dir/$kind-bad.mkv" "$dir/p8.mp4" \
  "$dir/$kind-dual.mp4" "$dir/$kind-bad-rpu.mp4" "$dir/$kind-mismatch.mp4" \
  "$dir/$kind-missing-rpu.mp4" "$dir/$kind-dual-reverse.mp4" "$rpu" "$dir/$kind-inband.mp4" "$dir/output.h265" \
  "$dir/$kind-inband-nocolr.mp4" "$dir/$kind-hvcc-nocolr.mp4" "$dir/$kind-inband-nosps.mp4" "$dir/$kind-sdr-colr.mp4" "$dir/$kind-inband-conflict.mp4"
 ffmpeg -y -hide_banner -loglevel error -i "$dir/output.h265" -f null -
done

for test in mp4_p8_engine mp4_seek_preroll;do
 extra=();if [[ $test == mp4_p8_engine ]];then extra+=(-Wl,--wrap=nv_dv7_rpu -Wl,--wrap=nv_dv7_rpu_mapping -Wl,--wrap=nv_dv7_rpu_mapping_report);fi
 cc -std=gnu11 -O1 -g -Wall -Wextra -Werror "${flags[@]}" -I"$prefix/include" \
  src/dts/dts_engine.c "tests/$test.c" -o "$dir/$test" -L"$prefix/lib" \
  -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
  build/dv7-host/libnuvio_dv7.a -lm -lpthread -ldl "${extra[@]}"
 for kind in mel fel;do
  if [[ $test == mp4_p8_engine ]];then
   "$dir/$test" "$dir" "$kind" "$dir/$kind-p8-output.h265"
   ffmpeg -y -hide_banner -loglevel error -i "$dir/$kind-p81.mp4" -map 0:v:0 -f framemd5 "$dir/$kind-before.md5"
   ffmpeg -y -hide_banner -loglevel error -i "$dir/$kind-p8-output.h265" -f framemd5 "$dir/$kind-after.md5"
   python3 - "$dir/$kind-before.md5" "$dir/$kind-after.md5" <<'CHECK'
import pathlib, sys
hashes=lambda p:[line.split(',')[-1].strip() for line in pathlib.Path(p).read_text().splitlines() if not line.startswith('#')]
a,b=map(hashes,sys.argv[1:]);assert len(a)==20 and a==b
print('PASS P8 output: identical decoded picture hashes')
CHECK
  else
   "$dir/$test" "$dir/$kind-seek-preroll.mp4" "$dir/$kind-output.nal" "$dir/$kind-preroll-output.h265"
   ffmpeg -hide_banner -loglevel error -i "$dir/$kind-preroll-output.h265" -f null -
  fi
 done
done
