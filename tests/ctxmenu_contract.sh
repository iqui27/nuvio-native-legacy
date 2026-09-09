#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Exercita o contrato de compilacao do escopo sem abrir SDL nem fazer rede.
cc -Wall -Wextra -Werror -fsyntax-only \
  src/ctxmenu.c src/catalogo.c src/trakt.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined

# Regressao da intencao: ela tem de ser capturada antes do POST e o mesmo
# valor tem de chegar ao espelho local quando a resposta confirmar.
linha_intencao=$(rg -n 'intencao = !ci->naLista;' src/ctxmenu.c | cut -d: -f1)
linha_post=$(rg -n 'trakt_watchlist_tipo\(ci->imdb, ci->tipo, intencao\)' src/ctxmenu.c | cut -d: -f1)
# AGORA HA DOIS ESPELHOS, e por isso o `head -1`. Com "Onde o + salva" na
# Lista do Nuvio nao existe POST para esperar: a escrita local ja terminou, o
# estado vai direto para CONFIRMADA e o espelho e aplicado ali mesmo. O outro
# espelho continua sendo o da resposta 2xx do Trakt, em ctx_atualizar. O que o
# teste cobra e a ORDEM — intencao capturada antes do POST, espelho depois —, e
# ela vale para os dois; o primeiro e o mais restritivo dos dois.
linha_espelho=$(rg -n 'cat_definir_na_lista\(atual, intencao\)' src/ctxmenu.c | cut -d: -f1 | head -1)
[ "$linha_intencao" -lt "$linha_post" ]
[ "$linha_post" -lt "$linha_espelho" ]

# Resposta nao nula sozinha nunca pode virar sucesso: o contrato exige HTTP
# 2xx e usa a variante que devolve o status.
rg -q 'rede_postar_st\(url, 20, cab, corpo, &status\)' src/trakt.c
rg -q 'confirmado = status >= 200 && status < 300' src/trakt.c

# A pressao longa continua pertencendo a home.c e chega ao modal via KEYUP.
rg -q 'home.c:.*NV_HOLD_MS.*KEYUP' src/ctxmenu.h

echo 'ctxmenu contract: PASS'
