#!/bin/bash
# Offline GL regression and PNGs: cold Home and live background/source changes.
set -eu
cd "$(dirname "$0")/.."
ulimit -c 0
output="${1:-/Volumes/ExternalSSD/tmp/rev/fundohome-png}"
phase="${2:-after}"
mkdir -p "$output"
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelayouts-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
baseline_flags=()
if [ "$phase" = before ]; then
  git show bfd9c6b3868dfe4eab2a2ac76ce07cebbe12c490:src/home.c > "$NUVIO_DADOS/home-baseline.c"
  baseline_flags+=("-DPALCO_BASELINE=\"$NUVIO_DADOS/home-baseline.c\"")
fi
cc "${sources[@]}" "${baseline_flags[@]}" tests/fundohome_palco.c -Isrc -o "$NUVIO_DADOS/test" \
  -Dconnect=fundohome_no_connect -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations >"$output/$phase-build.log" 2>&1 || { cat "$output/$phase-build.log" >&2; exit 1; }
rc=0
for layout in 0 2; do
  for hero in 1 0; do
    case_data=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelayouts-shot.XXXXXX")
    NUVIO_DADOS="$case_data" "$NUVIO_DADOS/test" "$output" "$phase" "$layout" "$hero" \
      >"$output/$phase-L$layout-hero$hero.log" 2>&1 || rc=$?
    rm -rf "$case_data"
    tail -n 6 "$output/$phase-L$layout-hero$hero.log"
  done
done
for bmp in "$output/$phase-"*.bmp; do
  [ -e "$bmp" ] || continue
  sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
  rm "$bmp"
done
exit "$rc"
