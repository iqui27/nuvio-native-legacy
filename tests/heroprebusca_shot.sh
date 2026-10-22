#!/bin/bash
# Mac: pre-busca do destaque e sinopse dos candidatos (tests/heroprebusca_shot.c).
#
# As chaves do TMDB e do Trakt saem do local.properties do app web pelo
# tools/env.sh, como em tools/mac.sh — sem elas as fontes virtuais falham e a
# captura mostra a reserva (o que tambem e um caso valido de olhar).
#
#   bash tests/heroprebusca_shot.sh [segundos]
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-heroprebusca-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

ENV_D=$(tools/env.sh --allow-unconfigured 2>/dev/null || true)
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
eval cc '"${sources[@]}"' tests/heroprebusca_shot.c -Isrc -o /tmp/nuvio-heroprebusca-shot \
  -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-heroprebusca-shot "$NUVIO_DADOS" "$@"
