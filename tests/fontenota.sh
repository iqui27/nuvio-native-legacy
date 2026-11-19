#!/bin/bash
# Evidencia de qualidade da fonte (Code Geass S1E3 e cia). Ver tests/fontenota.c.
#
#   bash tests/fontenota.sh
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/fontenota.c src/fonteauto.c src/linguas.c tests/fontenota.c \
   -o "${TMPDIR:-/tmp}/nuvio-fontenota-tests"
"${TMPDIR:-/tmp}/nuvio-fontenota-tests"

# CONTRATO com streams.c: a nota entra na pontuacao pelos dois caminhos, e o
# log da escolha explica a vencedora.
grep -q 'fontenota_somar(q, &nt, 1)' src/streams.c || { echo "fontenota: qualidade() nao soma a evidencia"; exit 1; }
grep -q 'fontenota_somar(p, &nt, 0)' src/streams.c || { echo "fontenota: Comecar rapido perdeu idioma/compatibilidade"; exit 1; }
grep -q 'auto pick evidence' src/streams.c || { echo "fontenota: log da escolha sem a explicacao"; exit 1; }
grep -q 'automaticoDaFolhaModo' src/streams.c || { echo "fontenota: selo da folha ignora Primeira da lista"; exit 1; }
grep -q 'idiomaDeclaradoNota(s, idioma' src/streams.c || { echo "fontenota: idioma ignora o audio declarado pelo addon"; exit 1; }
echo "fontenota: contrato de streams.c ok"
