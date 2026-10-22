#!/bin/bash
# Capturas do cartao da 2.0.2 em BMP: a previa nas tres cenas (menu de
# contexto, destaque da Home, ilha do relogio), a pagina "Apoie o projeto" com
# os QRs, ingles e animacoes reduzidas. Antes, as regras: Continuar leva ao
# apoio, Concluir fecha, Agora nao e Voltar fecham, e todos gravam a marca.
# Janela GL ESCONDIDA e desenho num FBO. Fica fora da suite (*_shot.sh): precisa de GL e
# de olho humano. Sem rede: as artes sao as do pacote (deploy/app/art).
#
#   bash tests/novidades202_shot.sh /tmp/nuvio-novidades202
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-n202-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades202_shot.c -Isrc \
  -o "${TMPDIR:-/tmp}/nuvio-n202-shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-n202-shot" "${1:-/tmp/nuvio-novidades202}"
