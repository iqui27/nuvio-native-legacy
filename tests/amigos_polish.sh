#!/bin/bash
# Friends row polish (#205): no "See all"/"+ Add" pills in styles 0..4, the standard home
# focus ring on the focused card, and the initial-letter fallback for a missing avatar.
# NV_POLISH_REV=<rev> builds src/ from that revision (FAIL proof against the old code).
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-polishw.XXXXXX")
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-abas-shot.XXXXXX")
trap 'rm -rf "$work" "$NUVIO_DADOS"' EXIT
src=src
if [ -n "${NV_POLISH_REV:-}" ]; then
  mkdir "$work/tree"; git archive "$NV_POLISH_REV" src deploy 2>/dev/null | tar -x -C "$work/tree"
  cp -R tests "$work/tree/tests"; src="$work/tree/src"; cd "$work/tree"
fi
sources=()
for source in $src/*.c $src/dts/*.c; do
  case "$source" in */main.c|*/recomenda.c|*/rede.c|*/celular.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/amigos_estilos_shot.c -I"$src" -o "$work/polish" -O1 -g -DNV_SOCIALVIS_DEMO -DNV_AMIGOSFIL_TESTE -DNV_AMIGOS_POLISH \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined >"$work/build.log" 2>&1 || { tail -40 "$work/build.log"; exit 2; }
"$work/polish" "$work/s" >"$work/test.log" 2>&1 || { tail -15 "$work/test.log"; exit 1; }
echo "amigos polish: ok"
