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
#
# A busca de fontes tambem roda aqui (a causa da folha vazia, B6/D5), com rede,
# parser, cache e i18n dublados no proprio teste — ver tests/addonslista.c.
# js.c entra pelo caso do #112: o manifesto do addon de canal e lido de
# verdade (capacidadesDoManifesto), porque e ele que decide o tipo pedido.
cc "${flags[@]}" src/addons.c src/js.c tests/addonslista.c -o /tmp/nuvio-addonslista-tests
saida=$(/tmp/nuvio-addonslista-tests)
echo "$saida" | grep -v '^\[addons\]' || true
echo "$saida" | grep -q 'addonslista: ok'
# resposta curta sem fonte vai para o log com o texto (os 75 bytes do id 1504)
echo "$saida" | grep -qF '[addons] Formato antigo: resposta sem fonte: {"err":"Invalid debrid key"}' \
  || { echo "FALHOU: amostra da resposta sem fonte fora do log"; exit 1; }
echo "$saida" | grep -qF '[addons] Fonte e catalogo: resposta sem fonte: {"streams":[]}' \
  || { echo "FALHOU: amostra de {\"streams\":[]} fora do log"; exit 1; }
echo "addonslista.sh: ok"
