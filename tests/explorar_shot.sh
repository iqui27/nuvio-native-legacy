#!/bin/bash
# Capturas da tela Explorar 2.0 (climas, clima aberto, vizinhanca, trilha), em
# PNG. Janela GL ESCONDIDA e desenho num FBO: nada aparece na tela de quem
# roda. Catalogo sintetico com arte do pacote; sem chave do TMDB e sem rede
# (NUVIO_DADOS aponta para uma pasta temporaria), entao e o caminho LOCAL que
# aparece. Fica fora da suite automatica (*_shot.sh): precisa de GL e de olho
# humano.
#
#   bash tests/explorar_shot.sh /tmp/nuvio-explorar
set -eu
cd "$(dirname "$0")/.."
out="${1:-${TMPDIR:-/tmp}/nuvio-explorar}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-explorar-shot-XXXXXX")"
NUVIO_DADOS="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-explorar-dados-XXXXXX")"
export NUVIO_DADOS
trap 'rm -rf "$tmp" "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
cc "${sources[@]}" tests/explorar_shot.c -Isrc -o "$tmp/shot" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -DNV_SUPABASE_URL='""' -DNV_SUPABASE_ANON_KEY='""' -DNV_TV_LOGIN_BASE='""' \
  -DNV_TRAKT_CLIENT_ID='""' -DNV_TRAKT_CLIENT_SECRET='""' \
  -DNV_SIMKL_CLIENT_ID='""' -DNV_SIMKL_APP='""' -DNV_TMDB_API_KEY='""' \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$tmp/shot" "$out"
