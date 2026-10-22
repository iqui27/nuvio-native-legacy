#!/bin/bash
# O contentor que a sonda do MKV leu (mkv.h, MKV_CONT_*): classificador puro e a
# sonda real contra MKV/MP4/TS gerados pelo ffmpeg num servidor local.
set -euo pipefail
cd "$(dirname "$0")/.."
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || { echo "mkv_contentor.sh: ffmpeg nao encontrado em $FFMPEG"; exit 1; }
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvcont.XXXXXX")
pid=""
trap '[ -n "$pid" ] && kill "$pid" 2>/dev/null; rm -rf "$DIR"' EXIT
mkdir -p "$DIR/www"
for ext in mkv mp4 ts; do
  "$FFMPEG" -v error -y -f lavfi -i "testsrc2=size=160x90:rate=24:duration=1" -f lavfi -i "sine=duration=1" \
    -c:v libx264 -pix_fmt yuv420p -c:a aac -shortest "$DIR/www/a.$ext"
done
porta=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
morta=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1])')
python3 -m http.server "$porta" --bind 127.0.0.1 --directory "$DIR/www" >/dev/null 2>&1 &
pid=$!
for _ in $(seq 50); do curl -sf -o /dev/null "http://127.0.0.1:$porta/a.ts" && break; sleep 0.1; done
cc -Isrc tests/mkv_contentor.c src/mkv.c src/rede.c src/redeurl.c \
  -o "$DIR/t" -O1 -g -Wall -I/opt/homebrew/include -Wno-deprecated-declarations
"$DIR/t" "http://127.0.0.1:$porta" "http://127.0.0.1:$morta"
