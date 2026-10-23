#!/bin/bash
# Link the production app and FPS instrumentation without running UI/network.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fpscena-build.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc src/*.c src/dts/*.c -Isrc -o "$work/app" -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
cc -fsyntax-only tests/fluidez_perf.c -Isrc -DNV_SHOT_HOOKS -DNV_FLUIDEZ_PERF \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
echo 'ok: production link + host FPS fixture compile (app not executed)'
