#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-library.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/biblioteca_servidores.c src/salvos.c src/focus.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/test"
"$work/test"
cc tests/servidores_setup.c src/srvsetup_net.c src/jellyfin.c src/listas.c src/js.c src/redeurl.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/setup"
"$work/setup"
cc tests/servidores_setup_http.c src/srvsetup_net.c src/jellyfin.c src/js.c src/jsw.c src/redeurl.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$work/http"
"$work/http"
# The fixture includes src/streams.c; link the real modules it references (chipfonte.sh pattern).
mkdir "$work/obj"
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c) continue;; esac
  printf '%s\n' "$source"
done | xargs -P "${NVQ_CC_JOBS:-8}" -I{} sh -c 'cc -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wno-deprecated-declarations -Wno-macro-redefined \
  -c "$0" -o "$1/$(echo "$0" | tr / _).o"' {} "$work/obj"
cc tests/servidores_source_filter.c "$work/obj/"*.o -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-declarations -o "$work/sources" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/sources"
cc tests/servidores_cache.c src/srvcache.c src/js.c src/jsw.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -o "$work/cache"
"$work/cache"
cc tests/servidores_rows.c src/servidores.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-macro-redefined -o "$work/rows"
"$work/rows"
