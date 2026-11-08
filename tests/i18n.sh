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
# Every diagnostic must survive batches larger than the former 60-error cap.
python3 - <<'PY'
import sys
sys.path.insert(0, "tools")
import idiomas

rows = [(i + 1, "key %03d" % i, "English") for i in range(61)]
idiomas.IDIOMAS = ("ar",)
idiomas.ler_mestra = lambda: rows
idiomas.ler_irma = lambda cod: [(n, key, "") for n, key, _ in rows]
_, errors = idiomas.conferir()
assert len(errors) == 61, "Expected all 61 diagnostics, got %d" % len(errors)
assert errors[-1] == "idioma_ar.h:61: traducao arabe vazia", errors[-1]
print("i18n: all 61 diagnostics retained")
PY
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
   -I/opt/homebrew/include/SDL2 -o ${TMPDIR:-/tmp}/nuvio-idioma-tests \
   -Wno-macro-redefined -Wno-deprecated-declarations
${TMPDIR:-/tmp}/nuvio-idioma-tests
python3 tools/idiomas.py
python3 tools/varredura-i18n.py
# Settings row AJ_OPENSEEK_DESFAZER. The scanner misses it: ACAO() is not a
# draw call, and "Desfazer o ajuste" has no accent and no stopword. This check
# fails if that visible label has no real key in the master or any sibling.
python3 - <<'PY'
import re, sys
sys.path.insert(0, "tools")
import idiomas
CHAVE = "Desfazer o ajuste"
EN = "Undo adjustment"
src = open("src/ajustes.c", encoding="utf-8").read()
assert 'ACAO("%s")' % CHAVE in src, "row label left ajustes.c"
mestra = {pt: en for _, pt, en in idiomas.ler_mestra()}
assert mestra.get(CHAVE) == EN, "master missing %r (got %r)" % (CHAVE, mestra.get(CHAVE))
for cod in idiomas.IDIOMAS:
    val = {pt: v for _, pt, v in idiomas.ler_irma(cod)}.get(CHAVE, "")
    assert val.strip() and val not in (CHAVE, EN), "%s: %r" % (cod, val)
    if cod in idiomas.ESCRITA:
        assert re.search(idiomas.ESCRITA[cod], val), "%s: no script in %r" % (cod, val)
print("i18n: Desfazer o ajuste presente")
PY
echo "i18n: tudo ok"
