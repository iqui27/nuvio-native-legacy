#!/bin/bash
# One setup-flow check and native evidence capture, entirely isolated data.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-setuptv.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${1:-/Volumes/ExternalSSD/tmp/rev/setuptv2}
mkdir -p "$out" "$work/data" "$work/server-data"
cc -std=gnu11 -Isrc -Itests src/celular.c tests/guiav5_phone.c -lpthread -o "$work/phone"
"$work/phone"
python3 tools/idiomas.py
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guiav5ui.c|src/srvsetup.c|src/epg.c) continue;; esac
  sources+=("$source")
done
flags=(-Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""'
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' -DNV_SIMKL_CLIENT_ID='""'
  -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""')
cc "${sources[@]}" src/srvsetup.c tests/setuptv_epg.c tests/setuptv.c "${flags[@]}" -o "$work/shot"
NUVIO_DADOS="$work/data" "$work/shot" "$out"
# Reuse the same production objects for server navigation and native screenshots.
cc "${sources[@]}" src/epg.c src/guiav5ui.c tests/servidores_setup_shot.c "${flags[@]}" -o "$work/servers"
NUVIO_DADOS="$work/server-data" NUVIO_SHOT_FLOW=1 sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/servers" "$out"
NUVIO_DADOS="$work/server-data" NUVIO_SHOT_SCREEN='setup-plex-pin setup-bibliotecas setup-pronto' NUVIO_SHOT_QUICK=1 sandbox-exec -p '(version 1) (allow default) (deny network*)' "$work/servers" "$out"
git diff --check
echo 'PASS: setup TV flow, server navigation, translations and PNG evidence'
