#!/usr/bin/env python3
"""Gera as trilhas de PREMIOS da Caca a partir do Wikidata (CC0).

Por que Wikidata: e a fonte de dados de premios com licenca livre (CC0) e com
os ids do IMDb (P345) e do TMDB (P4947) de cada filme — a TV casa o "visto"
pelo imdb e o Worker resolve o resto pelo TMDB. Plano social-pessoal, 8.2 [R7].

Saida: servidor/recomendacoes/src/trilhas-dados.js (versionado; o Worker serve
em /v1/trilha?id=...). Rodar de novo depois de cada cerimonia:

    python3 tools/trilhas-wikidata.py

So filmes (tem P4947, o id de FILME no TMDB, e imdb "tt..."): a declaracao de
premio do Wikidata tambem fica nos produtores que receberam a estatueta.
"""
import json
import pathlib
import re
import urllib.parse
import urllib.request

RAIZ = pathlib.Path(__file__).resolve().parent.parent
SAIDA = RAIZ / "servidor" / "recomendacoes" / "src" / "trilhas-dados.js"
UA = "nuvio-native-legacy trilhas (github.com/iqui27/nuvio-native-legacy)"

# id da trilha -> (item do premio no Wikidata, nomes)
PREMIOS = {
    "oscar-melhor-filme": ("Q102427", {
        "pt-BR": "Oscar de Melhor Filme", "pt": "Óscar de Melhor Filme",
        "en": "Academy Award for Best Picture", "es": "Óscar a la mejor película",
        "fr": "Oscar du meilleur film", "de": "Oscar für den besten Film",
        "it": "Oscar al miglior film"}),
    "oscar-filme-internacional": ("Q105304", {
        "pt-BR": "Oscar de Melhor Filme Internacional", "pt": "Óscar de Melhor Filme Internacional",
        "en": "Academy Award for Best International Feature", "es": "Óscar a la mejor película internacional",
        "fr": "Oscar du meilleur film international", "de": "Oscar für den besten internationalen Film",
        "it": "Oscar al miglior film internazionale"}),
}

CONSULTA = """
SELECT ?f ?fLabel ?imdb ?tmdb (MIN(?d) AS ?data) WHERE {
  ?f p:P166 ?s . ?s ps:P166 wd:%s .
  ?f wdt:P345 ?imdb . ?f wdt:P4947 ?tmdb .
  OPTIONAL { ?f wdt:P577 ?d }
  SERVICE wikibase:label { bd:serviceParam wikibase:language "en". }
} GROUP BY ?f ?fLabel ?imdb ?tmdb ORDER BY ?data
"""


def consultar(q):
    url = "https://query.wikidata.org/sparql?" + urllib.parse.urlencode({"query": q})
    req = urllib.request.Request(url, headers={"Accept": "application/sparql-results+json", "User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.load(r)["results"]["bindings"]


def main():
    trilhas = {}
    for tid, (premio, nomes) in PREMIOS.items():
        vistos, filmes = set(), []
        for b in consultar(CONSULTA % premio):
            imdb = b["imdb"]["value"]
            if not imdb.startswith("tt") or imdb in vistos:
                continue
            vistos.add(imdb)
            m = re.match(r"\+?(\d{4})-(\d{2})-(\d{2})", b.get("data", {}).get("value", ""))
            filmes.append({"tmdb": int(b["tmdb"]["value"]), "imdb": imdb,
                           "titulo": b["fLabel"]["value"],
                           "data": int("".join(m.groups())) if m else 0})
        filmes.sort(key=lambda f: f["data"])
        trilhas[tid] = {"id": tid, "tipo": "trilha", "nome": nomes, "ordem": "lancamento", "filmes": filmes}
        print(f"{tid}: {len(filmes)} filmes")
    SAIDA.write_text(
        "// GERADO por tools/trilhas-wikidata.py a partir do Wikidata (CC0). Nao editar a mao.\n"
        "export const TRILHAS_PREMIOS = " + json.dumps(trilhas, ensure_ascii=False, indent=1) + ";\n",
        encoding="utf-8")


if __name__ == "__main__":
    main()
