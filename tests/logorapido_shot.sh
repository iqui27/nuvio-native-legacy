#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# bash tests/logorapido_shot.sh <dados-isolados> [sem-logo]
set -eu
cd "$(dirname "$0")/.."
: "${1:?pasta de dados isolada obrigatoria}"
mkdir -p "$1"
OBJ=${OBJ:-${TMPDIR:-/tmp}/nuvio-logorapido-obj}
mkdir -p "$OBJ"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objs=()
pids=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c) continue;; esac
  o="$OBJ/$(basename "$s" .c).o"
  objs+=("$o")
  if [ ! -f "$o" ] || [ "$s" -nt "$o" ] || [ -n "$(find src -name '*.h' -newer "$o" -print -quit)" ]; then
    cc "${flags[@]}" -c "$s" -o "$o" & pids+=($!)
    if [ ${#pids[@]} -ge 8 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
  fi
done
for p in ${pids[@]+"${pids[@]}"}; do wait "$p"; done
cc "${flags[@]}" tests/logorapido_shot.c "${objs[@]}" -o "$OBJ/shot" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$OBJ/shot" "$@"
