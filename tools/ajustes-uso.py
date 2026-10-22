#!/usr/bin/env python3
"""Quais ajustes as pessoas mais mexem: pessoas DISTINTAS por chave e plataforma.

Le linhas da tabela `registro` do D1 exportadas com wrangler (--json) e conta as
linhas de log "[ajustes] mudou <chave>: <antigo> -> <novo> (origem=...)".
Cada pessoa conta uma vez por chave, por mais que tenha mexido. Rajadas ja saem
juntas do app. Padrao: so origem=ajustes e central (a pessoa mexeu).

  wrangler d1 execute nuvio-recomendacoes --remote --json \
    --command "SELECT pessoa, plataforma, texto FROM registro" > registro.json
  python3 tools/ajustes-uso.py registro.json [--origem ajustes,central,primeira] [--json]
"""
import argparse
import json
import re
import sys
from collections import defaultdict

LINHA = re.compile(r"\[ajustes\] mudou (\S+): (\S+) -> (\S+) \(origem=(\w+)\)")


def linhas_d1(dado):
    """Aceita a saida do wrangler ([{results:[...]}]), uma lista de linhas ou {results:[...]}."""
    if isinstance(dado, dict):
        dado = dado.get("results", [dado])
    saida = []
    for item in dado:
        if isinstance(item, dict) and "results" in item:
            saida.extend(item["results"])
        elif isinstance(item, dict):
            saida.append(item)
    return saida


def contar(linhas, origens):
    pessoas = defaultdict(lambda: defaultdict(set))   # chave -> plataforma -> {pessoa}
    mudancas = defaultdict(int)
    for r in linhas:
        pessoa = r.get("pessoa") or "?"
        plat = r.get("plataforma") or "?"
        for m in LINHA.finditer(r.get("texto") or ""):
            chave, _, _, origem = m.groups()
            if origem not in origens:
                continue
            pessoas[chave][plat].add(pessoa)
            mudancas[chave] += 1
    return pessoas, mudancas


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("arquivo", help="json exportado do wrangler")
    ap.add_argument("--origem", default="ajustes,central,primeira")
    ap.add_argument("--json", action="store_true")
    a = ap.parse_args()
    with open(a.arquivo, encoding="utf-8") as f:
        linhas = linhas_d1(json.load(f))
    pessoas, mudancas = contar(linhas, set(a.origem.split(",")))
    plats = sorted({p for d in pessoas.values() for p in d})
    ordem = sorted(pessoas, key=lambda c: (-len(set().union(*pessoas[c].values())), c))
    if a.json:
        json.dump([{"chave": c, "pessoas": len(set().union(*pessoas[c].values())),
                    "mudancas": mudancas[c], "por_plataforma": {p: len(s) for p, s in pessoas[c].items()}}
                   for c in ordem], sys.stdout, ensure_ascii=False, indent=1)
        print()
        return
    print("%-34s %7s %8s  %s" % ("chave", "pessoas", "mudancas", "  ".join(plats)))
    for c in ordem:
        tot = len(set().union(*pessoas[c].values()))
        print("%-34s %7d %8d  %s" % (c, tot, mudancas[c], "  ".join(str(len(pessoas[c].get(p, ()))) for p in plats)))


if __name__ == "__main__":
    main()
