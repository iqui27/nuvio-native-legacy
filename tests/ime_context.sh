#!/bin/bash
# Android password context and webOS keyboard behavior; no network or window.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ime-context.XXXXXX")
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2 2>/dev/null || sdl2-config --cflags --libs)"
flags=(-O1 -g -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
for platform in NV_ANDROID NV_TEXTO_SDL_TESTE; do
  cc "${flags[@]}" -D"$platform" src/sistexto.c src/entrada_texto.c tests/voice_platforms.c \
    -o "$work/test" "${sdl_flags[@]}"
  SDL_VIDEODRIVER=dummy "$work/test"
done
