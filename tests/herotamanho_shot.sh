#!/bin/bash
# Capture all hero sizes with glass off, using the existing local-art harness.
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/herotamanho-png}"
mkdir -p "$output"
shot_data=$(mktemp -d "${TMPDIR:-/tmp}/herotamanho-shot.XXXXXX")
trap 'rm -rf "$shot_data"' EXIT
export NUVIO_DADOS="$shot_data"
fixtures="$shot_data/fixtures"
mkdir -p "$fixtures"
sips -s format png deploy/app/art/00.jpg --out "$fixtures/bg.png" >/dev/null
cp deploy/app/art/logo/00.png "$fixtures/wide.png"
cp deploy/app/art/logo/01.png "$fixtures/tall.png"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/herologo_shot.c -Isrc -DNV_TOUCH_UI -DNV_SHOT_HOOKS \
  -o "$shot_data/shot" -O1 -g \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
for layout in 0 1 2; do
  for size in 0 1 2; do
    "$shot_data/shot" "$shot_data" "$fixtures" "$output/tv-layout-$layout-size-$size.png" "$size" "$layout" 1920 1080
  done
done
for size in 0 1 2; do
  "$shot_data/shot" "$shot_data" "$fixtures" "$output/portrait-size-$size.png" "$size" 0 540 960
done
