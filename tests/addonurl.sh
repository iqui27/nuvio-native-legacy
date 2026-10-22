#!/bin/bash
# #201: a URL de um addon chega INTEIRA ao pedido (1500 caracteres), e a que
# passa de NV_ADDON_URL_MAX e recusada com uma linha de log — nunca cortada.
# Ver o cabecalho de tests/addonurl.c. NAO precisa de rede.
#
#   bash tests/addonurl.sh
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,-dead_strip \
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# SO addons.c e js.c, com a mesma receita de tests/addonslista.sh: -dead_strip
# descarta o resto do modulo e os vizinhos sao dublados no proprio teste.
cc "${flags[@]}" src/addons.c src/addonstats.c src/js.c tests/addonurl.c -o /tmp/nuvio-addonurl-tests
saida=$(/tmp/nuvio-addonurl-tests)
echo "$saida" | grep -v '^\[addons\]' || true
echo "$saida" | grep -q 'addonurl: ok'
# A recusa e DITA: nome do addon e tamanho. Duas vezes — a linha do arquivo
# (2100) e a instalacao pela TV (2100).
n=$(echo "$saida" | grep -cF '[addons] Grande: URL de 2100 caracteres nao cabe (maximo 2047): addon ignorado')
[ "$n" = 1 ] || { echo "FALHOU: esperava 1 linha de recusa, vieram $n"; exit 1; }
# E a URL em si nunca vai para o log: ela carrega a chave de debrid do dono.
if echo "$saida" | grep -q 'SEGREDO'; then echo "FALHOU: URL de addon no log"; exit 1; fi
echo "addonurl.sh: ok"
