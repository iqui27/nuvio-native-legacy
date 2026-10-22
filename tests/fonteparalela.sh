#!/bin/bash
# Conferir varias fontes ao mesmo tempo (2.0.2, opcional): a ordem da fila vale
# sempre. Ver tests/fonteparalela.c.
#
#   bash tests/fonteparalela.sh
set -eu
cd "$(dirname "$0")/.."
bin="${TMPDIR:-/tmp}/nuvio-fonteparalela-tests"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc src/fonteparalela.c src/fonteauto.c tests/fonteparalela.c -o "$bin" -lpthread
"$bin"

# CONTRATO: o paralelismo so existe atras do ajuste (ligado de fabrica), so nas
# primeiras 3 E so para fontes ja em cache no debrid (fonteparalela_prefixo +
# prontaNoDebrid: nao foraCache, nao torrent), e so em "Melhor fonte". Fonte fora
# do cache segue uma por vez (issue #130): conferi-la baixaria de verdade.
corpo=$(awk '/^int stream_primeira_boa\(/,/^}/' src/streams.c)
if ! printf '%s' "$corpo" | grep -q 'ajustes_fonte_conferir_varias()' ||
   ! printf '%s' "$corpo" | grep -q 'fonteparalela_soltando('; then
  echo "fonteparalela: stream_primeira_boa nao usa fonteparalela atras do ajuste"; exit 1
fi
# A Conferencia da corrida nao pode ser a da pilha: os fios que seguem
# conferindo depois que fonteparalela volta ainda a usam (ver ConfDona).
if printf '%s' "$corpo" | grep -q 'fonteparalela[_a-z]*(.*&c[,)]'; then
  echo "fonteparalela: stream_primeira_boa passa a Conferencia da pilha para a corrida"; exit 1
fi
if printf '%s' "$corpo" | grep -v 'ajustes_fonte_conferir_varias' | grep -q 'pthread_create'; then
  echo "fonteparalela: stream_primeira_boa abre fios por fora do ajuste"; exit 1
fi
if ! printf '%s' "$corpo" | grep -q 'fonteparalela_prefixo(fila, nf, 3, prontaNoDebrid'; then
  echo "fonteparalela: a conferencia conjunta nao esta limitada as fontes prontas no debrid"; exit 1
fi
pronta=$(awk '/^static int prontaNoDebrid\(/,/^}/' src/streams.c)
if ! printf '%s' "$pronta" | grep -q '!lista\[i\].foraCache' || ! printf '%s' "$pronta" | grep -q '!soP2P'; then
  echo "fonteparalela: prontaNoDebrid nao exclui fora-do-cache e torrent"; exit 1
fi
if ! grep -q '^  0, /\* check several sources at once' src/ajustes_ux_padrao.inc; then
  echo "fonteparalela: padrao do ajuste nao e Ligado"; exit 1
fi
echo "fonteparalela: contrato de streams.c ok"
