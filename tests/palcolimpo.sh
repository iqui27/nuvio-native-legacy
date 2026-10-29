#!/bin/bash
# Real GL rendering: Stage budget, fixed geometry, crossfades, layouts and glass.
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/palcolimpo}"
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
if [ "$phase" = before ] || [ "$phase" = original-before ] || [ "$phase" = review-before ]; then
  baseline=2f60a19ffd4939b3a41aa305d9af98ba676a9388
  if [ "$phase" = original-before ]; then
    # Before Stage settings existed: exercise the original full-hero/poster fallback.
    baseline=c1e6553b7^
    baseline_flags+=(-DPALCO_ORIGINAL)
  elif [ "$phase" = review-before ]; then
    baseline=30923efaf
  fi
  git show "$baseline:src/home.c" > "$NUVIO_DADOS/home-baseline.c"
  if [ "$phase" = original-before ]; then
    # The shared fixture references these later Stage fields; historical drawing stays intact.
    printf '%s\n' 'static int palcoFundoModo = 1;' 'static float palcoFundoAlpha = 1;' >> "$NUVIO_DADOS/home-baseline.c"
  fi
  baseline_flags+=("-DPALCO_BASELINE=\"$NUVIO_DADOS/home-baseline.c\"")
fi
cc "${sources[@]}" "${baseline_flags[@]}" tests/palcolimpo.c -Isrc -o "$NUVIO_DADOS/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations >"$output/$phase-build.log" 2>&1
rc=0
scales="0.85 1"
glasses="0 1"
if [ "$phase" = before ] || [ "$phase" = original-before ] || [ "$phase" = dynamic-only ] || [[ "$phase" = review-* ]]; then scales=1; glasses=0; fi
for scale in $scales; do
  for glass in $glasses; do
    case_data=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homelayouts-shot.XXXXXX")
    NUVIO_DADOS="$case_data" "$NUVIO_DADOS/shot" "$output" "$phase" "$scale" "$glass" \
      >"$output/$phase-$scale-g$glass.log" 2>&1 || rc=$?
    rm -rf "$case_data"
    tail -n 6 "$output/$phase-$scale-g$glass.log"
  done
done
for bmp in "$output/$phase-"*.bmp; do
  [ -e "$bmp" ] || continue
  sips -s format png "$bmp" --out "${bmp%.bmp}.png" >/dev/null
  rm "$bmp"
done
exit "$rc"
