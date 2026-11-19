#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A fila de primeira vez da 1.8.0 (novidadesfila.h): instalacao nova, atualizacao
# da 1.7.x e a segunda abertura. Sem janela nem rede.
#   bash tests/novidades_fila.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
bin="$(mktemp ${TMPDIR:-/tmp}/nuvio-novidades-fila.XXXXXX)"
d1="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fila-nova.XXXXXX)"
d2="$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fila-atual.XXXXXX)"
trap 'rm -rf "$bin" "$d1" "$d2"' EXIT
cc "${sources[@]}" tests/novidades_fila.c -Isrc -o "$bin" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$d1" "$bin" nova
NUVIO_DADOS="$d1" "$bin" fechar
NUVIO_DADOS="$d1" "$bin" segunda
NUVIO_DADOS="$d2" "$bin" atualizou
NUVIO_DADOS="$d2" "$bin" fechar
NUVIO_DADOS="$d2" "$bin" segunda
echo "novidades_fila: ok"
