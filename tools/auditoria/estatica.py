#!/usr/bin/env python3
"""Auditoria estatica de pre-release (docs/auditoria-release.md).

Roda com o python do graphify (pipx): o grafo de chamadas sai do extrator AST
dele, refeito a cada execucao sobre src/ da arvore que esta sendo auditada.

    python tools/auditoria/estatica.py [--raiz .] [--sem-base] [--so 1,2]

Imprime PASS/FAIL por verificacao, com arquivo:linha. Sai com 1 se alguma
falhar. As verificacoes 4-6 (corrida, I/O no quadro, helper duplicado) tem uma
BASE de divida conhecida (tools/auditoria/base.txt): so o que for NOVO falha.
As 1-3 sao contratos e nao tem base: uma falha ali e defeito de produto.

O GRAFO DO GRAPHIFY LIGA CHAMADAS PELO NOME. Duas funcoes `static` com o mesmo
nome em arquivos diferentes viram uma so; por isso toda aresta para uma static
de OUTRO arquivo e cortada (o mesmo filtro do caca2). E as chamadas por
ponteiro de pthread_create/SDL_CreateThread nao sao arestas para ele: sao
acrescentadas aqui, marcadas como entrada de fio.
"""
import argparse
import collections
import glob
import hashlib
import json
import os
import re
import sys
from pathlib import Path

# --------------------------------------------------------------------------
# CONTRATOS. Editar aqui quando o codigo mudar de nome; o resto e generico.

# (1) Entradas de pagina/player e o dado que cada uma TEM de pedir sozinha.
# "Sozinha" = alcancavel no grafo a partir da funcao de entrada, e nao de um
# diff por quadro no roteador (app.c), que nao roda quando o alvo e o mesmo da
# ultima vez ou quando ha player aberto/retido.
ENTRADAS = [
    # entrada,               dado,                     funcao que pede o dado
    ("detail_abrir",         "lista de episodios",     "desc_episodios"),
    ("detail_abrir",         "extras (nota/elenco)",   "extras_pedir"),
    ("player_abrir",         "lista de episodios (Up next)", "desc_episodios"),
    ("player_retomar_retido", "lista de episodios (Up next)", "desc_episodios"),
]
# O funil de abertura da pagina: quem abre o detalhe por fora dele pula o
# preparo que ele faz (retida descartada, id tmdb: resolvido...).
FUNIL_DETALHE = ("detail_abrir", "abrirTitulo")
FUNIL_DONO = {"detail.c"}           # o proprio modulo pode chamar por dentro

# (2) Troca de perfil: a funcao que chama isto e o tratador da troca.
TROCA_PERFIL_MARCO = "sync_trocar_perfil"
ESQUECER_RE = re.compile(r"(esquecer|limpar|zerar|invalidar|apagar)", re.I)
REFAZER_RE = re.compile(r"(refazer|repetir|recarregar|carregar|definir|montar|puxar|aplicar|"
                        r"trocar|restaurar|iniciar|reconstruir|ler)", re.I)
# Publicadores de dado do perfil que rodam em fio: precisam conferir o perfil
# (ou uma geracao) antes de publicar, senao a resposta do perfil anterior
# entra por cima do novo.
PUBLICADORES_PERFIL = ["cat_trocar_continuar", "cat_definir_tudo", "salvos_definir",
                       "col_definir_json"]
CONFERE_PERFIL_RE = re.compile(r"perfis_ativo|perfil\w*[Gg]er|[Gg]eracao|\bger\w*\s*[!=]=|"
                               r"cwGer|PerfilGer|perfil_geracao")

# (3) Rotulo de fonte -> de onde o dado PODE vir. `loja` e a variavel que
# guarda o dado no arquivo; `permitidos` sao os leitores de resposta que podem
# escrever nela. Qualquer outro leitor escrevendo ali faz o rotulo mentir.
PROVENIENCIA = [
    {"rotulo": "introdb", "arquivo": "intro.c", "loja": "trechos",
     "permitidos": {"intro_extrair"},
     # 2.0.3: quando a mesma escrita grava tambem esta variavel de procedencia,
     # o dado de outro leitor (AniSkip) viaja com a fonte real e o rotulo do
     # consumidor ja nao mente (credfonte.c escolhe "aniskip" por ela).
     "marca_fonte": "trechosAni",
     "leitor_re": r"\b(\w*extrair\w*|\w*_parse\w*|\w*ler_resposta\w*)\s*\("},
]
ROTULOS_FONTE = ["introdb", "trakt", "simkl", "nuvio", "tmdb", "aniskip", "cinemeta",
                 "imdb", "mdblist", "letterboxd", "tvdb"]

# (5) Raizes do quadro e o que bloqueia.
RAIZES_QUADRO = ["app_atualizar", "app_desenhar", "app_evento"]
REDE_RE = re.compile(r"^(rede_(baixar|postar|apagar|pedir|url_final|medir_vazao|aquecer)\w*|"
                     r"curl_easy_perform|getaddrinfo|gethostbyname)$")
ESPERA_RE = re.compile(r"^(usleep|nanosleep|sleep|SDL_Delay|pthread_join|SDL_WaitThread)$")

# (6) Helpers duplicados: corpo minimo para contar.
DUP_MIN_CHARS = 160
DUP_IGNORA = re.compile(r"^(idioma_|vendor/)")

# --------------------------------------------------------------------------

AQUI = Path(__file__).resolve().parent


def limpar_c(texto, manter_strings=False):
    """Tira comentarios (e, por padrao, o miolo das strings), preservando as
    quebras de linha para os numeros de linha continuarem valendo."""
    out = []
    i, n = 0, len(texto)
    while i < n:
        c = texto[i]
        if c == "/" and i + 1 < n and texto[i + 1] == "/":
            j = texto.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i)); i = j; continue
        if c == "/" and i + 1 < n and texto[i + 1] == "*":
            j = texto.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join("\n" if ch == "\n" else " " for ch in texto[i:j])); i = j; continue
        if c in "\"'":
            j = i + 1
            while j < n and texto[j] != c:
                if texto[j] == "\\": j += 1
                elif texto[j] == "\n": break
                j += 1
            j = min(j + 1, n)
            if manter_strings: out.append(texto[i:j])
            else: out.append(c + " " * max(0, j - i - 2) + (c if j - i >= 2 else ""))
            i = j; continue
        out.append(c); i += 1
    return "".join(out)


class Base:
    def __init__(self, raiz, cache):
        self.raiz = Path(raiz).resolve()
        self.src = self.raiz / "src"
        self.cache = Path(cache)
        self.cache.mkdir(parents=True, exist_ok=True)
        self._grafo()

    # ---- grafo -------------------------------------------------------------
    def _grafo(self):
        from graphify.extract import extract
        arqs = sorted(glob.glob(str(self.src / "*.c")) + glob.glob(str(self.src / "*.h")))
        devnull = open(os.devnull, "w")
        velho = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = devnull       # o extrator fala muito
        try:
            ast = extract([Path(a) for a in arqs], cache_root=self.cache, root=self.src)
        finally:
            sys.stdout, sys.stderr = velho
        self.nodes = {n["id"]: n for n in ast["nodes"]}
        self.txt, self.code, self.codes = {}, {}, {}
        for a in glob.glob(str(self.src / "*.c")):
            f = os.path.basename(a)
            t = open(a, errors="ignore").read()
            self.txt[f] = t.split("\n")
            self.code[f] = limpar_c(t).split("\n")
            self.codes[f] = limpar_c(t, manter_strings=True).split("\n")
        # funcoes: id -> (arquivo, inicio, fim, nome, static)
        self.fn = {}
        self.por_nome = collections.defaultdict(list)
        for i, n in self.nodes.items():
            f = n.get("source_file")
            if not n.get("_callable") or f not in self.code or not n.get("source_location"):
                continue
            nome = n["label"].rstrip("()")
            ini = int(n["source_location"][1:])
            fim = self._fim(f, ini)
            if fim is None:
                continue
            ctx = " ".join(self.code[f][max(0, ini - 3):ini])
            st = bool(re.search(r"\bstatic\b", ctx.split(nome)[0] if nome in ctx else ctx))
            self.fn[i] = (f, ini, fim, nome, st)
            self.por_nome[nome].append(i)
        # Nome publico = declarado num .h. Aresta para funcao de OUTRO arquivo
        # que nao esta em header nenhum e o grafo casando nome de libc com
        # funcao do projeto (stat() da libc virava a `stat` de um .c).
        self.publicos = set()
        for h in glob.glob(str(self.src / "*.h")):
            for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", limpar_c(open(h, errors="ignore").read())):
                self.publicos.add(m.group(1))
        self.chama = collections.defaultdict(set)
        cortadas = 0
        for e in ast["edges"]:
            if e.get("relation") != "calls":
                continue
            s, d = e["source"], e["target"]
            if s not in self.fn or d not in self.fn:
                continue
            if self.fn[s][0] != self.fn[d][0] and (self.fn[d][4] or self.fn[d][3] not in self.publicos):
                cortadas += 1; continue
            self.chama[s].add(d)
        # Nome com static no proprio arquivo ganha da global: o grafo pode ter
        # ligado a chamada a outra funcao de mesmo nome.
        self.cortadas = cortadas
        # entradas de fio
        self.fios = set()
        for i, (f, ini, fim, nome, st) in self.fn.items():
            corpo = "\n".join(self.code[f][ini - 1:fim])
            for m in re.finditer(r"pthread_create\s*\([^,]*,[^,]*,\s*(?:\([^)]*\)\s*)?&?(\w+)", corpo):
                alvo = self.resolver(m.group(1), f)
                if alvo:
                    self.fios.add(alvo); self.chama[i].add(alvo)
            for m in re.finditer(r"SDL_CreateThread\s*\(\s*&?(\w+)", corpo):
                alvo = self.resolver(m.group(1), f)
                if alvo:
                    self.fios.add(alvo); self.chama[i].add(alvo)

    def _fim(self, f, ini):
        linhas = self.code[f]
        prof, comecou = 0, False
        for k in range(ini - 1, min(len(linhas), ini + 4000)):
            ln = linhas[k]
            if not comecou and ";" in ln and "{" not in ln and k > ini + 3:
                return None
            for ch in ln:
                if ch == "{":
                    prof += 1; comecou = True
                elif ch == "}":
                    prof -= 1
                    if comecou and prof == 0:
                        return k + 1
        return None

    def resolver(self, nome, arquivo=None):
        ids = self.por_nome.get(nome, [])
        if arquivo:
            mesmos = [i for i in ids if self.fn[i][0] == arquivo]
            if mesmos:
                return mesmos[0]
        glob_ = [i for i in ids if not self.fn[i][4]]
        return glob_[0] if glob_ else (ids[0] if len(ids) == 1 else None)

    def ids(self, nome):
        return [i for i in self.por_nome.get(nome, [])]

    def nome(self, i):
        return self.fn[i][3]

    def loc(self, i):
        return f"{self.fn[i][0]}:{self.fn[i][1]}"

    def corpo(self, i, strings=False):
        f, ini, fim = self.fn[i][:3]
        src = self.codes[f] if strings else self.code[f]
        return src[ini - 1:fim]

    def bfs(self, raizes, parar=frozenset()):
        ant = {r: None for r in raizes}
        fila = list(raizes)
        while fila:
            u = fila.pop(0)
            for v in self.chama.get(u, ()):
                if v in ant or v in parar:
                    continue
                ant[v] = u; fila.append(v)
        return ant

    def cadeia(self, ant, v, n=7):
        p = []
        while v is not None and len(p) < n:
            p.append(self.nome(v)); v = ant[v]
        return " <- ".join(p)

    def chamadas_de(self, i, alvo_re):
        """(linha, nome) de cada chamada no corpo de i cujo nome casa alvo_re."""
        f, ini = self.fn[i][:2]
        out = []
        for k, ln in enumerate(self.corpo(i)):
            for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", ln):
                if re.fullmatch(alvo_re, m.group(1)):
                    out.append((ini + k, m.group(1)))
        return out

    def sitios(self, nome):
        """Cada chamada de `nome` em src/*.c: (arquivo, linha, id da funcao)."""
        out = []
        for i, (f, ini, fim, n, st) in self.fn.items():
            for k, ln in enumerate(self.code[f][ini - 1:fim]):
                if k == 0 and n == nome:
                    continue
                if re.search(r"\b" + re.escape(nome) + r"\s*\(", ln):
                    out.append((f, ini + k, i))
        return sorted(out)

    def condicao_txt(self, f, linha, volta=3):
        """O `if` da mesma linha ou das 3 de cima, em texto original."""
        cl, _ = self.condicao(f, linha, volta)
        if not cl:
            return None, ""
        txt = " ".join(x.strip() for x in self.txt[f][cl - 1:linha])
        return cl, re.sub(r"\s+", " ", txt)[:200]

    def portoes(self, f, linha, n=2):
        """Cabecalhos dos blocos `if` que envolvem a linha (de dentro para fora)."""
        out = []
        for a, b in _bloco(self, f, linha):
            k = a - 1
            cab = self.code[f][k]
            # o cabecalho pode comecar linhas acima ("if (a &&\n b) {")
            j = k
            while j > 0 and "if" not in cab and not re.search(r"[;{}]\s*$", self.code[f][j - 1]):
                j -= 1; cab = self.code[f][j] + cab
            if re.search(r"\b(if|else)\b", cab):
                txt = " ".join(x.strip() for x in self.txt[f][j:k + 1])
                out.append(f"{f}:{j + 1}: {re.sub(chr(92) + 's+', ' ', txt)[:200]}")
            if len(out) >= n:
                break
        return out

    def condicao(self, f, linha, volta=12):
        """A condicao `if` mais proxima acima da linha (para mostrar o portao)."""
        for k in range(linha - 1, max(0, linha - volta) - 1, -1):
            ln = self.code[f][k]
            if re.search(r"\bif\s*\(", ln):
                # junta ate fechar o parentese
                txt = " ".join(x.strip() for x in self.code[f][k:min(k + 4, linha)])
                return k + 1, re.sub(r"\s+", " ", txt)[:220]
        return None, ""


# ---- resultado --------------------------------------------------------------
class Resultado:
    def __init__(self, num, titulo):
        self.num, self.titulo = num, titulo
        self.falhas, self.avisos, self.info = [], [], []
        self.conhecidas = 0

    def falha(self, chave, txt, ev=()):
        self.falhas.append((chave, txt, list(ev)))

    def aviso(self, txt, ev=()):
        self.avisos.append((txt, list(ev)))

    def imprimir(self, maxev=10):
        st = "FAIL" if self.falhas else "PASS"
        extra = f" ({self.conhecidas} na base de divida conhecida)" if self.conhecidas else ""
        print(f"\n[{st}] {self.num}. {self.titulo}{extra}")
        for chave, txt, ev in self.falhas:
            print(f"  FAIL {txt}")
            for e in ev[:maxev]:
                print(f"       {e}")
            if len(ev) > maxev:
                print(f"       ... +{len(ev) - maxev}")
        for txt, ev in self.avisos:
            print(f"  aviso {txt}")
            for e in ev[:3]:
                print(f"       {e}")
        for t in self.info:
            print(f"  info {t}")


# ---- 1 ----------------------------------------------------------------------
def v1_entradas(B):
    R = Resultado(1, "entradas de detalhe/player pedem o mesmo dado")
    for entrada, dado, func in ENTRADAS:
        es = B.ids(entrada)
        fs = set(B.ids(func))
        if not es or not fs:
            R.falha(f"1:{entrada}:{func}", f"{entrada} ou {func} nao existe mais (atualizar ENTRADAS)")
            continue
        ant = B.bfs(es)
        achou = [v for v in ant if v in fs]
        if achou:
            R.info.append(f"{entrada} -> {func}: ok ({B.cadeia(ant, achou[0])})")
            continue
        ev = [f"{B.loc(es[0])} {entrada}() nao alcanca {func}() no grafo"]
        for f, ln, quem in B.sitios(func):
            ev.append(f"{f}:{ln} so pede em {B.nome(quem)}()")
            ev += ["     sob " + p for p in B.portoes(f, ln)]
        R.falha(f"1:{entrada}:{func}", f"{entrada}() nao pede {dado} ({func})", ev)
    # funil do detalhe
    alvo, funil = FUNIL_DETALHE
    sitios = [s for s in B.sitios(alvo) if s[0] not in FUNIL_DONO]
    fun = B.ids(funil)
    if fun:
        f0 = fun[0]
        # o preparo do funil: chamadas antes de detail_abrir no corpo dele
        prep = []
        for k, ln in enumerate(B.corpo(f0)):
            if re.search(r"\b" + alvo + r"\s*\(", ln):
                break
            prep += [m.group(1) for m in re.finditer(r"\b([a-z]\w+_\w+)\s*\(", ln)]
        prep = [p for p in dict.fromkeys(prep) if not p.startswith(("cat_item", "strncmp", "atol"))]
        fora = [(f, ln, q) for f, ln, q in sitios if B.nome(q) != funil]
        for f, ln, q in fora:
            corpo = "\n".join(B.corpo(q))
            falta = [p for p in prep if not re.search(r"\b" + p + r"\s*\(", corpo)]
            R.falha(f"1:funil:{B.nome(q)}",
                    f"{f}:{ln} {B.nome(q)}() abre o detalhe por fora de {funil}()",
                    [f"o funil faz e este caminho nao: {', '.join(falta) or '(nada)'}",
                     f"funil: {B.loc(f0)}"])
    # player_abrir: quem abre sem definir o episodio
    for f, ln, q in B.sitios("player_abrir"):
        if f == "player.c":
            continue
        prox = " ".join(B.code[f][ln - 1:ln + 8])
        if not re.search(r"player_definir_episodio|episodioDoDetalhe|tocarCanal|ehCanal", prox) \
                and B.nome(q) != "tocarCanal":
            R.aviso(f"{f}:{ln} {B.nome(q)}() abre o player sem definir episodio nas 8 linhas seguintes")
    return R


# ---- 2 ----------------------------------------------------------------------
def _bloco(B, f, linha):
    """(ini, fim) do bloco { } que contem `linha`, e o do pai."""
    linhas = B.code[f]
    pilha, blocos = [], []
    for k, ln in enumerate(linhas):
        for ch in ln:
            if ch == "{":
                pilha.append(k + 1)
            elif ch == "}" and pilha:
                a = pilha.pop()
                if a <= linha <= k + 1:
                    blocos.append((a, k + 1))
    blocos.sort(key=lambda b: b[1] - b[0])
    return blocos


def v2_troca_perfil(B):
    R = Resultado(2, "o que a troca de perfil derruba e refeito")
    sit = B.sitios(TROCA_PERFIL_MARCO)
    if not sit:
        R.falha("2:marco", f"{TROCA_PERFIL_MARCO}() nao e chamada (atualizar TROCA_PERFIL_MARCO)")
        return R
    f, ln, dono = sit[0]
    blocos = _bloco(B, f, ln)
    bi, bf = blocos[1] if len(blocos) > 1 else blocos[0]      # o pai do if (trocou)
    chamadas = []
    for k in range(bi - 1, bf):
        for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", B.code[f][k]):
            alvo = B.resolver(m.group(1), f)
            if alvo:
                chamadas.append((k + 1, m.group(1), alvo))
    raizes = [a for _, _, a in chamadas]
    ant = B.bfs(raizes)
    alc = {B.nome(v) for v in ant}
    # O que a troca so dispara (fio) costuma voltar pelo quadro: sync_passo
    # aplica o que o fio do sync trouxe. Isso conta, mas e dito.
    quadro = B.bfs([i for n in RAIZES_QUADRO for i in B.ids(n)])
    alcQ = {B.nome(v) for v in quadro}
    R.info.append(f"tratador: {B.nome(dono)}() {f}:{bi}-{bf}, {len(chamadas)} chamadas, "
                  f"{len(ant)} funcoes alcancaveis (fios incluidos)")
    # o que cai
    derruba = []
    for k, nome, alvo in chamadas:
        if ESQUECER_RE.search(nome):
            derruba.append((f"{f}:{k}", nome, alvo))
    # setters chamados com NULL dentro do que cai (cat_definir_tudo(NULL, 0...))
    nulos = []
    for loc, nome, alvo in derruba:
        for k, l in enumerate(B.corpo(alvo)):
            m = re.search(r"\b(\w+_(definir|trocar)\w*)\s*\(\s*(NULL|0)\s*[,)]", l)
            if m:
                nulos.append((f"{B.fn[alvo][0]}:{B.fn[alvo][1] + k}", m.group(1), nome))
    for loc, nome, alvo in derruba:
        mod = nome.split("_")[0]
        cand = sorted(n for n in alc if n.startswith(mod + "_") and REFAZER_RE.search(n)
                      and not ESQUECER_RE.search(n))
        candQ = sorted(n for n in alcQ if n.startswith(mod + "_") and REFAZER_RE.search(n)
                       and not ESQUECER_RE.search(n))
        if "_" not in nome:
            # static do proprio app.c (invalidarPerfil): o que ela zera e
            # conferido pelos setters com NULL logo abaixo.
            continue
        if cand:
            R.info.append(f"{loc} {nome}() -> refeito por {', '.join(cand[:4])}")
        elif candQ:
            R.info.append(f"{loc} {nome}() -> so o ciclo do quadro refaz ({', '.join(candQ[:3])}), "
                          "depende do sync trazer o dado")
        else:
            R.falha(f"2:{nome}", f"{loc} {nome}() derruba e nada do modulo '{mod}_' refaz",
                    [f"alcancaveis a partir da troca: nenhuma {mod}_(refazer|carregar|definir|...)"])
    for loc, setter, quem in nulos:
        ok = []
        for i in B.ids(setter):
            pass
        reais = []
        for f2, ln2, q in B.sitios(setter):
            l2 = B.code[f2][ln2 - 1]
            if re.search(re.escape(setter) + r"\s*\(\s*(NULL|0)\s*[,)]", l2):
                continue
            if q in ant:
                reais.append(f"{f2}:{ln2} em {B.nome(q)}() <- {B.cadeia(ant, q, 5)}")
            elif q == dono or q in quadro:
                reais.append(f"{f2}:{ln2} em {B.nome(q)}() (ciclo do quadro)")
        if reais:
            R.info.append(f"{loc} {setter}(NULL) em {quem}() -> republicado com dado em {reais[0]}")
        else:
            R.falha(f"2:nulo:{setter}",
                    f"{loc} {quem}() zera {setter}(NULL) e nenhum {setter}(dado) e alcancavel da troca",
                    [f"chamadas com dado: {', '.join(f'{a}:{b}' for a, b, _ in B.sitios(setter)[:5])}"])
    # publicadores em fio sem conferir perfil
    fio_alc = {}
    for e in B.fios:
        for v in B.bfs([e], B.fios - {e}):
            fio_alc.setdefault(v, e)
    for pub in PUBLICADORES_PERFIL:
        for f2, ln2, q in B.sitios(pub):
            if q not in fio_alc:
                continue
            l2 = B.code[f2][ln2 - 1]
            if re.search(re.escape(pub) + r"\s*\(\s*(NULL|0)\s*[,)]", l2):
                continue
            corpo = "\n".join(B.corpo(q)) + "\n" + "\n".join(B.corpo(fio_alc[q]))
            if not CONFERE_PERFIL_RE.search(corpo):
                R.falha(f"2:pub:{B.nome(q)}:{pub}",
                        f"{f2}:{ln2} {B.nome(q)}() publica {pub}() de um fio sem conferir perfil/geracao",
                        [f"fio: {B.loc(fio_alc[q])} {B.nome(fio_alc[q])}()",
                         "a resposta do perfil anterior pode entrar depois da troca"])
    return R


# ---- 3 ----------------------------------------------------------------------
def v3_rotulos(B):
    R = Resultado(3, "rotulo de fonte so vem de campo real da API")
    for c in PROVENIENCIA:
        f, loja = c["arquivo"], c["loja"]
        if f not in B.code:
            R.falha(f"3:{c['rotulo']}", f"{f} nao existe (atualizar PROVENIENCIA)"); continue
        esc = re.compile(r"(memcpy\s*\(\s*&?" + loja + r"\b|\b" + loja + r"\s*\[[^\]]*\]\s*(\.\w+\s*)?=[^=]|\b"
                         + loja + r"\s*=[^=])")
        for i, (ff, ini, fim, nome, st) in B.fn.items():
            if ff != f:
                continue
            corpo = B.corpo(i)
            escreve = [ini + k for k, l in enumerate(corpo) if esc.search(l)]
            if not escreve:
                continue
            leitores = {}
            for k, l in enumerate(corpo):
                for m in re.finditer(c["leitor_re"], l):
                    leitores.setdefault(m.group(1), ini + k)
            # leitores alcancados por funcoes chamadas daqui (ex.: pedirAniskip)
            for v in B.bfs([i]):
                if v == i or B.fn[v][0] != f:
                    continue
                for k, l in enumerate(B.corpo(v)):
                    for m in re.finditer(c["leitor_re"], l):
                        leitores.setdefault(m.group(1), B.fn[v][1] + k)
            ruins = {n: ln for n, ln in leitores.items() if n not in c["permitidos"]}
            mf = c.get("marca_fonte")
            if ruins and mf and any(re.search(r"\b" + mf + r"\s*=[^=]", l) for l in corpo):
                ruins = {}
            if ruins:
                emite = [f"{a}:{b}" for a in B.codes for b, l in enumerate(B.codes[a], 1)
                         if f'"{c["rotulo"]}"' in l][:3]
                R.falha(f"3:{c['rotulo']}:{nome}",
                        f"rotulo \"{c['rotulo']}\" cobre dado de outra fonte: {f}:{escreve[0]} {nome}() "
                        f"grava `{loja}` com {', '.join(sorted(ruins))}",
                        [f"{f}:{ln} {n}() (nao e {', '.join(sorted(c['permitidos']))})" for n, ln in ruins.items()]
                        + [f"rotulo emitido em {e}" for e in emite])
    # ROTULO POR ELIMINACAO: num ternario com dois ou mais rotulos de fonte, o
    # ultimo ramo ("o que nao e A nem B e C") afirma uma fonte sem campo que a
    # prove. Aviso, nao falha: enum de tres valores mapeado para texto e
    # legitimo. O rotulo que MENTE e pego pelo contrato PROVENIENCIA acima.
    rot = "|".join(ROTULOS_FONTE)
    for f, linhas in B.codes.items():
        for n, l in enumerate(linhas, 1):
            m = re.search(r":\s*\"(" + rot + r")\"\s*\)?\s*[;,)]", l)
            if not m:
                continue
            trecho = " ".join(x.strip() for x in linhas[max(0, n - 4):n])
            outros = set(re.findall(r"\?\s*\"(" + rot + r")\"", trecho)) - {m.group(1)}
            if outros:
                R.aviso(f"{f}:{n} \"{m.group(1)}\" por eliminacao (depois de {', '.join(sorted(outros))})",
                        [B.txt[f][n - 1].strip()[:150]])
    return R


# ---- 4 ----------------------------------------------------------------------
TRAVA_RE = re.compile(r"pthread_mutex_lock|pthread_mutex_trylock|SDL_LockMutex|atomic_|__sync|__atomic|"
                      r"\btravar\w*\(|\btrancar\w*\(|\b[A-Z_]*(TRANC|TRAV|LOCK)[A-Z_]*\s*\(|pthread_rwlock")


def _estaticas(linhas):
    vs = set()
    for ln in linhas:
        if not ln.startswith("static") or re.match(r"static\s+(inline\s+)?const\b", ln):
            continue
        # atomico, trava, por-fio (_Thread_local) e volatile (bandeira de
        # proposito) ficam fora: a lista e para ler, nao para encher.
        if re.search(r"_Atomic|atomic_|pthread_mutex_t|pthread_cond_t|pthread_once|SDL_mutex|"
                     r"pthread_rwlock_t|volatile|_Thread_local|__thread|thread_local", ln):
            continue
        cab = ln.split("=")[0]
        if "(" in cab and not re.search(r"\(\s*\*", cab):
            continue
        for m in re.finditer(r"\*?\s*([A-Za-z_]\w*)\s*(?:\[[^\]]*\])*\s*(?=[=;,])", cab if "=" in ln else ln):
            nm = m.group(1)
            if len(nm) > 2 and nm not in ("static", "int", "char", "float", "double", "unsigned", "long", "void", "struct",
                          "bool", "size_t", "uint8_t", "int64_t", "uint32_t", "Uint32", "Uint64", "time_t"):
                vs.add(nm)
    return vs


def v4_corridas(B):
    R = Resultado(4, "estado de arquivo escrito por 2+ fios sem trava")
    raizes = [i for n in RAIZES_QUADRO + ["main"] for i in B.ids(n)]
    M = B.bfs(raizes, B.fios)
    T = {}
    for e in B.fios:
        for v in B.bfs([e], B.fios - {e}):
            T.setdefault(v, set()).add(e)
    porArq = collections.defaultdict(list)
    for i in B.fn:
        porArq[B.fn[i][0]].append(i)
    # TRAVA NO CHAMADOR conta: helper static chamado so de dentro de quem ja
    # segura a trava (garantirFaixas sob catTrava) nao e corrida. Ponto fixo:
    # travada = trava no corpo, ou todos os chamadores do mesmo arquivo travados.
    chamadores = collections.defaultdict(set)
    for u, vs in B.chama.items():
        for v in vs:
            if B.fn[u][0] == B.fn[v][0]:
                chamadores[v].add(u)
    travada = {i for i in B.fn if TRAVA_RE.search("\n".join(B.corpo(i)))}
    mudou = True
    while mudou:
        mudou = False
        for i in B.fn:
            if i in travada or not B.fn[i][4]:
                continue
            cs = chamadores.get(i)
            if cs and all(c in travada for c in cs):
                travada.add(i); mudou = True
    for f, ids in porArq.items():
        vs = _estaticas(B.code[f])
        vs -= {m.group(1) for l in B.code[f] for m in [re.match(r"\s*#\s*define\s+(\w+)", l)] if m}
        if not vs:
            continue
        for v in sorted(vs):
            wre = re.compile(r"\b" + v + r"\b\s*(\[[^\]]*\]\s*)?(\.\w+\s*|->\w+\s*)*(=[^=]|\+\+|--|\+=|-=|\|=|&=)|"
                             r"(\+\+|--)\s*\b" + v + r"\b|free\s*\(\s*" + v + r"\b|memset\s*\(\s*&?" + v
                             + r"\b|memcpy\s*\(\s*&?" + v + r"\b|snprintf\s*\(\s*" + v + r"\b")
            rre = re.compile(r"\b" + v + r"\b")
            thr, mn = [], []
            local = re.compile(r"(^|[;{(,]\s*)(const\s+)?(struct\s+)?[A-Za-z_]\w*\s*\**\s*\b" + v
                               + r"\b\s*(\[[^\]]*\]\s*)*(=[^=]|;|,|\))", re.M)
            for i in ids:
                corpo = "\n".join(B.corpo(i))
                if not rre.search(corpo) or local.search(corpo):
                    continue                    # nao usa, ou tem local/parametro com o mesmo nome
                w = bool(wre.search(corpo))
                lk = i in travada
                if i in T:
                    thr.append((i, w, lk))
                if i in M:
                    mn.append((i, w, lk))
            if not thr or not mn:
                continue
            tw = [x for x in thr if x[1] and not x[2]]
            mw = [x for x in mn if x[1] and not x[2]]
            # so conta ESCRITA sem trava dos DOIS lados (ou escrita num lado e
            # leitura sem trava no outro com realloc/free): os de baixo risco
            # (leitura de int solto) ficam fora para a lista ser lida.
            mr = [x for x in mn if not x[2]]
            tr = [x for x in thr if not x[2]]
            if not ((tw and mw) or (tw and mr and re.search(r"(free|realloc)\s*\(\s*" + v, "\n".join(
                    "\n".join(B.corpo(i)) for i, _, _ in tw))) or (mw and tr and re.search(
                    r"(free|realloc)\s*\(\s*" + v, "\n".join("\n".join(B.corpo(i)) for i, _, _ in mw)))):
                continue
            # a mesma funcao nos dois lados nao e prova de dois fios ao mesmo tempo
            if {i for i, _, _ in tw} == {i for i, _, _ in mw} and len(tw) == 1 and not (set(T.get(tw[0][0], ())) and tw[0][0] in M):
                continue
            ent = sorted({B.nome(e) for i, _, _ in (tw or tr) for e in T[i]})[:3]
            R.falha(f"4:{f}:{v}", f"{f} `{v}`: fio escreve em {', '.join(B.nome(i) for i, _, _ in (tw or tr))[:90]}; "
                                   f"quadro em {', '.join(B.nome(i) for i, _, _ in (mw or mr))[:90]}",
                    [f"fios: {', '.join(ent)}",
                     "linhas: " + ", ".join(B.loc(i) for i, _, _ in (tw or tr)[:2] + (mw or mr)[:2])])
    return R


# ---- 5 ----------------------------------------------------------------------
def _portao_de_teste(B, M, v):
    """Algum passo da cadeia so acontece sob `if (flag)` cuja flag so e ligada
    por funcao que ninguem em src/ chama? (contapend_sem_fio, por exemplo)."""
    u = v
    while M[u] is not None:
        pai = M[u]
        for f2, ln2, q in B.sitios(B.nome(u)):
            if q != pai:
                continue
            m = re.search(r"\bif\s*\(\s*!?\s*(\w+)\s*\)", B.code[f2][ln2 - 1])
            if m:
                var = m.group(1)
                quem = set()
                for i, (ff, ini, fim, nome, st) in B.fn.items():
                    if ff != f2:
                        continue
                    corpo = "\n".join(B.corpo(i))
                    if re.search(r"\b" + var + r"\s*=[^=]", corpo[corpo.find("{") + 1:]):
                        quem.add(nome)
                if quem and all(not B.sitios(q2) for q2 in quem):
                    return var
            break
        u = pai
    return None


def v5_bloqueio(B):
    R = Resultado(5, "rede/espera alcancavel do quadro sem fio")
    raizes = [i for n in RAIZES_QUADRO for i in B.ids(n)]
    if not raizes:
        R.falha("5:raiz", "nenhuma raiz de quadro (atualizar RAIZES_QUADRO)"); return R
    M = B.bfs(raizes, B.fios)
    vistos = set()
    for v in M:
        f, ini = B.fn[v][:2]
        if f == "rede.c":
            continue
        for k, l in enumerate(B.corpo(v)):
            for m in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", l):
                n = m.group(1)
                if REDE_RE.match(n) or ESPERA_RE.match(n):
                    chave = f"5:{B.nome(v)}:{n}"
                    if chave in vistos:
                        continue
                    vistos.add(chave)
                    ev = [B.cadeia(M, v, 10)]
                    # o portao de cada passo da cadeia: quase sempre e um `if`
                    u = v
                    while M[u] is not None and len(ev) < 4:
                        pai = M[u]
                        for f2, ln2, q in B.sitios(B.nome(u)):
                            if q == pai:
                                cl, ct = B.condicao_txt(f2, ln2)
                                if cl:
                                    ev.append(f"{f2}:{ln2} {B.nome(pai)}() chama {B.nome(u)}() sob: {ct}")
                                break
                        u = pai
                    morto = _portao_de_teste(B, M, v)
                    if morto:
                        R.info.append(f"{f}:{ini + k} {B.nome(v)}() -> {n}() so atras de `{morto}`, "
                                      "que so e ligado por funcao sem chamador em src/ (gancho de teste)")
                        continue
                    if n in ("pthread_join", "SDL_WaitThread"):
                        # juntar um fio que ja avisou que acabou e normal;
                        # so vira falha se alguem provar que ele ainda corre.
                        R.aviso(f"{f}:{ini + k} {B.nome(v)}() junta fio ({n}) no quadro", ev[:1])
                        continue
                    tipo = "rede" if REDE_RE.match(n) else "espera"
                    R.falha(chave, f"{f}:{ini + k} {B.nome(v)}() chama {n}() ({tipo}) no fio do quadro", ev)
    R.info.append(f"{len(M)} funcoes alcancaveis do quadro, {len(B.fios)} entradas de fio cortam a busca")
    return R


# ---- 6 ----------------------------------------------------------------------
def v6_duplicados(B):
    R = Resultado(6, "helper duplicado (mesmo corpo em 2+ arquivos)")
    grupos = collections.defaultdict(list)
    for i, (f, ini, fim, nome, st) in B.fn.items():
        if DUP_IGNORA.search(f):
            continue
        corpo = B.corpo(i)
        # so o corpo, do primeiro { em diante, sem espacos
        txt = "\n".join(corpo)
        k = txt.find("{")
        if k < 0:
            continue
        norm = re.sub(r"\s+", "", txt[k:])
        if len(norm) < DUP_MIN_CHARS:
            continue
        cab = re.sub(r"\s+", " ", txt[:k]).strip()
        assinatura = re.sub(r"\b" + re.escape(nome) + r"\b", "F", cab)
        h = hashlib.sha1((assinatura + norm).encode()).hexdigest()[:12]
        grupos[h].append(i)
    for h, ids in sorted(grupos.items()):
        arqs = {B.fn[i][0] for i in ids}
        if len(arqs) < 2:
            continue
        nomes = sorted({B.nome(i) for i in ids})
        R.falha(f"6:{'+'.join(nomes)}", f"{'/'.join(nomes)[:60]} igual em {len(arqs)} arquivos",
                [", ".join(B.loc(i) for i in ids)])
    return R


# ---- 7 ----------------------------------------------------------------------
PIPEFAIL_RE = re.compile(r"^\s*set\s+(-[A-Za-z]+\s+)*-[A-Za-z]*o\s+pipefail\b|^\s*set\s+-[A-Za-z]*o\s+pipefail\b",
                         re.M)
# Cabeca do comando a esquerda do pipe que conta como "saida de teste": binario
# do teste ($tmp/t, ./bin, /tmp/x), compilador, interprete. echo/printf/rg/grep/
# readelf/find... a esquerda nao: ali o status do pipe nao esconde falha de teste.
CABECA_TESTE_RE = re.compile(r'^(?:"?\$|""|\./|/|\.\./|cc\b|clang|gcc\b|make\b|timeout\b|bash\b|sh\b|'
                             r'python3?\b|node\b|java\b|kotlinc?\b)')


def _logicas(texto):
    """(linha, texto) por comando logico: junta `\\` no fim, tira comentario,
    corpo de heredoc, aspas e linha de padrao de `case`."""
    out, buf, ini, fim_here = [], "", 0, None
    for n, l in enumerate(texto.split("\n"), 1):
        if fim_here is not None:
            if l.strip() == fim_here:
                fim_here = None
            continue
        if not buf:
            ini = n
        s = re.sub(r"'[^']*'", "''", l)
        s = re.sub(r'"(?:[^"\\]|\\.)*"', '""', s)
        s = re.sub(r"(^|\s)#.*", "", s)
        m = re.search(r"<<-?\s*\\?['\"]?(\w+)", l)
        if m:
            fim_here = m.group(1)
        if s.rstrip().endswith("\\"):
            buf += s.rstrip()[:-1] + " "
            continue
        buf += s
        out.append((ini, buf))
        buf = ""
    return out


def _pipe_de_teste(logica):
    s = logica.split("||")[0].replace("|&", " | ")      # depois do || o pipe e do tratador
    if re.match(r"^\s*[^\s()=]+(\s*\|\s*[^\s()=]+)*\)", s):      # padrao de case
        return False
    if re.match(r"^\s*case\b", s):
        s = s.split(" in ", 1)[-1]
        s = re.sub(r"[^\s;()]+\)", " ", s)
    k = s.find("|")
    if k < 0:
        return False
    esq = s[:k].strip()
    esq = re.sub(r"^(if|while|until|then|do|!|\{|&&|\()\s*", "", esq)
    esq = re.sub(r"^(\w+=\$\(\s*)", "", esq)
    esq = re.sub(r"^(\w+=(?!\$\()\S*\s+)+", "", esq)
    esq = re.sub(r"^(if|!)\s+", "", esq)
    if "2>&1" in esq and re.search(r"-o\s", esq):                       # compilacao
        return True
    return bool(CABECA_TESTE_RE.match(esq))


def v7_pipefail(B):
    R = Resultado(7, "tests/*.sh que cala o status do teste num pipe (sem set -o pipefail)")
    for f in sorted(glob.glob(str(B.raiz / "tests" / "*.sh"))):
        t = open(f, errors="replace").read()
        if PIPEFAIL_RE.search(t):
            continue
        ev = [f"{os.path.relpath(f, B.raiz)}:{n}: {re.sub(chr(32) + '+', chr(32), l.strip())[:90]}"
              for n, l in _logicas(t) if _pipe_de_teste(l)]
        if ev:
            R.falha("7:" + os.path.basename(f), "pipe sem pipefail: o status e o do tail/grep/tee, "
                    "um teste que falha sai 0", ev[:1])
    return R


VERIFICACOES = [v1_entradas, v2_troca_perfil, v3_rotulos, v4_corridas, v5_bloqueio, v6_duplicados,
                v7_pipefail]
COM_BASE = {4, 5, 6}


def ler_base(p):
    if not p.exists():
        return set()
    return {l.split("#")[0].strip() for l in p.read_text().split("\n") if l.split("#")[0].strip()}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--raiz", default=str(AQUI.parent.parent))
    ap.add_argument("--cache", default=os.path.join(os.environ.get("TMPDIR", "/tmp"), "nv-auditoria-ast"))
    ap.add_argument("--base", default=str(AQUI / "base.txt"))
    ap.add_argument("--sem-base", action="store_true", help="mostra tambem a divida conhecida")
    ap.add_argument("--gravar-base", action="store_true", help="grava as falhas atuais de 4-6 como base")
    ap.add_argument("--so", default="", help="ex.: 1,2,3")
    a = ap.parse_args()
    so = {int(x) for x in a.so.split(",") if x}
    B = Base(a.raiz, a.cache)
    print(f"grafo: {len(B.fn)} funcoes, {sum(len(v) for v in B.chama.values())} chamadas, "
          f"{B.cortadas} arestas falsas para static de outro arquivo cortadas, {len(B.fios)} entradas de fio")
    base = set() if a.sem_base else ler_base(Path(a.base))
    novas_base, falhou = [], 0
    for k, v in enumerate(VERIFICACOES, 1):
        if so and k not in so:
            continue
        R = v(B)
        if k in COM_BASE:
            novas_base += [c for c, _, _ in R.falhas]
            if base:
                antes = len(R.falhas)
                R.falhas = [x for x in R.falhas if x[0] not in base]
                R.conhecidas = antes - len(R.falhas)
        R.imprimir()
        falhou |= bool(R.falhas)
    if a.gravar_base:
        Path(a.base).write_text("# divida conhecida das verificacoes 4-6 (auditoria-release).\n"
                                "# Uma linha por item aceito; apague a linha quando consertar.\n"
                                + "\n".join(sorted(set(novas_base))) + "\n")
        print(f"\nbase gravada: {a.base} ({len(set(novas_base))} itens)")
    print("\nESTATICA:", "FAIL" if falhou else "PASS")
    return 1 if falhou else 0


if __name__ == "__main__":
    sys.exit(main())
