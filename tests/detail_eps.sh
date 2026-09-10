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

echo 'detail eps: PASS'
