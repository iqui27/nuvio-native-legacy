#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Modo seguro de ponta a ponta: cada fase e um processo que morre sem se despedir
# (_exit), com disco de verdade. Ver tests/seguro_ajustes.c.
#
#   bash tests/seguro_ajustes.sh
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/seguro_ajustes.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-seguro-ajustes \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
dir=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-seguro-ajustes.XXXXXX)
trap 'rm -rf "$dir"' EXIT
for fase in 1-limites 2-caiu-reverte 3-caiu-confirmada 4-limpa 5-caiu 6-caiu-perfil 7-caiu-persiste; do
  ${TMPDIR:-/tmp}/nuvio-seguro-ajustes "$dir" "$fase" > "$dir/log-$fase.txt" 2>&1 || { cat "$dir/log-$fase.txt" | tail -30; exit 1; }
  grep -h '^\[seguro\]' "$dir/log-$fase.txt" | sed "s/^/  [$fase] /"
done
echo "seguro_ajustes: ok"
