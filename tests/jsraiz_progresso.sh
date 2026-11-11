#!/bin/bash
# Array walk termination on malformed server answers. ASan + UBSan always: the
# malformed inputs are the point, and each case is bound by an alarm in the test.
#   bash tests/jsraiz_progresso.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-jsraiz.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread -fsanitize=address,undefined -fno-omit-frame-pointer)
flags+=(-I/opt/homebrew/include)
if command -v sdl2-config >/dev/null; then
  # shellcheck disable=SC2207
  flags+=($(sdl2-config --cflags) -I"$(sdl2-config --prefix)/include")
fi
flags+=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then linkflags=(-Wl,-dead_strip); else linkflags=(-ldl -Wl,--gc-sections); fi
cc "${flags[@]}" -DNV_TEST_SERVIDOR_PREPARAR -Drede_pedir=teste_rede_pedir -Dtmpfile=teste_tmpfile -c src/plex.c -o "$tmp/plex.o"
cc "${flags[@]}" -DNV_TEST_SERVIDOR_PREPARAR -Drede_pedir=teste_rede_pedir -c src/jellyfin.c -o "$tmp/jellyfin.o"
cc "${flags[@]}" tests/jsraiz_progresso.c "$tmp/plex.o" "$tmp/jellyfin.o" src/servidores.c src/srvcache.c src/srvsetup_net.c src/srvidx.c src/rede.c src/redeurl.c src/js.c src/jsw.c \
  "${linkflags[@]}" -lz -o "$tmp/teste"
"$tmp/teste"
