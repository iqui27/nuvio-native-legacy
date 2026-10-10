#!/bin/bash
# #390: modelo de URL dos posteres longo (300-400), colado do celular, salvo e
# relido inteiro. Compila o app inteiro com -DAJUSTES_TESTE, sem janela.
#   bash tests/poster_modelo_longo.sh
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_DADOS=$(mktemp -d /tmp/nuvio-postermodelo-dados.XXXXXX)
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc -DAJUSTES_TESTE "${sources[@]}" tests/poster_modelo_longo.c -Isrc -o /tmp/nuvio-postermodelo-tests \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined -w
/tmp/nuvio-postermodelo-tests
