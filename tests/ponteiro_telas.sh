#!/bin/bash
# Ponteiro do Magic Remote NAS TELAS (tests/ponteiro_telas.c): cada tela
# registra alvos, passar por cima foca e clicar ativa. Eventos SDL de mouse
# passam por ponteiro_evento como no aparelho. Janela GL escondida do Mac;
# sem rede.
#
#   bash tests/ponteiro_telas.sh [pasta-das-capturas]
set -euo pipefail
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ponteiro-telas.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
if [ $# -gt 0 ]; then mkdir -p "$1"; fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
if ! cc "${sources[@]}" tests/ponteiro_telas.c -Isrc -o "$NUVIO_DADOS/teste" \
  -O1 -g -DNV_SHOT_HOOKS -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  >"$NUVIO_DADOS/build.log" 2>&1; then
  cat "$NUVIO_DADOS/build.log" >&2
  exit 1
fi
"$NUVIO_DADOS/teste" "$@"
