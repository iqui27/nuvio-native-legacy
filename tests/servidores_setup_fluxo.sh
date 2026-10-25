#!/bin/bash
# Personal-server setup: step history, default focus, on-step errors, and the
# wizard covering a live previous screen (no leak at the left edge).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-setup-flow.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/srvsetup.c) continue;; esac
  sources+=("$source")
done
binary=${NUVIO_SHOT_BIN:-$work/shot}
if [ "${NUVIO_SHOT_REUSE_BIN:-0}" != 1 ] || [ ! -x "$binary" ]; then
cc "${sources[@]}" tests/servidores_setup_shot.c -Isrc -o "$binary" -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
fi
output=${1:-$work/out}
mkdir -p "$output"
run() {
  local data; data=$(mktemp -d "$work/data.XXXXXX")
  NUVIO_DADOS="$data" NUVIO_RAIL=moderna NUVIO_SHOT_PT=1 NUVIO_SHOT_RELOGIO=1 \
    sandbox-exec -p '(version 1) (allow default) (deny network*)' "$binary" "${1:-$output}"
}
NUVIO_SHOT_FLOW=1 run
for scale in .85 1; do
  for glass in 0 1; do
    mkdir -p "$output/$scale-$glass"
    NUVIO_SHOT_BEHIND=1 NUVIO_SHOT_ESCALA_TEXTO=$scale NUVIO_SHOT_VIDRO=$glass run "$output/$scale-$glass"
  done
done
echo "PASS: setup flow and full-screen cover"
