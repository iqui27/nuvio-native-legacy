#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas do cartao da 1.7 em BMP: as tres cenas da previa (ilha do relogio
# com o modal e o painel de Salvos, Spotlight, layout Apple TV) em pt e en, nos
# momentos de cada uma, o modal e o Spotlight em ru e as animacoes reduzidas.
# Antes, as regras: OK abre o Spotlight, Esquerda + OK = Agora nao, Voltar =
# Agora nao, cima poe o foco na previa (direita/OK trocam a cena sem fechar), e
# todos gravam a marca. Janela GL ESCONDIDA e desenho num FBO: nada aparece na
# tela de quem roda. Fica fora da suite (*_shot.sh): precisa de GL e de olho
# humano. Sem rede: as artes sao as do pacote.
#
#   bash tests/novidades170_shot.sh /tmp/nuvio-novidades170
#   N170_SO=ja bash tests/novidades170_shot.sh      # so um grupo de capturas
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-n170-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades170_shot.c -Isrc \
  -o ${TMPDIR:-/tmp}/nuvio-n170-shot -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-n170-shot "${1:-${TMPDIR:-/tmp}/nuvio-novidades170}"
