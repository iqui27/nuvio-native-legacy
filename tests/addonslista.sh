#!/bin/bash
# addons.txt lido vira addon USAVEL (ligado), nao so visivel. Ver o cabecalho
# de tests/addonslista.c. NAO precisa de rede.
#
#   bash tests/addonslista.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,-dead_strip \
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# SO addons.c: o teste chama o leitor do arquivo e os acessores, que nao tocam
# rede, SDL nem descoberta. -dead_strip com secoes por funcao descarta o resto
# do modulo, entao nenhum dos vizinhos precisa de stub (mesma receita de
# tests/manifesto_cache.sh).
cc "${flags[@]}" src/addons.c tests/addonslista.c -o /tmp/nuvio-addonslista-tests
/tmp/nuvio-addonslista-tests
