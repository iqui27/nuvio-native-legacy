#!/bin/bash
# Botao de microfone no .tpk (teclado da Samsung): a ponte do Voz.cs contra sistexto. Ver o .c.
#   bash tests/tpk_voz.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-voz-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
read -r -a SDL_FLAGS <<< "$(pkg-config --cflags --libs sdl2 2>/dev/null || sdl2-config --cflags --libs)"
flags=(-O1 -g -Isrc -DNV_TPK -Wall -Wextra -Wno-unused-parameter)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/sistexto.c src/entrada_texto.c tests/tpk_voz.c -o "$tmp/t" "${SDL_FLAGS[@]}"
SDL_VIDEODRIVER=dummy "$tmp/t" | grep -E '^(PASS|Assert)|\[texto\] (host|microfone)'
