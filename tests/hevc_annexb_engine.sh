#!/usr/bin/env bash
# Reproduces the LG HDR10/DV8 + DTS picture fault on the real engine path:
# a film whose middle part carries a different PPS 0 than the container's
# hvcC. FFmpeg's hevc_mp4toannexb puts the old hvcC PPS 0 after the stream's
# own one, so that part decodes wrong; the engine's conversion must decode
# exactly like the original stream. Needs NUVIO_DTS_ROOT (pinned FFmpeg
# libraries) and an ffmpeg CLI with libx265, dca encoder and HEVC decoder.
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -DNV_HTTP_BUFFER_DIAG -DNV_PICTURE_DIAG -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
x265='pools=1:frame-threads=1:log-level=error:keyint=24:min-keyint=24:scenecut=0:repeat-headers=1:hdr10=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc'
part() { # $1 name, $2 source, $3 extra x265 params (PPS-only differences)
  ffmpeg -hide_banner -loglevel error -f lavfi -i "$2" -t 3 -c:v libx265 -pix_fmt yuv420p10le \
    -x265-params "$x265$3" "$tmp/$1.mkv"
  # One part alone: its hvcC equals its own sets, so this is its true stream.
  ffmpeg -hide_banner -loglevel error -i "$tmp/$1.mkv" -c copy -bsf:v hevc_mp4toannexb -f hevc "$tmp/$1.hevc"
  echo "file '$tmp/$1.mkv'" >> "$tmp/parts.txt"
}
part a 'testsrc2=size=192x108:rate=24' ''
part b 'mandelbrot=size=192x108:rate=24' ':cbqpoffs=6:crqpoffs=-6:no-sign-hide=1'
part c 'testsrc2=size=192x108:rate=24' ''
cat "$tmp/a.hevc" "$tmp/b.hevc" "$tmp/c.hevc" > "$tmp/film.hevc"
# The container keeps the first part's hvcC; later parts carry their own sets.
ffmpeg -hide_banner -loglevel error -f concat -safe 0 -i "$tmp/parts.txt" \
  -f lavfi -i 'sine=frequency=500:duration=9:sample_rate=48000' \
  -map 0:v -map 1 -c:v copy -c:a dca -strict -2 -b:a 768k "$tmp/film.mkv"
cc "${flags[@]}" src/dts/dts_engine.c tests/hevc_annexb_engine.c \
  -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group \
  -lm -pthread -o "$tmp/engine"
"$tmp/engine" "$tmp/film.mkv" "$tmp/engine.hevc" > "$tmp/engine.log" 2>&1 || { cat "$tmp/engine.log";exit 1; }
ffmpeg -hide_banner -loglevel error -i "$tmp/film.mkv" -map 0:v -c copy -bsf:v hevc_mp4toannexb -f hevc "$tmp/stock.hevc"
md5() { ffmpeg -hide_banner -loglevel quiet -f hevc -i "$1" -f framemd5 - | grep -v '^#' | awk -F, '{print $NF}'; }
md5 "$tmp/film.hevc" > "$tmp/ref.md5"
md5 "$tmp/engine.hevc" > "$tmp/engine.md5"
md5 "$tmp/stock.hevc" > "$tmp/stock.md5"
frames=$(wc -l < "$tmp/ref.md5")
[[ $frames == 216 ]] || { echo "unexpected frame count $frames";exit 1; }
cmp "$tmp/ref.md5" "$tmp/engine.md5" || { echo 'FAIL engine output decodes differently from the original stream';exit 1; }
bad=$(paste -d' ' "$tmp/ref.md5" "$tmp/stock.md5" | awk '$1!=$2' | wc -l)
# The fault must reproduce with FFmpeg's filter, or this fixture proves nothing.
(( bad >= 48 )) || { echo "fixture did not reproduce the PPS fault (bad=$bad)";exit 1; }
grep -q '^\[hevc-annexb\] converter=own lengthSize=4' "$tmp/engine.log"
grep -q '^\[hevc-annexb\] mode=inband' "$tmp/engine.log"
grep -q '^\[fixture\] video=216$' "$tmp/engine.log"
echo "PASS PPS change mid-film: engine Annex-B decodes like the original ($frames/$frames frames); FFmpeg's filter breaks $bad frames"
