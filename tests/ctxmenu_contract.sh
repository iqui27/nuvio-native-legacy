#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Exercita o contrato de compilacao do escopo sem abrir SDL nem fazer rede.
cc -Wall -Wextra -Werror -fsyntax-only \
  src/ctxmenu.c src/catalogo.c src/trakt.c tests/stub_fichameta.c src/cwordem.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined

# Regressao da intencao: ela tem de ser capturada antes do POST e o mesmo
# valor tem de chegar ao espelho local quando a resposta confirmar.
linha_intencao=$(rg -n 'intencao = !tituloSalvo\(ci\);' src/ctxmenu.c | cut -d: -f1)
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

# O TETO DO MENU TEM DE CABER O MENU. Issue #36: CTX_MAX era 3 e havia quatro
# opcoes possiveis ao mesmo tempo (filme com progresso), entao montar() escrevia
# em ops[3]. O sintoma foi a ultima linha nunca acender, porque focoAnim so era
# animado ate CTX_MAX. Aqui se cobra que o numero de opcoes que montar() pode
# empilhar nunca passe do vetor, e que ops[] so seja escrito por juntar().
ctx_max=$(rg -n '^#define CTX_MAX ' src/ctxmenu.c | sed 's/.*CTX_MAX *//')
# O menu de LISTA (72fc391f: Abrir lista / Biblioteca / Home, ops OP_L_*) e um
# ramo que termina em `return` dentro de montar(): nunca empilha com as demais,
# entao so as linhas de juntar() de fora dele contam contra o teto.
# O menu da FILEIRA (Home 2.1: fixar, mover, ocultar; ops OP_F_*) e o mesmo tipo
# de ramo, com `return` no fim: nao empilha com as opcoes do cartaz.
n_juntar=$(rg '^\s*juntar\(' src/ctxmenu.c | rg -vc 'OP_L_|OP_F_')
[ "$n_juntar" -le "$ctx_max" ] || {
  echo "ctxmenu: $n_juntar opcoes possiveis para CTX_MAX=$ctx_max" >&2; exit 1; }
rg -q 'ops\[nOps\]\.rot = rot' src/ctxmenu.c
[ "$(rg -c 'ops\[nOps\]' src/ctxmenu.c)" -eq 1 ] || {
  echo 'ctxmenu: ops[nOps] escrito fora de juntar()' >&2; exit 1; }

# O foco tem de ser animado ate onde o desenho le. Ler focoAnim[i] com i < nOps
# enquanto o laco de animacao vai so ate CTX_MAX foi exatamente o defeito.
rg -q 'for \(i = 0; i < CTX_MAX; i\+\+\)' src/ctxmenu.c

# TIRAR DE CONTINUAR PEDE CONFIRMACAO (dono, 02/10): em aplicar() o desvio
# para a pagina 2 vem ANTES da remocao, e a pagina 2 so chama aplicar() com o
# foco no primeiro botao. O caminho de eventos e coberto em tests/hold.c.
linha_conf=$(rg -n 'acao == OP_TIRAR_CONTINUAR && pagina != 2' src/ctxmenu.c | cut -d: -f1)
linha_tirar=$(rg -n 'desc_tirar_continuar\(imdb, temp, ep\)' src/ctxmenu.c | cut -d: -f1)
[ -n "$linha_conf" ] && [ "$linha_conf" -lt "$linha_tirar" ] || {
  echo 'ctxmenu: tirar de Continuar sem a confirmacao antes' >&2; exit 1; }
# The cancel branch also resets confExtra (confirmation of extra actions, 24edc98e);
# the contract is unchanged: first button applies, anything else goes back to page 0.
rg -q 'if \(confFoco == 0\) aplicar\(\); else \{ pagina = 0; confExtra = -1; \}' src/ctxmenu.c

# A ilha do menu sempre fica fora do cartaz focado: direita quando cabe,
# esquerda como segunda opcao e, sem largura livre, dentro dos limites da tela.
rg -q 'if \(direita <= maxX\) return direita;' src/ctxmenu.c
rg -q 'if \(esquerda >= minX\) return esquerda;' src/ctxmenu.c
rg -q 'if \(maxX - direita >= esquerda - minX\)' src/ctxmenu.c
rg -q 'x = ctxXCartaz\(cartazRect\);' src/ctxmenu.c
if rg -q 'CTX_CARTAO_LARGO|sobreArte' src/ctxmenu.c; then
  echo 'ctxmenu: ilha ancorada sobre a arte do cartaz' >&2; exit 1
fi

echo 'ctxmenu contract: PASS'
