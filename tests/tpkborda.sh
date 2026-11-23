#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Premult da borda das legendas (src/tpkborda.c) e as contas de Subtitle
# contrast / gamma (src/assperf.c). Ver tests/tpkborda.c. Precisa dos headers
# do SDL2 (Homebrew no Mac; NV_SDL_INC aponta outra pasta).
set -eu
cd "$(dirname "$0")/.."
SDLI="${NV_SDL_INC:-$(pkg-config --cflags-only-I sdl2 2>/dev/null | sed 's#/SDL2##') -I/opt/homebrew/include}"
O="$TMPDIR/nuvio-tpkborda"; mkdir -p "$O"
cc -DNV_TPK -Wall -Wextra -Isrc $SDLI -c src/tpkborda.c -o "$O/tpkborda.o"
cc -DNV_ASSPERF_TESTE -Wall -Wextra -Isrc $SDLI -c src/assperf.c -o "$O/assperf.o"
cc -DNV_ASSPERF_TESTE -Wall -Wextra -Isrc -c tests/tpkborda.c -o "$O/teste.o"
cc "$O/teste.o" "$O/tpkborda.o" "$O/assperf.o" -o "$O/teste" -lpthread -lm
"$O/teste"
