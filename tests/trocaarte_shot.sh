#!/bin/bash
# Capturas da tela "Trocar arte" (#142) sobre a pagina do titulo, em PNG, sem
# rede. Nao entra na suite (testa-tudo.sh pula *_shot.sh): precisa de janela
# GL e de olho humano — mas as asserts de fluxo (abrir pelo circular, previa,
# OK grava, Automatico apaga, Voltar nao grava) derrubam a captura se falharem.
#
#   bash tests/trocaarte_shot.sh /tmp/nuvio-trocaarte
#
# Compila tudo MENOS src/main.c e src/detail.c, que a captura inclui (precisa
# de nivel, botao e arteDe, que sao estaticos de detail.c).
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA: a captura GRAVA a escolha, e
# o proprio programa se recusa a rodar se dados_dir() nao for ela.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-trocaarte-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/detail.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/trocaarte_shot.c -Isrc -o /tmp/nuvio-trocaarte-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-trocaarte-shot "$@"
