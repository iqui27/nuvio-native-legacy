#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Proxy de TS da Live TV (src/proxyts.c) contra o servidor Xtream falso do
# diagnostico. Precisa de python3, ffmpeg e curl.
set -eu
cd "$(dirname "$0")/.."
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${TPK:-0}" = 1 ]; then flags+=(-DNV_TPK); fi
command -v ffmpeg >/dev/null || { echo "sem ffmpeg: pulado"; exit 0; }
M=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-proxyts.XXXXXX)
mkdir -p "$M/hls"
ffmpeg -loglevel error -y -f lavfi -i testsrc2=size=1280x720:rate=25 -f lavfi -i sine -t 12 \
  -c:v libx264 -preset ultrafast -b:v 3M -c:a aac -f mpegts "$M/h264.ts"
ffmpeg -loglevel error -y -i "$M/h264.ts" -c copy -f hls -hls_time 4 -hls_list_size 0 \
  -hls_segment_filename "$M/hls/seg%d.ts" "$M/hls/media.m3u8"
cp tests/livetvdiag_servidor.py "$M/servidor.py"
python3 "$M/servidor.py" 8765 & SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$M"' EXIT
sleep 0.5
cc ${flags[@]+"${flags[@]}"} -Wall src/rede.c src/redeurl.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c tests/proxyts.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lz -o "${TMPDIR:-/tmp}/nuvio-proxyts" -Wno-deprecated-declarations
"${TMPDIR:-/tmp}/nuvio-proxyts"
