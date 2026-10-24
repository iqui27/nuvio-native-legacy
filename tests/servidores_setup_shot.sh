#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-setup-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/srvsetup.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/servidores_setup_shot.c -Isrc -o "$work/shot" -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
output=${1:-/Volumes/ExternalSSD/tmp/rev/servidores-setup-png}
mkdir -p "$output" "$work/data"
run_capture() {
  NUVIO_DADOS="$work/data" NUVIO_RAIL="${NUVIO_RAIL:-moderna}" NUVIO_SHOT_PT=1 NUVIO_SHOT_RELOGIO=1 sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/shot" "$1"
  python3 tools/servidores_png_check.py "$1"
}
if [ "${NUVIO_J2_MATRIX:-0}" = 1 ]; then
  for scale in .85 1 .9 1.1 1.2 1.3; do
    for glass in 0 1; do
      variant="$output/$scale-$glass"
      mkdir -p "$variant"
      NUVIO_SHOT_QUICK=1
      if [ "$scale" = .85 ] || [ "$scale" = 1 ]; then unset NUVIO_SHOT_QUICK; fi
      export NUVIO_SHOT_QUICK
      NUVIO_SHOT_ESCALA_TEXTO=$scale NUVIO_SHOT_VIDRO=$glass run_capture "$variant"
    done
  done
else
  run_capture "$output"
fi
