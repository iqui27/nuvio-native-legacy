#!/bin/bash
# Migracao unica da Memoria para imagens (2.0.5): highcache leva a 300 MB, normal nao.
#
#   bash tests/ajustes_memoria_migra.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
falhou=0
dispositivo=$(mktemp /tmp/nuvio-memoria-dispositivo.XXXXXX)
trap 'rm -f "$dispositivo"' EXIT
for variante in normal highcache; do
  flags=""
  [ "$variante" = highcache ] && flags="-DNV_TEX_MB_FIXO=300"
  cc "${sources[@]}" tests/ajustes_memoria_migra.c -Isrc -o "/tmp/nuvio-ajustes-memoria-$variante" \
    -O1 -g $flags -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
  echo "[$variante]"
  "/tmp/nuvio-ajustes-memoria-$variante" "$dispositivo" || falhou=1
done
/tmp/nuvio-ajustes-memoria-normal --reload "$(< "$dispositivo")" || falhou=1
exit "$falhou"
