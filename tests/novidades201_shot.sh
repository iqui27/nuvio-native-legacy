#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do cartao da 2.0.1 em BMP: a previa nas tres cenas (Central de
# controle segurando, crescendo e aberta; a passagem; Ajustes; velocidade), a
# pagina "Apoie o projeto" com os QRs, ingles e animacoes reduzidas. Antes, as
# regras: Continuar leva ao apoio, Concluir fecha, Esquerda + OK pede a
# Central, Agora nao e Voltar fecham, e todos gravam a marca. Janela GL
# ESCONDIDA e desenho num FBO. Fica fora da suite (*_shot.sh): precisa de GL e
# de olho humano. Sem rede: as artes sao as do pacote (deploy/app/art).
#
#   bash tests/novidades201_shot.sh /tmp/nuvio-novidades201
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-n201-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades201_shot.c -Isrc \
  -o "${TMPDIR:-/tmp}/nuvio-n201-shot" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-n201-shot" "${1:-${TMPDIR:-/tmp}/nuvio-novidades201}"
