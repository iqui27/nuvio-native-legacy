#!/usr/bin/env python3
"""Insere as chaves do Comecar (2.1) nas 29 tabelas de traducao.

As chaves ainda nao existem na mestra: este script recebe um JSON
{chave: {"en": ingles, <idioma>: traducao, ...}}, insere cada chave na posicao
correta (ordem = bytes UTF-8 decodificados, a mesma que o idioma.c compara) na
mestra e na mesma posicao nas irmas. Linguas sem traducao no JSON recebem o
ingles — aceito pelo conferidor (valor == ingles) e marcado na revisao.
Roda uma vez e sai do repositorio.
"""
import json, pathlib, re, sys

RAIZ = pathlib.Path(__file__).resolve().parent.parent
SRC = RAIZ / "src"
IDIOMAS = ("ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr", "ptpt", "sv",
           "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr", "bg", "el", "id",
           "vi", "ja", "zhcn", "zhtw")

def dec(s):
    return s.replace('\\"', '"').replace("\\n", "\n").replace("\\\\", "\\").encode("utf-8")

def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")

CHAVE_MESTRA = re.compile(r'^\s*\{\s*"((?:[^"\\]|\\.)*)"')
CHAVE_IRMA = re.compile(r'^\s*T\(\s*"((?:[^"\\]|\\.)*)"')

def chave_de(l, rx):
    m = rx.match(l)
    return m.group(1) if m and not l.lstrip().startswith("//") else None

def inserir(caminho, rx, fmt_linha, dados, novas, trad):
    linhas = caminho.read_text(encoding="utf-8").splitlines()
    out, fila = [], sorted(novas, key=dec)
    for l in linhas:
        k = chave_de(l, rx)
        if k:
            while fila and dec(k) > dec(fila[0]):
                kk = fila.pop(0)
                out.append(fmt_linha(kk, trad(kk)))
        out.append(l)
    pos = max((i for i, l in enumerate(out) if chave_de(l, rx)), default=len(out)) + 1
    for kk in fila: out.insert(pos, fmt_linha(kk, trad(kk)))
    caminho.write_text("\n".join(out) + "\n", encoding="utf-8")

dados = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
mestra = SRC / "idioma_tab.h"
existentes = {chave_de(l, CHAVE_MESTRA) for l in mestra.read_text(encoding="utf-8").splitlines()}
existentes.discard(None)
novas = [k for k in dados if k not in existentes]
if not novas:
    print("nada a inserir"); sys.exit(0)

inserir(mestra, CHAVE_MESTRA,
        lambda k, v: '  { "%s", "%s" },' % (esc(k), esc(v)),
        dados, novas, lambda k: dados[k]["en"])
for cod in IDIOMAS:
    inserir(SRC / ("idioma_%s.h" % cod), CHAVE_IRMA,
            lambda k, v, c=cod: '  T("%s", "%s"),' % (esc(k), esc(v)),
            dados, novas, lambda k, c=cod: dados[k].get(c) or dados[k]["en"])
print("inseridas %d chaves em 29 tabelas" % len(novas))
