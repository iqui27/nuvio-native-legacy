#!/bin/bash
# Glass budget per screen on the webOS renderer (gfx.c with -DNV_WEBOS), offline:
# details with the sources sheet, details Trakt comments row, Search, Explore,
# Settings. Each: 0 captures/frame static, at most 1 while moving focus, and
# a per-screen fill / text-raster budget (vidro_telas.h).
#   bash tests/vidro_telas.sh [png-dir]
# NUVIO_VIDRO_BASELINE=<rev>: same harness against that revision's gfx.c.
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/vidrotelas/depois}"
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vidro-telas.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -DNV_SHOT_HOOKS -DAJUSTES_TESTE -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -Isrc
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
objects=()
pids=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/gfx.c|src/detail.c) continue;; esac
  object="$work/$(basename "$source" .c).o"
  cc "${flags[@]}" -c "$source" -o "$object" & pids+=($!)
  objects+=("$object")
  if [ ${#pids[@]} -ge 10 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
gfx_source=src/gfx.c
if [ -n "${NUVIO_VIDRO_BASELINE:-}" ]; then
  git show "$NUVIO_VIDRO_BASELINE:src/gfx.c" > "$work/gfx.c"
  gfx_source="$work/gfx.c"
fi
cc "${flags[@]}" -DNV_WEBOS -c "$gfx_source" -o "$work/gfx.o" & pids+=($!)
cc "${flags[@]}" -c src/detail.c -o "$work/detail.o" & pids+=($!)
for p in "${pids[@]}"; do wait "$p"; done
status=0
for shot in detalhe_glass busca explorar ajustes; do
  extra=("$work/detail.o")
  [ "$shot" = detalhe_glass ] && extra=()
  cc "${flags[@]}" "tests/${shot}_shot.c" "${objects[@]}" "${extra[@]}" "$work/gfx.o" "${libs[@]}" -o "$work/$shot"
  data=$(mktemp -d "$work/nuvio-detglass-dados.XXXXXX")
  NUVIO_DADOS="$data" NUVIO_TESTE_DIR="$data" NUVIO_VIDRO_TELAS="$out" \
    sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/$shot" "$work/$shot-out" \
    > "$work/$shot.log" || status=1
  grep -E '^(vidro_telas|PASS|FAIL)' "$work/$shot.log" || status=1
  grep -q '^FAIL' "$work/$shot.log" && status=1
done
exit "$status"
