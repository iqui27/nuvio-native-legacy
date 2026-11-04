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
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-ldl -Wl,--gc-sections); fi
cc "${flags[@]}" tests/jsraiz_progresso.c src/plex.c src/jellyfin.c src/servidores.c src/srvcache.c src/srvsetup_net.c src/srvidx.c src/rede.c src/redeurl.c src/js.c src/jsw.c \
  -o "$tmp/teste"
"$tmp/teste"
