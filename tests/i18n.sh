#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A TABELA RESPONDE, ESTA ORDENADA, E NAO FALTA NINGUEM.
#
# Sao tres perguntas diferentes e o issue #12 tropecou nas tres em rodadas
# distintas: chave ausente (texto em portugues na tela), tabela fora de ordem
# (busca binaria erra calada) e frase montada com snprintf (a string final
# nunca casa com chave). tests/idioma.c cobre as duas primeiras;
# tools/varredura-i18n.py cobre a terceira e reencontra a primeira sozinha.
#
# OS 28 IDIOMAS DE TABELA (ro, uk, ru, fr, de, es, it, nl, pl, tr, pt-PT, sv, da, no,
# cs, sk, sl, hu, lt, bs, sr, bg, el, id, vi, ja, zh-CN, zh-TW) tem um terceiro
# guardiao, o de TEXTO:
# tools/idiomas.py confere, em cada tabela irma, a mesma quantidade de linhas, a
# ordem da mestra, a chave de cada linha, os mesmos marcadores printf na mesma
# ordem, nenhum valor vazio e os mesmos \n. O tests/idioma.c repete o essencial
# pelo caminho que o app usa (i18n), varrendo as chaves nos 29 idiomas.
set -eu
cd "$(dirname "$0")/.."
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
   -I/opt/homebrew/include/SDL2 -o ${TMPDIR:-/tmp}/nuvio-idioma-tests \
   -Wno-macro-redefined -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-idioma-tests
python3 tools/idiomas.py
python3 tools/varredura-i18n.py
echo "i18n: tudo ok"
