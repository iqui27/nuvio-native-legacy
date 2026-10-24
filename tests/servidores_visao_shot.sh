#!/bin/bash
# Hidden GL/FBO PNG proof at 1920x1080; synthetic fixtures only.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-servers-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
objects="${TMPDIR:-/tmp}/nuvio-home21-shot-objects"
mkdir -p "$objects"
objectsToLink=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  object="$objects/${source//\//_}.o"
  if [ ! -e "$object" ] || [ "$source" -nt "$object" ]; then
    cc -c "$source" -Isrc -o "$object" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -w
  fi
  objectsToLink+=("$object")
done
cc "${objectsToLink[@]}" tests/servidores_visao_shot.c -Isrc -o "$work/shot" -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
output=${1:-/Volumes/ExternalSSD/tmp/rev/servidores21-png}
mkdir -p "$output" "$work/data"
NUVIO_DADOS="$work/data" "$work/shot" "$output"
