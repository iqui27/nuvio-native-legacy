#!/bin/bash
# Focused offline gate for both Discord reports.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/busca_titulo.sh
bash tests/tpk_voz.sh
bash tests/tpk_voz_host.sh
bash tests/wgt_voz.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-voice-platforms.XXXXXX")
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2 2>/dev/null || sdl2-config --cflags --libs)"
for platform in NV_ANDROID NV_TEXTO_SDL_TESTE; do
  cc -O1 -g -Isrc -D"$platform" src/sistexto.c src/entrada_texto.c tests/voice_platforms.c \
    -o "$work/test" "${sdl_flags[@]}"
  SDL_VIDEODRIVER=dummy "$work/test"
done
