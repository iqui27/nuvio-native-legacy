#!/bin/bash
# Foreground-only offline native D-pad gate; isolated data, no TV or accounts.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nav21.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${NAV21_OUT:-/Volumes/ExternalSSD/tmp/rev/navteste}
export NAV21_OUT="$out"
mkdir -p "$out" "$work/data"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/ilha.c|src/reacao.c|src/player.c|src/srvsetup.c|src/conquistas.c|src/comecar.c|src/aiostreamsui.c|src/biblioteca.c|src/filmo.c|src/addonguia.c|src/detail.c|src/social.c|src/ajustes.c|src/streams.c|src/juntos.c|src/juntosui.c|src/home.c|src/extras.c|src/rede.c) continue;; esac
  sources+=("$source")
done
timeout 240 cc -DAJUSTES_TESTE -DNV_SHOT_HOOKS "${sources[@]}" tests/nav21.c tests/nav21_router.c tests/nav21_servers.c tests/nav21_achievements.c tests/nav21_start.c tests/nav21_aio.c tests/nav21_library.c tests/nav21_filmo.c tests/nav21_addonguide.c tests/nav21_detail.c tests/nav21_social.c tests/nav21_settings.c tests/nav21_sources.c tests/nav21_juntos.c tests/nav21_juntos_data.c tests/nav21_home.c tests/nav21_extras.c tests/nav21_network.c tests/nav21_island.c tests/nav21_rating.c tests/nav21_player.c \
  -Isrc -Itests -I"$work" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -o "$work/nav21"
export NUVIO_DADOS="$work/data"
timeout 240 "$work/nav21" "$out/violations.md"
