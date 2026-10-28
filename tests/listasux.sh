#!/bin/bash
# One native object build; run Lists and existing Library regressions against it.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-listasux.XXXXXX")
trap 'rm -rf "$work"' EXIT
obj="${TMPDIR:-/tmp}/nuvio-listasux-objects"
mkdir -p "$obj"
export CCACHE_DIR="$obj/ccache"
export CCACHE_TEMPDIR="$obj/ccache-tmp"
mkdir -p "$CCACHE_TEMPDIR"
flags=(-O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined -DNV_TRAKT_CLIENT_ID='"chave-de-teste"')
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
# These headers are the ABI/data dependencies changed by this patch.
headers=$(shasum src/listas.h src/caca.h src/idioma_tab.h | shasum | cut -d ' ' -f 1)
previous=$(cat "$obj/headers.sha" 2>/dev/null || true)
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  leaf="${source##*/}"; object="$obj/${leaf%.c}.o"
  objects+=("$object")
  if [ ! -f "$object" ] || [ "$source" -nt "$object" ] || { [ "$headers" != "$previous" ] && [[ "$source" = src/listas.c || "$source" = src/caca.c || "$source" = src/biblioteca.c || "$source" = src/idioma.c ]]; }; then
    ccache cc "${flags[@]}" -c "$source" -o "$object"
  fi
done
echo "$headers" > "$obj/headers.sha"
# Capture edits made while the initial object pass was running.
for source in src/biblioteca.c src/listas.c; do
  leaf="${source##*/}"; object="$obj/${leaf%.c}.o"
  if [ "$source" -nt "$object" ]; then ccache cc "${flags[@]}" -c "$source" -o "$object"; fi
done
run_test() {
  local name="$1" exclude="${2:-}" selected=()
  for object in "${objects[@]}"; do
    leaf="${object##*/}"
    [ "${leaf%.o}" = "$exclude" ] || selected+=("$object")
  done
  cc "${flags[@]}" "tests/$name.c" "${selected[@]}" "${libs[@]}" -o "$work/$name"
  mkdir -p "$work/data-$name"
  NUVIO_DADOS="$work/data-$name" "$work/$name"
}
run_test listasux_parser listas
run_test listasux biblioteca
run_test listas
run_test listas_rede listas
run_test biblioteca_up biblioteca
run_test biblioteca_grade_escala biblioteca
bash tests/biblioteca_previa.sh
bash tests/listas_wasm.sh
python3 tests/listas_auth.py
python3 tests/listas_picker.py
python3 tests/listas_adicionar.py
if [ "${1:-}" = shots ]; then
  selected=()
  for object in "${objects[@]}"; do
    case "${object##*/}" in biblioteca.o|caca.o) continue;; esac
    selected+=("$object")
  done
  cc "${flags[@]}" tests/listasux_shot.c tests/listasux_trails.c "${selected[@]}" "${libs[@]}" -o "$work/shot"
  mkdir -p "$work/data-shot"
  output="${2:-/Volumes/ExternalSSD/tmp/rev/listasux}"
  mkdir -p "$output"
  NUVIO_DADOS="$work/data-shot" "$work/shot" "$output"
  git --no-pager show "${BIB_REV:-1e01e0ef2cf383b3f2d9fbf39fdde6de705416fe}:src/biblioteca.c" > "$work/biblioteca-before.c"
  cc "${flags[@]}" -DBIB_SOURCE="\"$work/biblioteca-before.c\"" -DSHOT_PHASE='"before"' -DLISTASUX_BEFORE \
    tests/listasux_shot.c tests/listasux_trails.c "${selected[@]}" "${libs[@]}" -o "$work/before"
  mkdir -p "$work/data-before"
  NUVIO_DADOS="$work/data-before" "$work/before" "$output"
  python3 tests/listasux_compare.py "$output"
fi
# Link the application once, using the same verified objects.
cc "${flags[@]}" src/main.c "${objects[@]}" "${libs[@]}" -o "$obj/nuvio"
