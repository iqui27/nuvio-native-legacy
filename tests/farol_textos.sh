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
import pathlib, re, sys
sys.path.insert(0, "tools")
import idiomas

fonte = open("src/faroltexto.h", encoding="utf-8").read()
definidos = dict(re.findall(
    r'#define\s+(FT_\w+)\s+"((?:[^"\\]|\\.)*)"', fonte))
assert definidos, "faroltexto.h sem defines"
chaves = set(definidos.values())

# Prova que toda constante FT_* usada no codigo fonte existe em src/faroltexto.h
# (excluindo o enum FarolTipo de farolredator.h e o vetor interno FT_PROIBIDAS).
nao_chave = {"FT_INT", "FT_NUM1", "FT_BOOL", "FT_TOKEN", "FT_ENUM",
             "FT_VERSAO", "FT_HASH", "FT_OBJETO", "FT_ARRAY", "FT_PROIBIDAS"}
usados_falta = []
for arq in sorted(pathlib.Path("src").glob("*")):
    if arq.suffix not in (".c", ".h", ".inc") or arq.name == "faroltexto.h":
        continue
    txt = arq.read_text(encoding="utf-8", errors="replace")
    txt = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), txt, flags=re.S)
    for idx, linha in enumerate(txt.splitlines(), 1):
        cod = re.sub(r'//.*$', '', linha)
        for nome in re.findall(r'\b(FT_[A-Z0-9_]+)\b', cod):
            if nome in nao_chave or nome.endswith("_"):
                continue
            if nome not in definidos:
                usados_falta.append(f"{arq}:{idx}: {nome}")
if usados_falta:
    for u in usados_falta:
        print("FALHOU: constante usada sem definicao em faroltexto.h:", u)
    sys.exit(1)

# Prova que nao sobrou nenhum marcador de texto pendente na arvore (src/ e tests/).
marcador = "FALTA_" + "TEXTO"
sobras = []
for pasta in ("src", "tests"):
    for arq in sorted(pathlib.Path(pasta).rglob("*")):
        if not arq.is_file() or arq.name == "farol_textos.sh":
            continue
        if arq.suffix not in (".c", ".h", ".inc", ".sh", ".py", ".mjs", ".cjs"):
            continue
        txt = arq.read_text(encoding="utf-8", errors="replace")
        for idx, linha in enumerate(txt.splitlines(), 1):
            if marcador in linha:
                sobras.append(f"{arq}:{idx}")
if sobras:
    for s in sobras:
        print("FALHOU: marcador residual encontrado em", s)
    sys.exit(1)

ingles = {pt: en for _, pt, en in idiomas.ler_mestra()}
falhas = 0
for pt in sorted(chaves):
    if pt not in ingles or not ingles[pt].strip():
        print("FALHOU: idioma_tab.h sem a chave %r" % (pt,)); falhas += 1
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
print("farol_textos: %d chaves traduzidas em %d idiomas; 0 marcadores pendentes" % (len(chaves), len(idiomas.IDIOMAS)))
PY
python3 tools/idiomas.py
echo "farol_textos: tudo ok"
