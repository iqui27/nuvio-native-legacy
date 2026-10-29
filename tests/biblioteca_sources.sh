#!/bin/bash
# Native screen, real handlers, full animation frames, isolated offline data.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/bibfonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
obj="${TMPDIR:-/tmp}/nuvio-listasux-objects"
mkdir -p "$obj"
export CCACHE_DIR="$obj/ccache" CCACHE_TEMPDIR="$obj/ccache-tmp"
mkdir -p "$CCACHE_TEMPDIR"
flags=(-O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined -DNV_TRAKT_CLIENT_ID='"chave-de-teste"')
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/biblioteca.c|src/caca.c) continue;; esac
  leaf="${source##*/}"; object="$obj/${leaf%.c}.o"
  objects+=("$object")
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then ccache cc "${flags[@]}" -c "$source" -o "$object"; fi
done
extra=()
phase=after
if [ -n "${BIB_REV:-}" ]; then
  git show "$BIB_REV:src/biblioteca.c" > "$work/biblioteca.c"
  extra+=("-DBIB_SOURCE=\"$work/biblioteca.c\"")
  phase=before
fi
cc "${flags[@]}" "${extra[@]}" -DSHOT_PHASE="\"$phase\"" tests/biblioteca_sources.c tests/listasux_trails.c "${objects[@]}" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/test"
output="${1:-/Volumes/ExternalSSD/tmp/rev/bibfonte}"
mkdir -p "$output" "$work/data"
NUVIO_DADOS="$work/data" "$work/test" "$output"
