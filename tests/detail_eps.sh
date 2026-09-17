#!/bin/sh
# A FILEIRA DE EPISODIOS MOSTRA UMA TEMPORADA SO, e a coluna dela e RELATIVA.
#
# Issue #35: a fileira emendava todas as temporadas e a aba era so um atalho de
# rolagem, entao passar de T1E4 para a direita caia em T2E1 sem que a aba
# dissesse nada. Agora a aba filtra, e com isso nasceu uma armadilha nova:
# `foco.coluna` conta dentro da temporada em exibicao e `cat_episodio()` conta
# dentro da serie inteira. Misturar os dois nao quebra a compilacao e nao
# quebra nenhum teste de logica — mostra o episodio ERRADO no card, que e o
# tipo de defeito que so aparece em foto.
#
#   bash tests/detail_eps.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

cc -fsyntax-only src/detail.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined

# Ninguem le a fileira com a coluna crua. As duas formas abaixo sao exatamente
# as que existiam antes do conserto.
if rg -q 'cat_episodio\(idx, foco\.coluna\)' src/detail.c; then
  echo 'detail: cat_episodio(idx, foco.coluna) e indice RELATIVO em vetor ABSOLUTO' >&2
  exit 1
fi
if rg -q 'cat_episodio\(idx, c\)' src/detail.c; then
  echo 'detail: cat_episodio(idx, c) e indice RELATIVO em vetor ABSOLUTO' >&2
  exit 1
fi

# E a contagem da secao conta o que a fileira mostra, nao a serie inteira: com
# cat_n_episodios aqui o foco andaria por colunas que nao existem.
rg -q 'case SEC_EPISODIOS: \{\s*$' src/detail.c
rg -q 'int q = epVisiveis\(\);' src/detail.c
if rg -q 'int q = cat_n_episodios\(idx\);' src/detail.c; then
  echo 'detail: secaoN conta a serie inteira, nao a temporada em exibicao' >&2
  exit 1
fi

# O desenho do card tambem passa pelo mapeamento.
rg -q 'const CatEp \*ep = cat_episodio\(idx, epAbsoluto\(c\)\);' src/detail.c

# Issue #43: abrir pela fileira "Continuar assistindo" tinha de acender a aba
# da temporada em PROGRESSO (S2E2 numa serie de 4 temporadas, por exemplo), e
# nao a do primeiro episodio da serie (S1E1). Sem isto detail_abrir escolhia
# sempre a temporada de cat_episodio(idx, 0) e ignorava ci->temporada.
if ! rg -q 'ci0->progresso > 0 && ci0->progresso < 90 &&' src/detail.c; then
  echo 'detail: temporada inicial ignora o progresso de Continuar assistindo' >&2
  exit 1
fi

# E a coluna tinha de nascer na posicao RELATIVA do episodio em progresso
# dentro da temporada, nao em 0 — senao o card de abertura mostrava T2E1 no
# lugar de T2E2.
if ! rg -q 'foco\.colunaLembrada\[SEC_EPISODIOS\]\s*=\s*epAncora;' src/detail.c; then
  echo 'detail: coluna inicial da fileira de episodios nao usa a posicao do progresso' >&2
  exit 1
fi

# Descer do hero nao pode jogar fora essa memoria. O defeito relatado na
# issue: "pressing down to select which season returns me to season 1
# episode 1" — porque este trecho cravava foco.coluna em 0 na fileira de
# temporadas, e o sincronizador de detail_atualizar lia esse 0 como "usuario
# escolheu a Temporada 1" e reescrevia `temporada` por cima do valor certo.
if rg -q 'foco\.fileira = r; foco\.coluna = 0; nivel = 1;' src/detail.c; then
  echo 'detail: descer do hero zera a coluna e perde a temporada do progresso' >&2
  exit 1
fi
rg -q 'alvo = foco\.colunaLembrada\[r\];' src/detail.c

# --- EPISODIO QUE AINDA NAO FOI AO AR ---------------------------------------
#
# A lista desenhava um episodio nao exibido exatamente como um que voce so nao
# viu — o unico sinal era a AUSENCIA do selo de nota do Trakt, que e tambem o
# estado de uma serie que ninguem avaliou. Duas coisas diferentes com o mesmo
# desenho, e o card convidava a abrir o que nao existe.
#
# A fonte tem de continuar sendo a agenda do TMDB, que ja vem no corpo que a
# pagina baixa. Qualquer tentativa de deduzir pela DATA do episodio volta a
# esbarrar no CatEp.data, que chega formatado por extenso e passa por i18n.
rg -q 'static int epNaoExibido\(const CatEp \*ep\)' src/detail.c
rg -q 'extras_agenda_temporada\(\).*extras_agenda_episodio\(\)' src/detail.c
# SEM AGENDA NAO SE AFIRMA NADA: 0 e "nao sabemos", e ai o episodio e comum.
if ! rg -q 'if \(!ep \|\| t <= 0 \|\| e <= 0\) return 0;' src/detail.c; then
  echo 'detail: epNaoExibido afirma "nao exibido" sem a agenda do TMDB' >&2
  exit 1
fi

# --- RESUMO DA TEMPORADA -----------------------------------------------------
#
# A contagem de assistidos so pode sair quando o mapa DESTA serie existe.
# vistoep separa "nao viu" de "nao sabemos" (vistoep.h), e escrever "0
# assistidos" enquanto o Trakt nao respondeu e afirmar sobre o que ninguem
# contou — o mesmo defeito que o card ja evita ao nao desenhar selo de "nao
# assistido" durante a consulta.
rg -q 'vistoep_conhecido\(ci->imdb\)' src/detail.c
if ! rg -q 'if \(sabe\)' src/detail.c; then
  echo 'detail: o resumo da temporada conta assistidos sem saber se ha mapa' >&2
  exit 1
fi

echo 'detail eps: PASS'
