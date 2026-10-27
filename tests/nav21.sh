#!/bin/bash
# Foreground-only offline native D-pad gate; isolated data, no TV or accounts.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nav21.XXXXXX")
trap 'rm -rf "$work"' EXIT
out=${NAV21_OUT:-/Volumes/ExternalSSD/tmp/rev/navteste}
export NAV21_OUT="$out"
mkdir -p "$out" "$work/data"
python3 tests/nav21_contract.py
bash tests/j4_compile.sh tests/nav21.c "$work/nav21-social"
NUVIO_DADOS="$work/data" "$work/nav21-social"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/ilha.c|src/reacao.c|src/player.c|src/srvsetup.c|src/conquistas.c|src/comecar.c|src/aiostreamsui.c|src/biblioteca.c|src/filmo.c|src/addonguia.c|src/detail.c|src/social.c|src/ajustes.c|src/streams.c|src/juntos.c|src/juntosui.c|src/home.c|src/extras.c|src/rede.c) continue;; esac
  sources+=("$source")
done
timeout 600 cc -DAJUSTES_TESTE -DNV_SHOT_HOOKS "${sources[@]}" tests/nav21_gate.c tests/nav21_router.c tests/nav21_servers.c tests/nav21_achievements.c tests/nav21_start.c tests/nav21_aio.c tests/nav21_library.c tests/nav21_filmo.c tests/nav21_addonguide.c tests/nav21_detail.c tests/nav21_social.c tests/nav21_settings.c tests/nav21_sources.c tests/nav21_juntos.c tests/nav21_juntos_data.c tests/nav21_home.c tests/nav21_extras.c tests/nav21_network.c tests/nav21_island.c tests/nav21_rating.c tests/nav21_player.c \
  -Isrc -Itests -I"$work" -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -o "$work/nav21"
export NUVIO_DADOS="$work/data"
timeout 900 "$work/nav21" "$out/violations.md"
# Settings includes Pencil Open/Left/Right/Back render and reduced-motion checks.
# Also preserve the tour and setup gates for real actions and recovery flows.
bash tests/ajustes_tour.sh
NUVIO_SHOT_FLOW_ONLY=1 bash tests/servidores_setup_fluxo.sh "$@"

# Keep the live onboarding, translation and capture regressions as well.
rc=0
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c) continue;; esac
  sources+=("$source")
done
binary="$work/nav21-live"
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
  -I/opt/homebrew/include/SDL2 -o "$binary-idioma" \
  -Wno-macro-redefined -Wno-deprecated-declarations
"$binary-idioma" || rc=1
cc "${sources[@]}" tests/nav21_live.c -Isrc -o "$binary" \
  -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$binary" || rc=1
[ "$rc" = 0 ] || exit "$rc"
output="$TMPDIR/nav21-comecar"
mkdir -p "$output"
NUVIO_NAV21_REGRESSION=1 NUVIO_J2_MATRIX=1 NUVIO_SHOT_QUICK=1 bash tests/comecar_shot.sh "$output" || rc=1
exit "$rc"
