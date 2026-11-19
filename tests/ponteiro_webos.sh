#!/bin/bash
# #204: 484 do webOS longe da seta nao apaga a seta do sistema. Ver o .c.
#
#   bash tests/ponteiro_webos.sh
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-ponteiro-webos.XXXXXX")
trap 'rm -f "$bin"' EXIT
if [ "$(uname -s)" = Linux ]; then
  read -r -a sdlFlags <<< "$(pkg-config --cflags --libs sdl2)"
else
  sdlFlags=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2)
fi
cc -DNV_PONT_WEBOS_TESTE tests/ponteiro_webos.c src/ponteiro.c -Isrc -o "$bin" -Wall -Wextra \
  "${sdlFlags[@]}" -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$bin"
