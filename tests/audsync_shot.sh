#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da opcao "Por audio" (F06) na linha de AutoSync. Nao entra na suite
# (*_shot.sh): janela GL. Legenda externa por HTTP local; PCM sintetico.
#   bash tests/audsync_shot.sh /Volumes/ExternalSSD/nv-f06-shots/audsync
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do [ "$source" != src/main.c ] && sources+=("$source"); done
cc "${sources[@]}" tests/audsync_shot.c -Isrc -Itests -o ${TMPDIR:-/tmp}/nuvio-audsync-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
D=$(mktemp -d); W=$(mktemp -d)
# A mesma timeline do teste (semente 99), servida como SRT.
cat > "$W/gera.c" <<'C'
#include "audsync_sint.h"
int main(void) { sint_timeline(0, 900, 99); fputs(sint_srt(), stdout); return 0; }
C
cc -Itests "$W/gera.c" -o "$W/gera" -lm && "$W/gera" > "$W/falas.srt"
P=$((20000 + RANDOM % 20000))
python3 -m http.server $P --bind 127.0.0.1 --directory "$W" >/dev/null 2>&1 & S=$!
trap 'kill $S 2>/dev/null || true; rm -rf "$D" "$W"' EXIT
for i in $(seq 50); do nc -z 127.0.0.1 $P 2>/dev/null && break; sleep 0.1; done
mkdir -p "$(dirname "${1:-${TMPDIR:-/tmp}/nv-audsync}")"
NV_AUDSYNC_BASE="http://127.0.0.1:$P" NUVIO_DADOS="$D" NUVIO_TESTE_DIR="$D" ${TMPDIR:-/tmp}/nuvio-audsync-shot "$@"
