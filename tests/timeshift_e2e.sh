#!/bin/bash
# Timeshift local de ponta a ponta no Linux: HLS ao vivo do ffmpeg (playlist
# deslizante) -> proxy de TS -> gravacao em disco -> leitura atrasada.
# Precisa de python3, ffmpeg, ffprobe e curl. NUVIO_TS_DIR=/pendrive/pasta
# grava num volume de verdade.
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
set -o pipefail
cd "$(dirname "$0")/.."
command -v ffmpeg >/dev/null && command -v ffprobe >/dev/null || { echo "sem ffmpeg: pulado"; exit 0; }
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
M=$(mktemp -d "$TMPDIR/nuvio-timeshift-e2e.XXXXXX")
PORTA=8791
mkdir -p "$M/hls"
ffmpeg -loglevel error -re -f lavfi -i testsrc2=size=1280x720:rate=25 -f lavfi -i sine \
  -c:v libx264 -preset ultrafast -b:v 3M -g 50 -c:a aac -f hls -hls_time 2 -hls_list_size 5 \
  -hls_flags delete_segments -hls_segment_filename "$M/hls/seg%d.ts" "$M/hls/canal.m3u8" & FF=$!
(cd "$M/hls" && exec python3 -m http.server $PORTA --bind 127.0.0.1 >/dev/null 2>&1) & SRV=$!
trap 'kill $FF $SRV 2>/dev/null; rm -rf "$M"' EXIT
cc ${flags[@]+"${flags[@]}"} -w src/rede.c src/redeurl.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c tests/timeshift_e2e.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lz -lpthread -o "$TMPDIR/nuvio-timeshift-e2e"
for i in $(seq 1 40); do [ -s "$M/hls/canal.m3u8" ] && break; sleep 0.5; done
"$TMPDIR/nuvio-timeshift-e2e" "http://127.0.0.1:$PORTA/canal.m3u8" "${NUVIO_TS_DIR:-$M/disco}" "$M" | grep -v '^\[proxy-ts\] segmento'
