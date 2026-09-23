#!/bin/bash
# Capturas da tela Explorar (o ceu das historias), em BMP. Janela GL
# ESCONDIDA e desenho num FBO: nada aparece na tela de quem roda. Catalogo
# sintetico com arte do pacote; nao toca conta, cache de usuario nem rede
# (NUVIO_DADOS aponta para uma pasta temporaria). Fica fora da suite
# automatica (*_shot.sh): precisa de GL e de olho humano.
#
#   bash tests/explorar_shot.sh /tmp/nuvio-explorar
set -eu
cd "$(dirname "$0")/.."
out="${1:-/tmp/nuvio-explorar}"
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-explorar-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/explorar_shot.c -Isrc -o /tmp/nuvio-explorar-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-explorar-shot "$out"
