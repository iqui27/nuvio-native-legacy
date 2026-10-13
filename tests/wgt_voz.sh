#!/bin/bash
# Botao de microfone no .wgt (teclado da Samsung pelo <input> do
# tizen-shell.html): o C com -D__EMSCRIPTEN__ e um emscripten.h fingido, e o
# JS de verdade do shell no node. Ver tests/wgt_voz.c e tests/wgt_voz_shell.js.
#   bash tests/wgt_voz.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-wgt-voz-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
read -r -a SDL_FLAGS <<< "$(pkg-config --cflags --libs sdl2 2>/dev/null || sdl2-config --cflags --libs)"
flags=(-O1 -g -Itests/wgt_stub -Isrc -D__EMSCRIPTEN__ -Wall -Wextra -Wno-unused-parameter)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/sistexto.c src/entrada_texto.c tests/wgt_voz.c -o "$tmp/t" "${SDL_FLAGS[@]}"
for m in 1 -1 0 sem; do SDL_VIDEODRIVER=dummy "$tmp/t" "$m" | grep -E '^(PASS|Assert)'; done
node tests/wgt_voz_shell.js
