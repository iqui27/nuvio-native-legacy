"""Cobertura do catalogo UX executavel (ver tests/ajustes_secoes.sh)."""
import argparse
import re
import sys
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("fonte", help="src/ajustes.c, de onde vem o enum OpcaoId")
parser.add_argument(
    "--catalogo",
    default=str(Path(__file__).resolve().parents[1] / "src/ajustes_ux_tela.inc"),
    help="include do catalogo; padrao src/ajustes_ux_tela.inc",
)
args = parser.parse_args()
fonte = Path(args.fonte).read_text(encoding="utf-8")
catalogo = Path(args.catalogo).read_text(encoding="utf-8")

enum = re.search(r"typedef enum \{(.*?)\n\} OpcaoId;", fonte, re.S)
if not enum:
    sys.exit("FALHA: enum OpcaoId nao encontrado em " + args.fonte)
corpo = re.sub(r"//[^\n]*", "", enum.group(1))
corpo = re.sub(r"^\s*#[^\n]*", "", corpo, flags=re.M)  # #ifdef NV_ANDROID slots
opcoes = [n.strip() for n in corpo.split(",") if n.strip() and n.strip() != "AJ_N"]
# Retired from the screen on purpose; the enum slot stays (positional valor[]/CHAVE[]).
RETIRADAS = {
    "AJ_RELOGIO_POS",  # 1.8: clock island is always top-right (inativa() == 1)
    "AJ_DESCOBRIR",    # 2.0.3: the web Discover screen does not exist on the TV (inativa() == 1)
    "AJ_AVANCADAS",    # 2.0.3: lives in the "Avancadas" pill at the top of the index
    # 2.0.3 (owner, 07/10): which ratings MDBList fetches follows "Notas no
    # titulo"; turning a rating on turns its pair on (notaLigarPar in ajustes.c).
    "AJ_MDB_TRAKT", "AJ_MDB_IMDB", "AJ_MDB_TMDB", "AJ_MDB_LETTER",
    "AJ_MDB_TOMATES", "AJ_MDB_AUDIENCIA", "AJ_MDB_META", "AJ_MDB_MAL",
    # 2.0.5 (owner, 12/10): profundidade virou o reflexo do foco do GFX_CARD,
    # com um controle so (AJ_PROF_BRILHO). Os outros AJ_PROF_* (borda, cobertura
    # e os cinco por tipo de cartao) sairam da tela de proposito; os slots ficam
    # no enum para o valor[] salvo manter os indices (migracao profundidade-205).
    "AJ_PROF_BORDA", "AJ_PROF_COBERTURA", "AJ_PROF_POSTERS", "AJ_PROF_CW",
    "AJ_PROF_EPS", "AJ_PROF_ELENCO", "AJ_PROF_TRAILERS",
    # 2.0.5 (#509): o som do trailer virou UM ajuste ("Som do trailer");
    # o slot de "Som do trailer na pagina do titulo" fica no enum pelo valor[]
    # salvo, e a migracao do carregar adota a escolha antiga.
    "AJ_DET_TRAILER_SOM",
    # 2.0.6 (#535): edited inside another row's editor, not as rows of their
    # own (uxOpcaoNaTela in ajustes_ux_desenho.inc): the per-provider limits
    # in "Ordem dos provedores", size/groups in "Filtros de fontes".
    "AJ_FALHA_RD_LIMITE", "AJ_FALHA_TB_LIMITE", "AJ_FALHA_PM_LIMITE", "AJ_FALHA_AD_LIMITE",
    "AJ_FILTRO_TAM_MAX", "AJ_FILTRO_GRUPOS",
}
opcoes = [op for op in opcoes if op not in RETIRADAS]

tabela = re.search(r"static const Item TELA\[\] = \{(.*?)\n\};", catalogo, re.S)
if not tabela:
    sys.exit("FALHA: static const Item TELA[] nao encontrado em " + args.catalogo)
corpo_tela = re.sub(r"//[^\n]*", "", tabela.group(1))
itens = re.findall(r'\b(SEC|GRP|ROT|OPC)\(\s*("(?:[^"\\]|\\.)*"|AJ_[A-Z_0-9]+)', corpo_tela)

falhas = []
if not itens:
    falhas.append("nenhum item encontrado em TELA[]")
elif itens[0][0] != "SEC":
    falhas.append("TELA[] nao comeca por uma categoria (SEC)")
if any(t == "GRP" for t, _ in itens):
    falhas.append("TELA[] deve usar apenas blocos fixos ROT, sem GRP recolhivel")

na_tela = [v for t, v in itens if t == "OPC"]
for op in opcoes:
    n = na_tela.count(op)
    if n == 0:
        falhas.append("%s nao aparece em TELA[]: a opcao some da TV" % op)
    elif n > 1:
        falhas.append("%s aparece %d vezes em TELA[]" % (op, n))
for op in na_tela:
    if op not in opcoes:
        falhas.append("TELA[] cita %s, que nao existe no enum" % op)

# Categorias e blocos ROT precisam conter ao menos uma opcao.
abertos = {}
for t, v in itens + [("SEC", "<fim>")]:
    fecha = ("SEC", "ROT") if t == "SEC" else ("ROT",) if t == "ROT" else ()
    for tipo in fecha:
        if tipo in abertos:
            nome, n = abertos.pop(tipo)
            if n == 0:
                falhas.append("%s %s vazio(a) em TELA[]" % (tipo, nome))
    if t in ("SEC", "ROT"):
        abertos[t] = [v, 0]
    elif t == "OPC":
        for tipo in abertos:
            abertos[tipo][1] += 1

n_sec = sum(1 for t, _ in itens if t == "SEC")
if n_sec != 11:
    falhas.append("esperadas 11 categorias, encontradas %d" % n_sec)

ajuda = re.search(r"static const char \*SECAO_AJUDA\[\] = \{(.*?)\n\};", catalogo, re.S)
if not ajuda:
    falhas.append("SECAO_AJUDA[] nao encontrado no include")
else:
    n_ajuda = len(re.findall(r'^\s*"', ajuda.group(1), re.M))
    if n_ajuda != n_sec:
        falhas.append("%d categorias e %d frases em SECAO_AJUDA" % (n_sec, n_ajuda))

# 2.0.3: avançada = vem depois do MAIS() da propria categoria (a linha "Mais
# opcoes"). Cada categoria tem no maximo um, e ele nao fica vazio.
avancadas = []
mais_por_sec, depois, sec_atual = {}, False, None
for t, v in re.findall(r'\b(SEC|ROT|OPC|MAIS)\(\s*("(?:[^"\\]|\\.)*"|AJ_[A-Z_0-9]+)?', corpo_tela):
    if t == "SEC":
        sec_atual, depois = v, False
    elif t == "MAIS":
        mais_por_sec[sec_atual] = mais_por_sec.get(sec_atual, 0) + 1
        depois = True
    elif t == "OPC" and depois:
        avancadas.append(v)
for sec, n in mais_por_sec.items():
    if n > 1:
        falhas.append("categoria %s tem %d MAIS()" % (sec, n))
if "AJ_SEEKR_AJUSTE" not in avancadas:
    falhas.append("AJ_SEEKR_AJUSTE deve permanecer avancada")
if "AJ_SAIDA_PLAYER" in avancadas:
    falhas.append("AJ_SAIDA_PLAYER deve permanecer basica")
if "AJ_QUALIDADE" in avancadas:
    falhas.append("AJ_QUALIDADE deve permanecer basica")

if falhas:
    for f in falhas:
        print("FALHA: " + f)
    sys.exit(1)

print("PASS: %d opcoes de Ajustes em %d categorias, cada uma exatamente uma vez (%d avancadas)."
      % (len(opcoes), n_sec, len(avancadas)))
