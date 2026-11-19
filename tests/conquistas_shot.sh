#!/bin/bash
# Capturas e regras da tela Conquistas (2.1) em PNG 1920x1080: hub, aba, familias,
# detalhe, aviso depois dos creditos e o botao Conquistas do Perfil. Antes, as
# regras por evento de tecla (layout, varredura D-pad, Voltar, aviso sem tomar o
# foco, Perfil). Janela GL ESCONDIDA e desenho num FBO. Fica fora da suite
# (*_shot.sh): precisa de GL.
#
#   bash tests/conquistas_shot.sh tests/out/conquistas/conquistas
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-conquistas-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
BIN="${NUVIO_SHOT_BIN:-/tmp/nuvio-conquistas-shot}"
bash tests/j4_compile.sh tests/conquistas_shot.c "$BIN"
out="${1:-/tmp/nuvio-conquistas}"
mkdir -p "$(dirname "$out")"
"$BIN" "$out"
