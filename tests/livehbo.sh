#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-livehbo.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc "${flags[@]}" tests/guiafiltro_loader.c src/ltv.c src/js.c src/idcanal.c src/dados.c src/marco.c \
  src/guiaaddons.c src/guiafiltro.c src/livecat.c src/epg.c src/xtepg.c src/lembrete.c src/guialembrete.c \
  src/fontecache.c src/cachearte.c src/cachedisco.c src/xmltv.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -Wno-macro-redefined -lz -o "$work/test"
GUIAFILTRO_CASE=live-hbo NUVIO_DADOS="$work/data" "$work/test" "$work/data"
cc "${flags[@]}" tests/livehbo_groups.c src/js.c src/buscanorm.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -Wno-macro-redefined -o "$work/groups"
"$work/groups"
