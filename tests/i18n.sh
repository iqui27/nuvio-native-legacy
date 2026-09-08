#!/bin/bash
# A TABELA RESPONDE, ESTA ORDENADA, E NAO FALTA NINGUEM.
#
# Sao tres perguntas diferentes e o issue #12 tropecou nas tres em rodadas
# distintas: chave ausente (texto em portugues na tela), tabela fora de ordem
# (busca binaria erra calada) e frase montada com snprintf (a string final
# nunca casa com chave). tests/idioma.c cobre as duas primeiras;
# tools/varredura-i18n.py cobre a terceira e reencontra a primeira sozinha.
set -eu
cd "$(dirname "$0")/.."
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
   -I/opt/homebrew/include/SDL2 -o /tmp/nuvio-idioma-tests \
   -Wno-macro-redefined -Wno-deprecated-declarations
/tmp/nuvio-idioma-tests
python3 tools/varredura-i18n.py
echo "i18n: tudo ok"
