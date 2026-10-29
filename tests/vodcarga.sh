#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/vodcarga
mkdir -p "$out/.build"
work="$out/.build"
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "$work/data.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
flags=(-Isrc -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guiav5ui.c) continue;; esac
  object="$work/$(basename "${source%.c}").o"
  if [[ ! -f "$object" || "$source" -nt "$object" ]] || [[ -n $(find src -name '*.h' -newer "$object" -print -quit) ]]; then
    cc "${flags[@]}" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
extra=()
if [[ ${VOD_BASELINE:-0} == 1 ]]; then
  git show HEAD:src/guiav5ui.c > "$work/before.c"
  extra=(-DGUIAV5_UNDER_TEST=\""$work/before.c"\")
fi
cc "${flags[@]}" ${extra[@]+"${extra[@]}"} tests/vodcarga.c "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/test"
if [[ $# -gt 0 ]]; then
  "$work/test" "$@"
else
  "$work/test" index
  "$work/test" clear
  "$work/test"
  rm -f "$NUVIO_DADOS"/guiav5-vod-*.json
  "$work/test" empty
fi
