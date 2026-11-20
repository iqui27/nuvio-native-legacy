#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Diagnostico da Live TV contra um servidor Xtream falso + capturas. Precisa de
# python3 e ffmpeg (gera os videos de teste). Ver tests/livetvdiag_shot.c.
#   bash tests/livetvdiag_shot.sh /tmp/nuvio-ltd
set -eu
cd "$(dirname "$0")/.."
command -v ffmpeg >/dev/null || { echo "sem ffmpeg: pulado"; exit 0; }
M=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ltd-midia.XXXXXX)
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-ltd-shot.XXXXXX)
export NUVIO_DADOS
mkdir -p "$M/hls"
ffmpeg -loglevel error -y -f lavfi -i testsrc2=size=1920x1080:rate=25 -f lavfi -i sine -t 12 \
  -c:v libx264 -preset ultrafast -b:v 5M -c:a aac -f mpegts "$M/h264.ts"
ffmpeg -loglevel error -y -f lavfi -i testsrc2=size=1920x1080:rate=25 -f lavfi -i sine -t 6 \
  -c:v libx265 -preset ultrafast -pix_fmt yuv420p10le -x265-params log-level=error -c:a ac3 -f mpegts "$M/hevc10.ts"
ffmpeg -loglevel error -y -i "$M/h264.ts" -c copy -f hls -hls_time 4 -hls_list_size 0 \
  -hls_segment_filename "$M/hls/seg%d.ts" "$M/hls/media.m3u8"
cp tests/livetvdiag_servidor.py "$M/servidor.py"
python3 "$M/servidor.py" 8765 & SRV=$!
trap 'kill $SRV 2>/dev/null; rm -rf "$M" "$NUVIO_DADOS"' EXIT
sleep 0.5
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/livetvdiag.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/livetvdiag_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-livetvdiag-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-livetvdiag-shot "$@"
