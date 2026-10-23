#!/bin/bash
# Live TV category suggestions (src/livecat.c): section per unclassified channel,
# disk cache per base name, batches of 200, no network for classified channels.
# Includes Worker HTTP contract, shared limiter and cleanup regressions.
#
#   bash tests/livecat.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-livecat-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc src/livecat.c src/dados.c src/js.c tests/livecat.c -Isrc -DNV_REC_URL='"http://rec.test"' \
  -o "$tmp/unidade" -O1 -g -Wall -Wextra -Wno-unused-function
rc=0
NUVIO_DADOS="$tmp/unit" "$tmp/unidade" | grep -v "^\[dados\]" || rc=1
NUVIO_DADOS="$tmp/unit" "$tmp/unidade" restart | grep -v "^\[dados\]"
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc tests/livecat_guia.c src/guiafiltro.c src/livecat.c src/dados.c src/ltv.c src/js.c src/idcanal.c -Isrc \
  -DNV_REC_URL='"http://rec.test"' -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -fsanitize-coverage=trace-pc-guard -Wno-macro-redefined "${flags[@]}" -o "$tmp/guia"
# Run both integration regressions even when one fails (FAIL-before evidence).
NUVIO_DADOS="$tmp/guide" "$tmp/guia" || rc=1
NUVIO_DADOS="$tmp/guide" "$tmp/guia" restart || rc=1
if [ "${1:-}" = --guide-only ]; then exit "$rc"; fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/livecat_toggle.c -Isrc -o "$tmp/toggle" -O1 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
NUVIO_DADOS="$tmp/toggle-data" "$tmp/toggle" || rc=1
[ "$rc" = 0 ] || exit "$rc"
node servidor/recomendacoes/teste-livetvcat.mjs
