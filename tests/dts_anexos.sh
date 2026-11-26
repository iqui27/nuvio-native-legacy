#!/bin/bash
# Demux opens skip Matroska attachments (owner 18/11, log 13: a donor's 25 MB
# of fonts took the whole 10 s deadline): over HTTP from the test origin, an
# MKV with 8 MB of attachments opens with its tracks only, downloads less than
# the attachments and reads every packet, as the same MKV without them does.
# Needs FFmpeg for the .tpk in NUVIO_DTS_ROOT (see tests/dts_tpk_servidor.sh).
set -eu
cd "$(dirname "$0")/.."
ROOT=${NUVIO_DTS_ROOT:-/opt/nuvio-dts}
if [ ! -f "$ROOT/lib/libavformat.a" ] || ! command -v ffmpeg >/dev/null; then
  echo "dts_anexos: skipped (needs FFmpeg for the .tpk in $ROOT and the ffmpeg CLI)"
  exit 0
fi
work=$(mktemp -d /tmp/nuvio-dts-anexos.XXXXXX)
pid=''
trap 'if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || true; fi; rm -rf "$work"' EXIT
mkdir -p "$work/src"
head -c 8000000 /dev/urandom > "$work/f1.ttf"
head -c 300000 /dev/urandom > "$work/f2.otf"
ffmpeg -v error -f lavfi -i testsrc2=s=320x180:r=24 -f lavfi -i "sine=f=440:r=48000" -t 20 \
  -map 0:v -map 1:a -c:v libx264 -preset ultrafast -g 48 -pix_fmt yuv420p -c:a aac -ac 2 "$work/src/sem.mkv"
ffmpeg -v error -i "$work/src/sem.mkv" -map 0 -c copy \
  -attach "$work/f1.ttf" -metadata:s:t:0 mimetype=application/x-truetype-font \
  -attach "$work/f2.otf" -metadata:s:t:1 mimetype=application/vnd.ms-opentype "$work/src/com.mkv"
ffmpeg -v error -i "$work/src/sem.mkv" -map 0 -c copy "$work/src/sem.mp4"
python3 tests/dts_tpk_servidor_origem.py "$work/src" "$work/port" "$work/origem.log" &
pid=$!
for _ in $(seq 50); do [ -s "$work/port" ] && break; sleep 0.1; done
port=$(cat "$work/port")
${CC:-gcc} -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-misleading-indentation \
  -DNV_TPK -DNV_DTS_FFMPEG -I"$ROOT/include" -Isrc \
  tests/dts_anexos.c src/dts/dts_engine.c src/rede.c src/redeurl.c \
  "$ROOT/lib/libavformat.a" "$ROOT/lib/libavcodec.a" "$ROOT/lib/libswresample.a" "$ROOT/lib/libavutil.a" \
  -lz -lm -lpthread -ldl -o "$work/test"
fail=0
run() {
  local out
  if out=$("$work/test" "http://127.0.0.1:$port/$1" "$2" 2 2>&1); then echo "ok   $1: $(echo "$out" | grep -v '^\[' | tr '\n' ' ')"
  else echo "FAIL $1:"; echo "$out" | sed 's/^/     /'; fail=1; fi
  echo "$out" | grep '^\[dts-engine\] attachments' | sed 's/^/     /' || true
}
run com.mkv 8300000
run sem.mkv 0
run sem.mp4 0
a=$("$work/test" "http://127.0.0.1:$port/com.mkv" 8300000 2 | grep -o '^[0-9]* packets')
b=$("$work/test" "http://127.0.0.1:$port/sem.mkv" 0 2 | grep -o '^[0-9]* packets')
if [ "$a" = "$b" ]; then echo "ok   the same packets with and without the attachments ($a)"; else echo "FAIL packets: $a vs $b"; fail=1; fi
[ "$fail" = 0 ] && echo "dts_anexos: ok" || { echo "dts_anexos: FAILED"; exit 1; }
