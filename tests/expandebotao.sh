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

# O desenho em src/detail.c: rotulo INTEIRO (txt_linha, sem txt_linha_corta
# pela largura) e so com a pilula aberta, ordenado por anim_expande.
grep -q 'anim_expande(&inicioExp, &inicioRot' src/detail.c
grep -q 'txt_linha(TXT_DET_BOTAO,rotIni' src/detail.c
! grep -q 'txt_linha_corta(TXT_DET_BOTAO,rotIni' src/detail.c
echo "ok #205: detail.c draws the expanding label unclipped and ordered"
