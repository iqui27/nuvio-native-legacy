#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# O botao "Assistir do comeco" que expande na pagina de detalhe: a pilula
# abre primeiro e o ROTULO entra inteiro depois, nunca cortado pela largura
# em crescimento (o dono viu o texto "sendo escrito na hora"). Ver o .c.
set -eu
cd "$(dirname "$0")/.."
cc tests/expandebotao.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-expandebotao" -O1 -g -Wall
"${TMPDIR:-/tmp}/nuvio-expandebotao"

# The reachable path: the production block, acoesAgrupadas() and
# desenhaSecundario() extracted verbatim from src/detail.c and executed
# (tests/expandebotao_detalhe.c says what it proves and what it does not).
dir=$(mktemp -d "$TMPDIR/nuvio-expandebotao.XXXXXX")
trap 'rm -rf "$dir"' EXIT
python3 tests/expandebotao_extrai.py src/detail.c src/detail.h "$dir"
cc tests/expandebotao_detalhe.c -Isrc -I"$dir" -o "$dir/t" -O1 -g -Wall -Wno-unused-function -lm
"$dir/t"

# O desenho em src/detail.c: rotulo INTEIRO (txt_linha, sem txt_linha_corta
# pela largura) e so com a pilula aberta, ordenado por anim_expande.
grep -q 'anim_expande(&inicioExp, &inicioRot' src/detail.c
grep -q 'txt_linha(TXT_DET_BOTAO,rotIni' src/detail.c
if grep -q 'txt_linha_corta(TXT_DET_BOTAO,rotIni' src/detail.c; then
  echo "FAIL #205: grouped label is clipped by the growing width" >&2; exit 1
fi
echo "ok #205: grouped branch source draws the label unclipped and ordered (branch off: acoesAgrupadas() is 0)"
