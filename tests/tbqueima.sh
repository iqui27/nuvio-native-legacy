#!/bin/bash
# TorBox burn-in on LG and Android (src/tbqueima.c): auto pick, switch, the
# file's menus while burned, fallbacks, re-add, slow start, audio pick, Home.
# Fake player and TorBox, no network.   bash tests/tbqueima.sh   SANITIZE=1
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-tbqueima.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
SDL=$(sdl2-config --cflags 2>/dev/null || echo "-I/usr/include/SDL2 -I/opt/homebrew/include/SDL2 -I/opt/homebrew/include")
cc "${flags[@]}" -std=gnu11 -Isrc $SDL -O1 -g -Wall -Wextra -Werror -DNV_TBQ_TEST \
   src/tbqueima.c src/tbrelogio.c tests/tbqueima.c -pthread -lm -o "$D/t"
"$D/t"
# The real targets compile it too (no warnings).
for d in NV_WEBOS NV_ANDROID; do cc -std=gnu11 -Isrc $SDL -D$d -fsyntax-only -Wall -Wextra -Werror src/tbqueima.c; done
cc -std=gnu11 -Isrc $SDL -DNV_TPK -fsyntax-only -Wall -Wextra -Werror src/tbqueima.c
