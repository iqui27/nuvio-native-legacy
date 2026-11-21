#!/bin/bash
# Farol: toda fala FT_* e chave da mestra, tem traducao nao vazia nas 29 irmas,
# cabe em 2 linhas de 62 caracteres e foge da lista "nunca diz" do ALMA.
# O C cobre chave, largura, a lista proibida e a aceitacao pelo filtro
# faroltexto_aceita() (issue 35); o python cobre as irmas e o
# idiomas.py cobre ordem, marcadores, escrita e cobertura de glifo.
set -eu
cd "$(dirname "$0")/.."
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-textos.XXXXXX")"
bin="$dir/t"
trap 'rm -rf "$dir"' EXIT
cc -Wall -Wextra -O1 -g tests/farol_textos.c src/idioma.c src/faroltexto.c -Isrc \
   -I/opt/homebrew/include -o "$bin"
"$bin"
python3 - <<'PY'
import re, sys
sys.path.insert(0, "tools")
import idiomas

fonte = open("src/faroltexto.h", encoding="utf-8").read()
chaves = {s for _, s in re.findall(
    r'#define\s+(FT_\w+)\s+"((?:[^"\\]|\\.)*)"', fonte)}
assert chaves, "faroltexto.h sem defines"

ingles = {pt: en for _, pt, en in idiomas.ler_mestra()}
falhas = 0
for cod in idiomas.IDIOMAS:
    valores = {pt: v for _, pt, v in idiomas.ler_irma(cod)}
    for pt in chaves:
        val = valores.get(pt)
        if val is None:
            print("FALHOU: idioma_%s.h sem a chave %r" % (cod, pt)); falhas += 1
        elif not val.strip():
            print("FALHOU: idioma_%s.h chave %r vazia" % (cod, pt)); falhas += 1
        elif cod in idiomas.ESCRITA and not re.search(idiomas.ESCRITA[cod], val) \
             and val not in idiomas.VERBATIM and val != pt and val != ingles.get(pt, ""):
            print("FALHOU: idioma_%s.h %r sem a escrita da lingua" % (cod, pt)); falhas += 1
if falhas:
    print("farol_textos: %d falha(s) nas irmas" % falhas); sys.exit(1)
print("farol_textos: %d chaves traduzidas em %d idiomas" % (len(chaves), len(idiomas.IDIOMAS)))
PY
python3 tools/idiomas.py
echo "farol_textos: tudo ok"
