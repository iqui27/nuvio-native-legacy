#!/bin/bash
# Home com cada combinacao de "Background do hero" x "Destaque com outra
# arte", em BMP. Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh):
# precisa de janela GL, de REDE (metahub, TMDB, Trakt) e de olho humano.
#
# As chaves do TMDB e do Trakt saem do local.properties do app web pelo
# tools/env.sh, como em tools/mac.sh — sem elas as fontes virtuais falham e a
# captura mostra a reserva (o que tambem e um caso valido de olhar).
#
#   bash tests/heroarte_shot.sh /tmp/nuvio-heroarte
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-heroarte-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

ENV_D=$(tools/env.sh --allow-unconfigured 2>/dev/null || true)
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
eval cc '"${sources[@]}"' tests/heroarte_shot.c -Isrc -o /tmp/nuvio-heroarte-shot \
  -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-heroarte-shot "$NUVIO_DADOS" "$@"
