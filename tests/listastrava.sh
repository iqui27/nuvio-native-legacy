#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/listastrava.XXXXXX")
trap 'rm -rf "$work"' EXIT
obj="${TMPDIR:-/tmp}/nuvio-listasux-objects"
mkdir -p "$obj" "$work/data"
export CCACHE_DIR="$obj/ccache" CCACHE_TEMPDIR="$obj/ccache-tmp"
mkdir -p "$CCACHE_TEMPDIR"
flags=(-O2 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined -DNV_TRAKT_CLIENT_ID='"chave-de-teste"')
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/biblioteca.c|src/caca.c) continue;; esac
  leaf="${source##*/}"; object="$obj/${leaf%.c}.o"
  objects+=("$object")
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ]; then ccache cc "${flags[@]}" -c "$source" -o "$object"; fi
done
extra=()
if [ -n "${BIB_REV:-}" ]; then
  git show "$BIB_REV:src/biblioteca.c" > "$work/before.c"
  extra+=("-DBIB_SOURCE=\"$work/before.c\"" '-DSHOT_PHASE="before"')
fi
cc "${flags[@]}" "${extra[@]}" tests/listastrava.c tests/listasux_trails.c "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/test"
output=/Volumes/ExternalSSD/tmp/rev/listastrava
mkdir -p "$output"
NUVIO_DADOS="$work/data" "$work/test" "$output"
