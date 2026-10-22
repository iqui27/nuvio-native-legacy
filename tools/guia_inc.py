#!/usr/bin/env python3
"""Gera src/ajustes_ux_guia_dados.inc a partir de design/guia-de-uso/guia.json.

O GUIA DE USO (Ajustes › Sobre e ajuda › Guia de uso) e conteudo, nao codigo:
os 104 recursos em 12 capitulos, com o texto, o "Onde fica" e o atalho de cada
um, vem do JSON aprovado com o mockup (cada entrada tem a prova no codigo em
"prova"). Este script so traduz o JSON para tabelas C. Mudou o JSON, rode de
novo e mande os dois juntos:

    python3 tools/guia_inc.py

O QUE O JSON NAO DIZ e mora aqui, conferido no codigo de hoje:
  ALVO     a opcao de Ajustes que "Abrir em Ajustes" foca (AJ_*);
  TELA     a tela que "Abrir o Guia de TV" e cia. abrem (GT_*);
  EXEMPLO  a cena de demonstracao de "Ver exemplo". SO entra quem tem cena de
           verdade (a previa das Novidades da 1.8.0 ou da 1.7); sem cena, o
           botao sai — botao que nao faz o que diz nao existe;
  VISUAL   a imagem do inspetor (GV_*). Sem visual proprio, o inspetor usa a
           previa da opcao-alvo em Ajustes (ou nenhuma).
Os icones sao os Lucide de deploy/app/art/icones (aj_<nome>.png).

i18n: TODO texto passa por i18n() no desenho; as chaves sao o portugues daqui.
O "Onde fica" e quebrado em pedacos ("Ajustes", "Tela inicial"...) e cada
pedaco e uma chave, que a maioria ja existe por ser nome de tela de Ajustes.
"""
import json, pathlib, re

RAIZ = pathlib.Path(__file__).resolve().parent.parent
JSON = RAIZ / "design" / "guia-de-uso" / "guia.json"
SAIDA = RAIZ / "src" / "ajustes_ux_guia_dados.inc"

# Lucide renomeou dois desenhos depois do mockup.
ICONE = {"filter": "funnel", "subtitles": "captions"}

ALVO = {
    "c-atualizar": "AJ_ATUALIZAR", "h-layout": "AJ_HOME_LAYOUT", "h-fileiras": "AJ_FIL_ORDEM",
    "h-limite": "AJ_FIL_LIMITE", "h-destaque": "AJ_HERO_CATALOGOS", "h-continuar": "AJ_CW_LIGADO",
    "h-card": "AJ_EXPANDIR", "h-barra": "AJ_RAIL", "h-amigos": "AJ_FIL_ORDEM", "h-perfis": "AJ_PS_FUNDO",
    "t-trailer": "AJ_DET_TRAILER_AUTO", "t-trailer-destaque": "AJ_HERO_TRAILER",
    "t-trailer-ajuste": "AJ_TRAILER_FONTE", "t-notas": "AJ_NT_IMDB", "t-dados": "AJ_TMDB_LIGADO",
    "t-colecao": "AJ_COL_ARTE_CONTA", "f-folha": "AJ_FONTE_MANUAL", "f-texto": "AJ_FONTE_TEXTO",
    "f-auto": "AJ_FONTE_PRIORIDADE", "f-imagem": "AJ_QUALIDADE", "f-pausa": "AJ_PAUSA_OVERLAY",
    "f-seekr": "AJ_SEEKR_LIGADO", "f-reacao": "AJ_REACAO_CREDITOS", "f-assistido": "AJ_CW_CONCLUIDO",
    "f-debrid": "AJ_DEBRID_RD", "f-p2p": "AJ_P2P_LIGADO", "l-idiomas": "AJ_LEG_LINGUA",
    "l-app": "AJ_IDIOMA", "l-meta": "AJ_TMDB_IDIOMA", "i-saida": "AJ_SAIDA_PLAYER",
    "i-posicao": "AJ_RELOGIO", "s-mais": "AJ_SALVOS_DEST", "o-privacidade": "AJ_PERFIL_PESQ",
    "o-trakt": "AJ_TRAKT", "o-perfis": "AJ_ADDONS", "v-xtream": "AJ_XTREAM_SERVIDOR",
    "v-pais": "AJ_EPG_PAIS", "v-diag": "AJ_LIVETV_DIAG", "a-vidro": "AJ_VIDRO", "a-cor": "AJ_TEMA",
    "a-logo": "AJ_COR_LOGO", "a-fonte": "AJ_FONTE_UI", "a-cartaz": "AJ_LARGURA_DP",
    "a-posters": "AJ_POSTER_PROV", "a-selo": "AJ_SELO_VISTO", "d-diag": "AJ_DIAGNOSTICO",
    "d-vel": "AJ_VELOCIDADE", "d-mem": "AJ_TEX_MB", "d-img": "AJ_QUALIDADE_IMG",
    "d-leve": "AJ_RESOLUCAO", "p-registro": "AJ_ENVIAR_LOG", "p-canal": "AJ_LIVETV_DIAG",
    "p-trava": "AJ_VELOCIDADE",
    # 2.0.3: o guia passa a contar a 2.0 (docs/releases/2.0.x/NOTAS.md).
    "f-regras": "AJ_FONTE_REGEX", "f-rapido": "AJ_FONTE_CONFERIR_VARIAS", "f-som": "AJ_DTS_AC3",
    "f-proporcao": "AJ_PROPORCAO_PADRAO", "f-servidores": "AJ_JF_LIGADO", "f-plugins": "AJ_PLUGINS",
    "l-forcada": "AJ_LEG_FORCADA", "l-segunda": "AJ_LEG_LINGUA2", "o-social": "AJ_SOCIAL",
    "a-descanso": "AJ_ESMAECER", "a-abertura": "AJ_LOGO_APP", "p-som": "AJ_DTS_AC3",
}
TELA = {"Abrir o Guia de TV": "GT_GUIA_TV", "Abrir a Biblioteca": "GT_BIBLIOTECA",
        "Abrir a Agenda": "GT_AGENDA", "Abrir o Explorar": "GT_EXPLORAR",
        "Abrir Perfil e Stats": "GT_PERFIL"}
# Cena de "Ver exemplo": (cartao, cena). Nenhuma outra entrada mostra o botao.
EXEMPLO = {
    "c-spotlight": "GX_N170_SPOT", "i-ilha": "GX_N170_ILHA", "s-salvos": "GX_N170_ILHA",
    "f-folha": "GX_N180_FONTES", "f-melhor": "GX_N180_FONTES", "f-filtros": "GX_N180_FONTES",
    "i-avisos": "GX_N180_ILHA",
}
VISUAL = {
    "c-guia": "GV_GUIA", "h-layout": "GV_LAYOUTS", "h-fileiras": "GV_FILEIRAS",
    "f-folha": "GV_FONTES", "f-melhor": "GV_FONTES", "f-filtros": "GV_FONTES", "f-texto": "GV_FONTES",
    "i-avisos": "GV_ILHA", "p-rede": "GV_ILHA", "p-addon": "GV_ILHA", "p-trakt": "GV_ILHA",
    "l-ass": "GV_ASS", "a-vidro": "GV_VIDRO",
    "c-spotlight": "GV_N170_SPOT", "i-ilha": "GV_N170_ILHA", "s-salvos": "GV_N170_ILHA",
}

def c(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

def main():
    g = json.loads(JSON.read_text(encoding="utf-8"))
    caps = g["caps"]
    ids = [k["id"] for k in caps]
    ent = g["entradas"]
    assert len(ent) == 104 and len(caps) == 12, (len(ent), len(caps))
    for k in list(ALVO) + list(EXEMPLO) + list(VISUAL):
        assert any(e["id"] == k for e in ent), k
    out = ["// GERADO por tools/guia_inc.py a partir de design/guia-de-uso/guia.json.",
           "// NAO EDITE A MAO: mude o JSON (ou as tabelas do script) e rode de novo.",
           "static const GuiaCap GUIA_CAPS[] = {"]
    for k in caps:
        out.append("  { %s, %s, %s }," % (c(k["t"]), c("aj_" + ICONE.get(k["ic"], k["ic"])), c(k["sub"])))
    out.append("};")
    out.append("static const GuiaEnt GUIA_ENT[] = {")
    for e in ent:
        acoes = e["acao"]
        alvo = ALVO.get(e["id"], "-1")
        if "Abrir em Ajustes" in acoes:
            assert e["id"] in ALVO, e["id"]
        else:
            alvo = "-1"
        tela = "GT_NADA"
        for a in acoes:
            if a in TELA: tela = TELA[a]
        ex = EXEMPLO.get(e["id"], "GX_NADA") if "Ver exemplo" in acoes else "GX_NADA"
        trilhas = [[p.strip() for p in t.split(" › ")] for t in e["onde"].split("; ")]
        onde = " | ".join(" › ".join(t) for t in trilhas)
        out.append("  { %d, %s, %s, %d, %s,\n    %s,\n    %s, %s, %s, %s, %s }," % (
            ids.index(e["cap"]), c(e["t"]), c("aj_" + ICONE.get(e["ic"], e["ic"])), 1 if e.get("novo") else 0,
            c(e["id"]), c(e["txt"]), c(onde), alvo, tela, ex, VISUAL.get(e["id"], "GV_NADA")))
    out.append("};")
    out.append("#define GUIA_NCAPS %d" % len(caps))
    out.append("#define GUIA_NENT %d" % len(ent))
    out.append("")
    SAIDA.write_text("\n".join(out), encoding="utf-8")
    print("%s: %d capitulos, %d recursos, %d com Ver exemplo" % (SAIDA.relative_to(RAIZ), len(caps), len(ent),
          sum(1 for e in ent if "Ver exemplo" in e["acao"] and e["id"] in EXEMPLO)))

if __name__ == "__main__":
    main()
