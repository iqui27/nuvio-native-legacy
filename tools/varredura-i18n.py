#!/usr/bin/env python3
"""Acha texto de interface que NAO passa pela tabela de traducao.

POR QUE EXISTE. Tres rodadas do issue #12 foram resolvidas a mao, e a cada
release o relator mandava foto de mais uma tela em portugues. Varredura a mao
nao termina: erra o literal partido em dois, erra o acento escrito como \\uXXXX,
e nao sabe dizer se a chave existe na tabela.

DUAS FAMILIAS DE DEFEITO, e elas se consertam de formas diferentes:

  1. LITERAL DESENHADO SEM CHAVE. text.c chama i18n() em tudo que desenha, entao
     um literal solto TRADUZ — se a chave estiver na tabela. Sem a chave ele
     aparece em portugues. Conserto: acrescentar a linha em idioma_tab.h.

  2. FRASE MONTADA COM snprintf. A string final ("Retomar T1E1") nunca casa com
     chave nenhuma. Conserto: envolver o FORMATO — snprintf(b, n, i18n("..."), x)
     — que e como as telas ja consertadas fazem.

Uso:
    python3 tools/varredura-i18n.py          # lista o que falta
    python3 tools/varredura-i18n.py --tabela # so as linhas prontas para colar
"""
import re, sys, pathlib, unicodedata

RAIZ = pathlib.Path(__file__).resolve().parent.parent

# As funcoes de text.c que chamam i18n() no argumento `s`. txt_tracking esta na
# lista de proposito: ela mede E desenha, e nao passava por i18n.
DESENHO = ("txt_linha", "txt_linha_corta", "txt_linha_familia",
           "txt_linha_corta_familia", "txt_bloco", "txt_bloco_dir",
           "txt_tracking")

# Um literal em C pode vir partido ("abc" "def") e com escapes. Junta e decodifica.
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"(?:\s*"((?:[^"\\]|\\.)*)")*')

def literais_colados(texto, i):
    """Le a sequencia de literais adjacentes que comeca em `texto[i]` == '"'."""
    partes, n = [], len(texto)
    while i < n:
        while i < n and texto[i] in " \t\n\r\\":
            i += 1
        if i >= n or texto[i] != '"':
            break
        i += 1
        atual = []
        while i < n and texto[i] != '"':
            if texto[i] == "\\":
                atual.append(texto[i]); i += 1
                if i < n: atual.append(texto[i]); i += 1
            else:
                atual.append(texto[i]); i += 1
        i += 1
        partes.append("".join(atual))
    return "".join(partes), i

def decodificar(s):
    # \xNN, \uNNNN e os escapes simples. bytes -> utf-8, que e como a tabela guarda.
    out, i, n = bytearray(), 0, len(s)
    simples = {"n": 10, "t": 9, "r": 13, "0": 0, '"': 34, "\\": 92, "'": 39}
    while i < n:
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8"); i += 1; continue
        i += 1
        if i >= n: break
        e = s[i]; i += 1
        if e == "x":
            h = ""
            while i < n and len(h) < 2 and s[i] in "0123456789abcdefABCDEF":
                h += s[i]; i += 1
            if h: out.append(int(h, 16))
        elif e == "u":
            h = s[i:i+4]; i += 4
            try: out += chr(int(h, 16)).encode("utf-8")
            except ValueError: pass
        elif e in simples:
            out.append(simples[e])
        else:
            out += e.encode("utf-8")
    return out.decode("utf-8", "replace")

def chaves_da_tabela():
    txt = (RAIZ / "src/idioma_tab.h").read_text(encoding="utf-8")
    chaves = set()
    for linha in txt.splitlines():
        m = re.match(r'\s*\{\s*"((?:[^"\\]|\\.)*)"\s*,', linha)
        if m: chaves.add(decodificar(m.group(1)))
    return chaves

# Palavras e marcas que so aparecem em portugues. Nao serve para decidir o que
# TRADUZIR (todo literal desenhado e interface); serve para escolher, entre os
# muitos snprintf do codigo, quais sao frase de tela e quais sao log.
PT = re.compile(
    r"[áàâãéêíóôõúçÁÀÂÃÉÊÍÓÔÕÚÇ]|"
    r"\b(de|da|do|das|dos|em|para|com|sem|que|nao|não|uma|um|os|as|ao|à|"
    r"você|voce|seu|sua|todos|nenhum|nenhuma|mais|menos|ver|abrir|fechar|"
    r"episódio|episodio|temporada|fonte|fontes|legenda|legendas|assistir|"
    r"assistindo|reproduzir|adicionar|remover|buscar|carregando|erro|"
    r"agora|depois|antes|entre|sobre|desde|ainda|apenas|somente|"
    # Palavras SEM acento que o app usa e que nao tem stopword nenhuma na
    # frase: "A seguir" e "Personalizar" passaram batido na primeira versao
    # desta lista, e sao justamente duas das telas da foto do issue #12.
    r"seguir|personalizar|marcar|desmarcar|restaurar|padrao|padrão|opcoes|"
    r"opções|biblioteca|conta|perfil|tela|catalogo|catálogo|fileira|fileiras|"
    r"poster|pôster|destaque|amigos|historico|histórico|informacao|"
    r"informação|aguarde|selecionar|navegar|fechar)\b",
    re.IGNORECASE)

def eh_frase(s):
    """Tem cara de frase de interface? Descarta formato puro, id, url, chave JSON."""
    if len(s) < 3: return False
    if s.startswith(("http", "/", "tt", "{", "[", "#")): return False
    if re.fullmatch(r"[%\-+0-9.*a-zA-Z\s:/,]*", s) and "%" in s: return False
    if not re.search(r"[a-zA-ZáàâãéêíóôõúçÁ-Ú]{3}", s): return False
    return True

# Chamadas cujo texto NAO vai para a tela: log, comparacao, arquivo, rede.
# E a lista que separa "[rede] OpenSSL travado para %d regioes" (log, fica em
# portugues de proposito) de "Nenhuma fonte" (interface, tem de traduzir).
NAO_E_TELA = ("printf", "fprintf", "puts", "fputs", "perror", "marco",
              "strcmp", "strncmp", "strcasecmp", "strstr", "strchr", "strrchr",
              "getenv", "setenv", "fopen", "unlink", "remove", "rename",
              "mkdir", "system", "dlopen", "dlsym", "js_", "jsw_", "rede_",
              "curl_", "SDL_Log", "addons_buscar", "cat_indice_por",
              "idioma_registrar", "assert", "_Static_assert",
              "EM_ASM", "MAIN_THREAD")

RE_DESENHO = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(DESENHO) + r")\s*\(")

RE_NAO_E_TELA = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(f) for f in NAO_E_TELA) + r")")

def contexto(txt, i):
    """O trecho de codigo que antecede o literal, ate o comeco da instrucao."""
    j = i
    while j > 0 and txt[j-1] not in ";{}":
        j -= 1
    return txt[j:i]

# NAO E INTERFACE, conferido um a um. Cada entrada aqui e uma decisao, nao um
# "nao sei o que e": sem esta lista a ferramenta nao pode virar teste, e sem
# virar teste ela nao impede a proxima regressao.
IGNORAR = {
    "-perfil",                      # sufixo de nome de arquivo (ajustes.c)
    "biblioteca.h", "catalogo.h", "fileiras.h", "perfil.h", "legenda.h",
    "perfil.txt",                   # #include e nome de arquivo
    "com.webos.media.client.nuvio", # id do cliente LS2
    "abrir", "buscar",              # nomes de operacao do bridge JS (video_tizen.c)
    "fontes", "legenda", "mais", "nao", "poster",  # chaves internas, nao rotulo
    "crédit", "crédito",            # palavra procurada no capitulo do MKV
    # Nome proprio e sigla: iguais nos dois idiomas.
    "IMDb", "Trakt", "YouTube", "PIN", "AI-powered",
    # Nao sao tela: cabecalho do arquivo de preferencias e duas linhas de log
    # que a heuristica de portugues nao tem como distinguir das frases.
    "# Fileiras da Home, escolha DESTE aparelho. Nunca e enviada para\n"
    "# a conta nem para o Trakt.\n",
    "%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s",
    "hdr do pipeline: %s (fonte DV=%d)",
}

def varrer():
    chaves = chaves_da_tabela()
    faltando_tabela, faltando_i18n = {}, {}
    for arq in sorted((RAIZ / "src").glob("*.c")):
        if arq.name == "idioma.c":
            continue
        txt = arq.read_text(encoding="utf-8")
        # numero da linha por deslocamento
        quebras = [0]
        for k, c in enumerate(txt):
            if c == "\n":
                quebras.append(k)
        def linha_de(off):
            lo, hi = 0, len(quebras) - 1
            while lo < hi:
                mid = (lo + hi + 1) // 2
                if quebras[mid] <= off: lo = mid
                else: hi = mid - 1
            return lo + 1

        # Fora de comentario e fora de #include: percorre e pega cada bloco de
        # literais adjacentes uma vez so.
        i, n = 0, len(txt)
        while i < n:
            c = txt[i]
            if c == "/" and i + 1 < n and txt[i+1] == "/":
                i = txt.find("\n", i);  i = n if i < 0 else i;  continue
            if c == "/" and i + 1 < n and txt[i+1] == "*":
                i = txt.find("*/", i);  i = n if i < 0 else i + 2;  continue
            if c == "'":
                i += 3 if txt[i+1:i+2] != "\\" else 4;  continue
            if c != '"':
                i += 1;  continue
            ini = i
            bruto, i = literais_colados(txt, i)
            s = decodificar(bruto)
            if not eh_frase(s):
                continue
            if ("gl_FragColor" in s or "uniform " in s or "varying " in s
                    or "void main()" in s):
                continue                      # codigo de shader (gfx.c)
            ctx = contexto(txt, ini)
            # DUAS CAMADAS, e a diferenca entre elas ja custou uma regressao
            # nesta ferramenta.
            #
            # Literal entregue DIRETO a uma funcao de desenho e interface por
            # definicao: nao ha o que heuristicar, ou esta na tabela ou esta em
            # IGNORAR. Foi ao exigir "cara de portugues" tambem aqui que
            # "↑ Voltar aos filtros · Voltar: menu" (biblioteca.c) — que a
            # primeira versao pegava — voltou a passar batido.
            #
            # Fora do desenho (snprintf para um buffer que sera desenhado) nao
            # da para saber pelo contexto se e tela ou log, e ai sim vale a
            # heuristica de portugues.
            direto = RE_DESENHO.search(ctx) is not None
            if not direto and not PT.search(s):
                continue
            # POR NOME INTEIRO, e nao por substring: "printf" casa dentro de
            # "snprintf", e foi assim que "A seguir" (posplay.c) — uma das
            # telas da foto do #12 — passou batido nesta propria ferramenta.
            # Todo texto montado com snprintf estava sendo descartado calado.
            if RE_NAO_E_TELA.search(ctx):
                continue
            onde = f"{arq.name}:{linha_de(ini)}"
            # MONTAGEM: o formato tem de ser envolvido na origem, porque a
            # string final nunca casa com chave. Sem "%", o literal chega
            # inteiro na tela e basta a chave.
            if "%" in s and "i18n" not in ctx.split("(")[0][-40:] and "i18n" not in ctx:
                if s not in IGNORAR:
                    faltando_i18n.setdefault(s, []).append(onde)
            elif s not in chaves and s not in IGNORAR:
                faltando_tabela.setdefault(s, []).append(onde)
    return faltando_tabela, faltando_i18n

if __name__ == "__main__":
    tab, fmt = varrer()
    if "--tabela" in sys.argv:
        for s in sorted(tab): print('  { "%s", "" },' % s.replace('"', '\\"'))
        sys.exit(0)
    print("=== (1) literal desenhado SEM chave na tabela: %d ===" % len(tab))
    for s in sorted(tab):
        print("  %-72s %s" % (repr(s)[:72], ", ".join(tab[s][:3])))
    print()
    print("=== (2) snprintf em portugues SEM i18n no formato: %d ===" % len(fmt))
    for s in sorted(fmt):
        print("  %-72s %s" % (repr(s)[:72], ", ".join(fmt[s][:3])))
    sys.exit(1 if (tab or fmt) else 0)
