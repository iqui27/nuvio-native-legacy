#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas nativas de Ajustes com fixtures locais, sem conta nem rede.
# Precisa de janela GL; inspecao visual no host nao valida uma TV fisica.
#
#   NUVIO_AJUSTES_UX=1 bash tests/ajustes_shot.sh /tmp/nuvio-ajustes
#   NUVIO_RAIL=fixa bash tests/ajustes_shot.sh /tmp/nuvio-ajustes-fixa
#
# #427: cenario 6 com pedido salvo de 512 MB numa TV de 1704 MB (teto de 160 MB):
#   NUVIO_AJUSTES_UX=1 NUVIO_SHOT_RAM_MB=1704 NUVIO_SHOT_TEX_MB=6 bash tests/ajustes_shot.sh /tmp/nuvio-427
# Acrescente NUVIO_SHOT_TEX_EDITOR=1 para capturar as escolhas indisponiveis.
# Salva 12 cenas da UX (incluindo Trailers no enquadramento do mockup).
# Sem NUVIO_AJUSTES_UX, acrescenta uma captura para cada categoria.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/ajustes_shot.c -Isrc -o "${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-ajustes-shot}" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"${NUVIO_SHOT_BIN:-${TMPDIR:-/tmp}/nuvio-ajustes-shot}" "$@"
