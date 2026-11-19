#!/bin/bash
# Offline TV timeline and native GL captures; no device or remote input.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-play-morph.XXXXXX")
trap 'rm -rf "$work"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/rev/j-21-play-morph}"
mkdir -p "$work/data" "$out"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc -O1 -g -DNV_SHOT_HOOKS -DSDL_GetTicks=play_morph_ticks \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  "${sources[@]}" tests/play_morph.c -Isrc -o "$work/test" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test" "$out"
