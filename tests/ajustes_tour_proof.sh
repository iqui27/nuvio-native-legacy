#!/bin/bash
# Capture the two tour materials and compare them with the approved mock.
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/ajtour-fix-png}"
mock="${2:-/Volumes/ExternalSSD/nv-planos/ajustes-ux/png-v2/04.png}"
mkdir -p "$output"
binary="$(mktemp "${TMPDIR:-/tmp}/nuvio-tour-proof.XXXXXX")"
trap 'rm -f "$binary"' EXIT
export NUVIO_SHOT_FONTE=3 NUVIO_SHOT_IDIOMA=0
export NUVIO_AJ_QUADROS="tour tour-more tour@8 tour-where@8 tour-where"
NUVIO_SHOT_VIDRO=0 NUVIO_SHOT_BIN="$binary" bash tests/ajustes_shot.sh "$output/solid"
NUVIO_SHOT_VIDRO=1 "$binary" "$output/glass"
for material in solid glass; do
  app="$output/$material-tour.png"
  test "$(magick identify -format '%wx%h' "$app")" = 1920x1080
  test "$(magick identify -format '%wx%h' "$mock")" = 1920x1080
  magick \
    \( "$mock" -background '#171819' -fill white -font deploy/app/fonts/InterDisplay-Regular.ttf -pointsize 20 -gravity north -splice 0x42 -annotate +0+10 'APPROVED MOCK' \) \
    \( "$app" -background '#171819' -fill white -font deploy/app/fonts/InterDisplay-Regular.ttf -pointsize 20 -gravity north -splice 0x42 -annotate +0+10 'APP PT-BR' \) \
    \( "$mock" "$app" -evaluate-sequence mean -background '#171819' -fill white -font deploy/app/fonts/InterDisplay-Regular.ttf -pointsize 20 -gravity north -splice 0x42 -annotate +0+10 '50% OVERLAY' \) \
    +append "$output/trio-$material-tour.png"
  magick "$output/$material-tour-q8.png" "$app" +append "$output/anim-enter-$material.png"
  magick "$output/$material-tour-where-q8.png" "$output/$material-tour-where.png" \
    +append "$output/anim-exit-$material.png"
done
