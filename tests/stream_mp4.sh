#!/bin/bash
# "E MP4?" tem uma resposta so: stream_e_mp4 (streams.c), a mesma do cartao da
# fonte e dos tres pontos do app.c que anunciam o contentor ao video (o que
# decide se a tela/sonda do Dolby Vision em MKV entra). Ver tests/stream_mp4.c.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-stream-mp4.XXXXXX")
trap 'rm -rf "$dir"' EXIT
# Contrato do app.c: todo video_definir_mp4 passa pelo predicado unico.
total=$(grep -c 'video_definir_mp4(' src/app.c || true)
bons=$(grep -c 'video_definir_mp4(stream_e_mp4(' src/app.c || true)
if [ "$total" -lt 3 ] || [ "$total" != "$bons" ]; then
  echo "FALHOU: app.c tem $total video_definir_mp4, so $bons usam stream_e_mp4"
  grep -n 'video_definir_mp4(' src/app.c
  exit 1
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/stream_mp4.c -Isrc -o "$dir/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$dir/teste"
