#!/bin/bash
# ISSUE #42: "Settings are not getting saved (in player caption settings
# mainly)". Roda o mesmo binario em DOIS PROCESSOS — "mata o processo e
# reabre" so prova alguma coisa quando os `static` de player.c voltam ao
# valor de fabrica entre uma chamada e outra, o que um so processo no mesmo
# `main` nao demonstra.
set -eu
cd "$(dirname "$0")/.."

DADOS="$(mktemp -d)"
ARTE="$(mktemp -d)"
trap 'rm -rf "$DADOS" "$ARTE"' EXIT

sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/player_prefs.c -Isrc -o /tmp/nuvio-player-prefs-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined

export NUVIO_DADOS="$DADOS"
export NUVIO_ARTE_TESTE="$ARTE"
/tmp/nuvio-player-prefs-tests escrever
/tmp/nuvio-player-prefs-tests ler
