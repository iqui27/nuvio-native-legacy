#!/usr/bin/env python3
"""O MELHOR CENARIO PARA TODO MUNDO: agrega os relatorios do diagnostico.

Le os relatorios `diagnostico=v1|v2` que as TVs mandaram (tabela `registro`
do D1 nuvio-recomendacoes, via wrangler, SO LEITURA) e agrega por plataforma e
faixa de RAM — as mesmas faixas da tabela de src/perfiltv.c:

  - tempo medio por arte de cada fonte de arte (consulta + download), falhas
    e em quantos titulos ela repetiu a imagem do card (igual_ao_card);
  - perfil de textura/fios/heroi: o que valia, o candidato e o que aconteceu
    (aplicado, mantido, restaurado, ja no perfil);
  - e PROPOE a linha da tabela de perfiltv.c e a fonte padrao do destaque por
    plataforma (a mais rapida entre as que nao repetem o card).

Com poucos relatorios a proposta diz isso em vez de inventar numero: a regra
e a mesma de sempre no app, "nao afirmar sem medida".

  python3 tools/diag-agregado.py                 # consulta o D1 (wrangler)
  python3 tools/diag-agregado.py --arquivo x.json  # le um dump local
  python3 tools/diag-agregado.py --json          # saida em JSON

Nunca imprime `pessoa`, token, nem a lista de addons: so plataforma, versao,
RAM e numeros. O wrangler usa o login proprio dele; nada de credencial passa
por aqui.
"""
import argparse
import json
import os
import subprocess
import sys
from collections import defaultdict

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVIDOR = os.path.join(RAIZ, "servidor", "recomendacoes")
# `pessoa` vem so para CONTAR aparelhos distintos (vira hash aqui e nunca e
# impresso): tres relatorios da mesma TV nao sao "todo mundo".
CONSULTA = ("SELECT id, pessoa, plataforma, versao, criado, texto FROM registro "
            "WHERE texto LIKE 'diagnostico=v%' ORDER BY id")

# Fontes de fundo, na ordem do contrato ARTEHERO_* (o logo nao e fundo).
FUNDOS = ["catalog", "metahub", "tmdb", "trakt", "apple", "fanart", "anime", "tmdb_outro"]
AJUSTE_DE = {"catalog": 1, "metahub": 2, "tmdb": 3, "trakt": 4, "apple": 5,
             "fanart": 6, "anime": 7, "tmdb_outro": 3}
ROTULO_AJUSTE = {0: "Automatico", 1: "Catalogo", 2: "Metahub", 3: "TMDB", 4: "Trakt",
                 5: "Apple TV", 6: "fanart.tv", 7: "Anime"}
MIN_APARELHOS = 3


def tabela_atual(plataforma, mem):
    """Replica de ptv_tex_auto_mb/ptv_tex_teto_mb/ptv_padrao (src/perfiltv.c)."""
    tizen = plataforma.lower().startswith("tizen") or plataforma.lower() == "samsung"
    if tizen:
        auto = 96 if not mem else 64 if mem <= 1024 else 96 if mem < 4096 else 128
        return {"tex_mb": auto, "teto_mb": auto, "fios_rede": 2, "heroi": 1280}
    if not mem:
        return {"tex_mb": 96, "teto_mb": 160, "fios_rede": 4, "heroi": 1920}
    auto = 48 if mem < 800 else 64 if mem < 1200 else 96 if mem < 2000 else 128 if mem < 3000 else 192
    teto = 96 if mem < 1200 else 160 if mem < 2000 else 300 if mem < 3000 else 512
    return {"tex_mb": auto, "teto_mb": teto, "fios_rede": 2 if mem < 1200 else 4,
            "heroi": 1280 if mem < 1200 else 1920}


def faixa(plataforma, mem):
    """A faixa de RAM de perfiltv.c (ptv_tex_auto_mb)."""
    tizen = plataforma.lower().startswith("tizen") or plataforma.lower() == "samsung"
    if not mem:
        return ("tizen" if tizen else "lg") + " sem RAM"
    if tizen:
        return "tizen <=1GB" if mem <= 1024 else "tizen <4GB" if mem < 4096 else "tizen >=4GB"
    if mem < 800:
        return "lg <800MB"
    if mem < 1200:
        return "lg <1,2GB"
    if mem < 2000:
        return "lg <2GB"
    if mem < 3000:
        return "lg <3GB"
    return "lg >=3GB"


def ler_d1():
    cmd = ["npx", "--yes", "wrangler", "d1", "execute", "nuvio-recomendacoes",
           "--remote", "--json", "--command", CONSULTA]
    p = subprocess.run(cmd, cwd=SERVIDOR, capture_output=True, text=True, timeout=180)
    if p.returncode != 0:
        # stderr do wrangler pode ter caminho de config; so a ultima linha.
        ult = (p.stderr.strip().splitlines() or ["?"])[-1]
        sys.exit("diag-agregado: wrangler falhou: %s" % ult[:200])
    saida = p.stdout[p.stdout.find("["):]
    return json.loads(saida)[0]["results"]


def ler_arquivo(caminho):
    d = json.load(open(caminho, encoding="utf-8"))
    if isinstance(d, list) and d and "results" in d[0]:
        return d[0]["results"]
    return d


def campos_pipe(resto):
    """'catalog|ok=3|falhas=0' -> ('catalog', {'ok': '3', ...})"""
    partes = resto.split("|")
    kv = {}
    for p in partes[1:]:
        if "=" in p:
            k, v = p.split("=", 1)
            kv[k] = v
        elif ":" in p:
            k, v = p.split(":", 1)
            kv[k] = v
    return partes[0], kv


def num(v, padrao=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return padrao


def analisar(texto):
    r = {"fontes": {}, "sugestao": None}
    for linha in texto.splitlines():
        if "=" not in linha:
            continue
        k, v = linha.split("=", 1)
        if k == "arte_fonte":
            nome, kv = campos_pipe(v)
            r["fontes"][nome] = kv
        elif k == "sugestao_destaque":
            if v != "-":
                # alvo:x|diferente:1|lenta:y|ms_lenta=1|base:z|ms_base=2|motivo:m
                kv = {}
                for p in v.split("|"):
                    sep = ":" if ":" in p and ("=" not in p or p.index(":") < p.index("=")) else "="
                    if sep in p:
                        a, b = p.split(sep, 1)
                        kv[a] = b
                r["sugestao"] = kv
        elif k in ("perfil_antes", "perfil_candidato"):
            r[k] = [num(x) for x in v.split("|")]
        elif k == "addon":
            continue          # lista de addons da pessoa: fora do agregado
        else:
            r[k] = v
    return r


def agregar(linhas):
    grupos = defaultdict(lambda: {
        "n": 0, "versoes": set(), "ids": [],
        "fontes": defaultdict(lambda: {"ok": 0, "falhas": 0, "ms": 0, "iguais": 0,
                                       "medidas": 0, "relatorios": 0, "bytes": 0,
                                       "larg": 0, "alt": 0}),
        "aplicacao": defaultdict(int), "perfil_antes": [], "perfil_candidato": [],
        "tex_bytes": [], "tex_limite": [], "tex_pendentes": [], "tex_quentes": [],
        "artes_antes_ms": [], "artes_depois_ms": [], "pior_quadro_antes_ms": [],
        "destaque": defaultdict(int), "sugestoes": [], "modos": defaultdict(int),
        "aparelhos": set(), "mem": [], "despejos": [],
    })
    for linha in linhas:
        txt = linha.get("texto") or ""
        if not txt.startswith("diagnostico=v"):
            continue
        r = analisar(txt)
        plat = (linha.get("plataforma") or "?").strip() or "?"
        mem = num(r.get("mem_total_mb"))
        g = grupos[(plat, faixa(plat, mem))]
        g["n"] += 1
        g["aparelhos"].add(hash(linha.get("pessoa") or linha.get("id")))
        if mem:
            g["mem"].append(mem)
        g["despejos"].append(num(r.get("despejos_quentes_depois")) + num(r.get("tex_quentes")))
        g["versoes"].add(linha.get("versao") or r.get("versao") or "?")
        g["ids"].append(linha.get("id"))
        g["modos"][r.get("modo", "?")] += 1
        g["aplicacao"][r.get("aplicacao", "?")] += 1
        for k in ("perfil_antes", "perfil_candidato"):
            if k in r:
                g[k].append(r[k])
        for k in ("tex_bytes", "tex_limite", "tex_pendentes", "tex_quentes",
                  "artes_antes_ms", "artes_depois_ms", "pior_quadro_antes_ms"):
            if k in r:
                g[k].append(num(r[k]))
        if "destaque_fonte" in r:
            g["destaque"]["%s/%s" % (r["destaque_fonte"], r.get("destaque_diferente", "?"))] += 1
        if r["sugestao"]:
            g["sugestoes"].append(r["sugestao"])
        for nome, kv in r["fontes"].items():
            f = g["fontes"][nome]
            ok, fal = num(kv.get("ok")), num(kv.get("falhas"))
            if not ok and not fal:
                continue          # nao medida neste aparelho (sem chave, sem anime...)
            f["relatorios"] += 1
            f["ok"] += ok
            f["falhas"] += fal
            f["ms"] += num(kv.get("resolve_ms")) + num(kv.get("download_ms"))
            f["bytes"] += num(kv.get("bytes"))
            f["larg"] = max(f["larg"], num(kv.get("largura")))
            f["alt"] = max(f["alt"], num(kv.get("altura")))
            # v2 antigo nao tem iguais/igual_ao_card: fica "nao se sabe" (-1).
            if "iguais" in kv or "igual_ao_card" in kv:
                f["medidas"] += 1
                f["iguais"] += num(kv.get("iguais"), num(kv.get("igual_ao_card")))
    return grupos


def media(v):
    return sum(v) / len(v) if v else None


def propor(g, plat):
    """Proposta de linha da tabela de perfiltv.c e da fonte do destaque."""
    prop = {}
    n = g["n"]
    # Fonte do destaque: mais rapida por arte, sem falha majoritaria.
    candidatas = []
    for nome in FUNDOS:
        f = g["fontes"].get(nome)
        if not f or f["ok"] <= 0:
            continue
        ms = f["ms"] / f["ok"]
        taxa = f["falhas"] / (f["ok"] + f["falhas"])
        candidatas.append((ms, nome, taxa, f))
    candidatas.sort()
    mesma = [c for c in candidatas if c[2] < 0.5]
    diferentes = [c for c in mesma if c[3]["medidas"] > 0 and c[3]["iguais"] == 0]
    prop["destaque_mesma_foto"] = mesma[0][1] if mesma else None
    prop["destaque_outra_arte"] = diferentes[0][1] if diferentes else None
    if not any(c[3]["medidas"] for c in mesma):
        prop["destaque_outra_arte_obs"] = ("nenhum relatorio com igual_ao_card ainda "
                                           "(so a versao nova mede): sem proposta")
    # Perfil de textura. Um cache ENCHE ate o orcamento (a C9 com 300 MB
    # chega a ~200 MB numa sessao longa), entao o pico nao e necessidade. O
    # sinal de que falta e arte VISIVEL despejada (tex_quentes /
    # despejos_quentes_depois): sem nenhuma, a tabela fica; com alguma, um
    # degrau acima, dentro do teto da faixa.
    tab = tabela_atual(plat, max(g["mem"]) if g["mem"] else 0)
    prop["tabela_atual"] = tab
    usados = [b / 1048576.0 for b in g["tex_bytes"] if b]
    if usados:
        prop["tex_pico_mb"] = round(max(usados), 1)
    escada = [48, 64, 96, 128, 160, 192, 240, 300, 400, 512]
    if any(g["despejos"]):
        acima = [e for e in escada if e > tab["tex_mb"] and e <= tab["teto_mb"]]
        prop["tex_mb"] = acima[0] if acima else tab["tex_mb"]
        prop["tex_motivo"] = "arte visivel despejada em %d relatorio(s)" % sum(1 for x in g["despejos"] if x)
    else:
        prop["tex_mb"] = tab["tex_mb"]
        prop["tex_motivo"] = "nenhuma arte visivel despejada em %d relatorio(s)" % n
    # Nomes de aplicacaoNome() em diagnostico.c.
    aplicados = g["aplicacao"].get("aplicada", 0)
    restaurados = sum(v for k, v in g["aplicacao"].items() if k.startswith("restaurada"))
    iguais = g["aplicacao"].get("ja_no_perfil", 0)
    if not aplicados and not restaurados:
        prop["perfil"] = ("manter a tabela (candidato igual ao perfil em %d de %d)" % (iguais, n)
                          if iguais else "manter a tabela (nenhum candidato testado)")
    else:
        prop["perfil"] = "candidato mantido em %d, restaurado em %d de %d" % (aplicados, restaurados, n)
    if g["perfil_antes"]:
        ult = g["perfil_antes"][-1]
        prop["perfil_visto"] = {"tex_mb": ult[0], "fios_rede": ult[1], "heroi": ult[2]}
    prop["aparelhos"] = len(g["aparelhos"])
    prop["suficiente"] = len(g["aparelhos"]) >= MIN_APARELHOS
    return prop


def imprimir(grupos):
    total = sum(g["n"] for g in grupos.values())
    print("relatorios diagnostico: %d em %d grupo(s) (plataforma x RAM)" % (total, len(grupos)))
    if not total:
        return
    for (plat, fx), g in sorted(grupos.items()):
        print()
        print("== %s | %s | %d relatorio(s) de %d aparelho(s) | versoes %s | ids %s" % (
            plat, fx, g["n"], len(g["aparelhos"]), ",".join(sorted(g["versoes"])),
            ",".join(str(i) for i in g["ids"][-6:])))
        print("  modos: %s | aplicacao: %s" % (dict(g["modos"]), dict(g["aplicacao"])))
        if g["perfil_antes"]:
            print("  perfil antes (tex|fios|heroi): %s" % ["|".join(map(str, p)) for p in g["perfil_antes"]])
            print("  perfil candidato:              %s" % ["|".join(map(str, p)) for p in g["perfil_candidato"]])
        if g["tex_bytes"]:
            print("  texturas: pico %.1f MB de %.0f MB | pendentes %s | quentes %s" % (
                max(g["tex_bytes"]) / 1048576.0,
                max(g["tex_limite"] or [0]) / 1048576.0,
                max(g["tex_pendentes"] or [0]), max(g["tex_quentes"] or [0])))
        if g["artes_antes_ms"]:
            print("  artes pelo cache (quente): antes %.0f ms, depois %.0f ms, pior quadro %.0f ms" % (
                media(g["artes_antes_ms"]), media([x for x in g["artes_depois_ms"] if x]) or 0,
                media(g["pior_quadro_antes_ms"]) or 0))
        print("  destaque (fonte/diferente): %s" % dict(g["destaque"]))
        print("  %-11s %9s %7s %8s %10s %10s %s" % ("fonte", "ms/arte", "ok", "falhas", "KB/arte", "maior", "igual ao card"))
        for nome in FUNDOS + ["logo"]:
            f = g["fontes"].get(nome)
            if not f or (not f["ok"] and not f["falhas"]):
                continue
            ms = "%.0f" % (f["ms"] / f["ok"]) if f["ok"] else "-"
            kb = "%.0f" % (f["bytes"] / f["ok"] / 1024.0) if f["ok"] else "-"
            igual = ("%d titulo(s) em %d rel." % (f["iguais"], f["medidas"])) if f["medidas"] else "nao medido"
            if nome == "logo":
                igual = "-"
            print("  %-11s %9s %7d %8d %10s %10s %s" % (nome, ms, f["ok"], f["falhas"], kb,
                                                     "%dx%d" % (f["larg"], f["alt"]) if f["larg"] else "-", igual))
        for s in g["sugestoes"]:
            print("  sugestao enviada: %s" % "|".join("%s:%s" % kv for kv in s.items()))
        p = propor(g, plat)
        print("  PROPOSTA%s:" % ("" if p["suficiente"] else
              " (%d aparelho(s): indicativa, nao mude a tabela antes de %d)" % (p["aparelhos"], MIN_APARELHOS)))
        tab = p["tabela_atual"]
        print("    perfiltv.c hoje nesta faixa: textura %d MB (teto %d), %d fios, heroi %d" % (
            tab["tex_mb"], tab["teto_mb"], tab["fios_rede"], tab["heroi"]))
        print("    perfiltv.c proposto: textura %d MB, %d fios, heroi %d — %s%s" % (
            p["tex_mb"], tab["fios_rede"], tab["heroi"], p["tex_motivo"],
            "" if "tex_pico_mb" not in p else "; pico usado %.1f MB" % p["tex_pico_mb"]))
        if p.get("perfil_visto"):
            print("    perfil em vigor no aparelho: %d MB, %d fios, heroi %d" % (
                p["perfil_visto"]["tex_mb"], p["perfil_visto"]["fios_rede"], p["perfil_visto"]["heroi"]))
        print("    perfil: %s" % p["perfil"])
        ms = p["destaque_mesma_foto"]
        print("    destaque, mesma foto do card (padrao): %s" % (
            "%s (Background do hero = %s)" % (ms, ROTULO_AJUSTE[AJUSTE_DE[ms]]) if ms else "sem medida"))
        od = p["destaque_outra_arte"]
        if od:
            print("    destaque com outra arte: %s (Background do hero = %s)" % (od, ROTULO_AJUSTE[AJUSTE_DE[od]]))
        else:
            print("    destaque com outra arte: %s" % p.get("destaque_outra_arte_obs", "nenhuma fonte diferente do card medida"))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--arquivo", help="dump JSON local (saida do wrangler --json ou lista de linhas)")
    ap.add_argument("--json", action="store_true", help="saida em JSON")
    a = ap.parse_args()
    linhas = ler_arquivo(a.arquivo) if a.arquivo else ler_d1()
    grupos = agregar(linhas)
    if a.json:
        out = []
        for (plat, fx), g in sorted(grupos.items()):
            out.append({"plataforma": plat, "faixa": fx, "n": g["n"], "aparelhos": len(g["aparelhos"]),
                        "fontes": {k: dict(v) for k, v in g["fontes"].items()},
                        "aplicacao": dict(g["aplicacao"]), "proposta": propor(g, plat)})
        print(json.dumps(out, ensure_ascii=False, indent=1))
    else:
        imprimir(grupos)


if __name__ == "__main__":
    main()
