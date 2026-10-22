#!/bin/bash
# Um titulo, um lugar: o que o cartao da ilha / a faixa "Retomar agora" segura
# fica fora de "Continuar assistindo" e volta na frente quando sai (cwretido.h).
#
#   bash tests/cwretido.sh
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-cwretido-home-tests"
cc src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/ajlog.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/cwordem.c src/cwretido.c src/fonteregra.c src/regexjs.c tests/cwretido_home.c tests/amigosfil_stub.c \
  -Isrc -o "$out" -O1 -g -ffunction-sections -fdata-sections \
  -DNV_SHOT_HOOKS -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
"$out"
