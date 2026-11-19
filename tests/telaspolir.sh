#!/bin/bash
# Offline TV polish gate and native 1080p captures. Never touches device data.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
export NAV21_OUT="${TELASPOLIR_OUT:-/Volumes/ExternalSSD/tmp/rev/telaspolir/after}"
mkdir -p "$TMPDIR" "$NAV21_OUT"
work=$(mktemp -d "$TMPDIR/telaspolir.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ "${1:-}" = --baseline ]; then
  mkdir "$work/tree"
  git archive "${TELASPOLIR_BASELINE:-00fd00ba5}" src deploy | tar -x -C "$work/tree"
  cp -R tests "$work/tree/tests"
  cd "$work/tree"
fi
export TELASPOLIR_OUT="$NAV21_OUT"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/ilha.c|src/reacao.c|src/player.c|src/srvsetup.c|src/conquistas.c|src/comecar.c|src/aiostreamsui.c|src/biblioteca.c|src/filmo.c|src/addonguia.c|src/detail.c|src/social.c|src/ajustes.c|src/streams.c|src/juntos.c|src/juntosui.c|src/home.c|src/extras.c|src/rede.c|src/busca.c|src/login.c) continue;; esac
  sources+=("$source")
done
sources+=(tests/nav21_gate.c tests/nav21_router.c tests/nav21_servers.c tests/nav21_achievements.c
  tests/nav21_start.c tests/nav21_aio.c tests/nav21_library.c tests/nav21_filmo.c
  tests/nav21_addonguide.c tests/nav21_detail.c tests/nav21_social.c tests/nav21_settings.c
  tests/nav21_sources.c tests/nav21_juntos.c tests/nav21_juntos_data.c tests/nav21_home.c
  tests/nav21_extras.c tests/nav21_network.c tests/nav21_island.c tests/nav21_rating.c tests/nav21_player.c
  tests/telaspolir_search.c tests/telaspolir_login.c tests/telaspolir_pages.c)
export CCACHE_DIR="$TMPDIR/nuvio-telaspolir-ccache"
compiler=(cc)
if command -v ccache >/dev/null; then compiler=(ccache cc); fi
objects=(); pids=()
for source in "${sources[@]}"; do
  object="$work/$(basename "$source" .c).o"
  "${compiler[@]}" -c "$source" -o "$object" -DAJUSTES_TESTE -DNV_SHOT_HOOKS -DNV_TELASPOLIR \
    -Isrc -Itests -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations &
  pids+=($!); objects+=("$object")
  if [ ${#pids[@]} -ge 4 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
cc "${objects[@]}" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -o "$work/nav"
mkdir -p "$work/data"
rc=0
NUVIO_DADOS="$work/data" "$work/nav" "$NAV21_OUT/violations.md" || rc=$?
if [ "${1:-}" != --baseline ]; then
  bash tests/agenda_geometry.sh || rc=$?
  python3 tests/nav21_contract.py || rc=$?
fi
exit "$rc"
