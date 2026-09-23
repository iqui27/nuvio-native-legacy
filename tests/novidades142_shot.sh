#!/bin/bash
# Capturas do cartao da 1.4.2 (pt, en, foco em "Depois", reduzido) em BMP, e
# as regras do cartao: OK roda o diagnostico, Voltar = Depois, e os dois gravam
# a marca da 1.4.2 E a da 1.4. Janela GL ESCONDIDA e desenho num FBO: nada
# aparece na tela de quem roda. Fica fora da suite (*_shot.sh): precisa de GL
# e de olho humano.
#
#   bash tests/novidades142_shot.sh /tmp/nuvio-novidades142
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-n142-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades142_shot.c -Isrc \
  -o /tmp/nuvio-n142-shot -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-n142-shot "${1:-/tmp/nuvio-novidades142}"
