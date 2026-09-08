#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Linka somente a lógica exercitada, sem inicializar SDL/AppKit nem vídeo.
# Além de ser rápido, permite ASAN no macOS beta sem o diálogo do loader SDL.
#
# src/fileiras.c ENTRA NA LISTA porque home_layout.c chama sincronizarFileiras,
# que passou a consultar fil_unir/fil_limite quando a escolha local de fileiras
# nasceu. A lista e escrita a mao, entao ela nao acompanha sozinha: o teste
# ficou sem linkar ("_fil_unir referenced from _sincronizarFileiras") e o
# script morria antes de rodar um caso sequer.
cc "${flags[@]}" src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c tests/home_layout.c \
  -Isrc -o /tmp/nuvio-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-home-tests
