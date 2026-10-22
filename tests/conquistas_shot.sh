#!/bin/bash
# Capturas e regras da tela Conquistas (2.1) em PNG 1920x1080: hub, aba, familias,
# detalhe, aviso depois dos creditos e o botao Conquistas do Perfil. Antes, as
# regras por evento de tecla (layout, varredura D-pad, Voltar, aviso sem tomar o
# foco, Perfil). Janela GL ESCONDIDA e desenho num FBO. Fica fora da suite
# (*_shot.sh): precisa de GL.
#
#   bash tests/conquistas_shot.sh tests/out/conquistas/conquistas
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-conquistas-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
BIN="${NUVIO_SHOT_BIN:-/tmp/nuvio-conquistas-shot}"
cc -DAJUSTES_TESTE "${sources[@]}" tests/conquistas_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
out="${1:-/tmp/nuvio-conquistas}"
mkdir -p "$(dirname "$out")"
"$BIN" "$out"
