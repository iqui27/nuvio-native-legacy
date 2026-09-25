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
    r"informação|aguarde|selecionar|navegar|fechar|"
    # Palavras das TRES frases que vazaram na v1.0.35 ("Conhecido por %s",
    # "%d curtidas", "0 a %d por dia"): nenhuma delas tinha stopword da lista
    # acima, entao a porta 2 nao as viu e elas foram para a TV em portugues.
    r"por|dia|dias|curtidas|conhecido|título|titulo|títulos|titulos|"
    r"coleção|colecao|gênero|genero|nudez|leve|moderado|severo)\b",
    re.IGNORECASE)

def eh_frase(s):
    """Tem cara de frase de interface? Descarta formato puro, id, url, chave JSON."""
    if len(s) < 3: return False
    if s.startswith(("http", "/", "tt", "{", "[", "#")): return False
    # CAMINHO E URL montados com %s. So o startswith nao pega "%s/catalogo.txt"
    # nem "luna://com.webos.media/%s", e depois que a regra de formato passou a
    # deixar frases com "%" entrarem, os quatro viraram ruido fixo.
    if "://" in s: return False
    if re.search(r"/[A-Za-z0-9._-]+\.[A-Za-z]{2,4}$", s): return False
    # FORMATO PURO x FRASE QUE CONTEM FORMATO, e a diferenca custou tres
    # frases em portugues na v1.0.35. A regra antiga descartava todo literal
    # ASCII com "%" dentro — e "Conhecido por  %s", "%d curtidas" e
    # "0 a %d por dia" sao exatamente isso: sem acento, so letras e um
    # especificador. Agora tira os especificadores e pergunta se SOBROU
    # palavra; se nao sobrou, ai sim e formato puro ("%s/%s", "%d de %d").
    if "%" in s:
        resto = re.sub(r"%[-+0-9.*# ]*[a-zA-Z]", " ", s)
        if not re.search(r"[a-zA-ZáàâãéêíóôõúçÁ-Ú]{3}", resto): return False
    if not re.search(r"[a-zA-ZáàâãéêíóôõúçÁ-Ú]{3}", s): return False
    return True

# TERCEIRA FAMILIA DE DEFEITO, achada no menu de legendas/audio do player
# (issues #41/#42): FUNCAO QUE DEVOLVE ROTULO JA EM PORTUGUES, chamada sem
# i18n() em volta. ling_nome() (linguas.c) e o caso medido — devolve
# "Português", "Inglês" etc de uma tabela propria, e nao de idioma_tab.h — mas
# a lista cobre qualquer funcao assim que aparecer.
#
# POR QUE A VARREDURA DE CIMA NAO PEGA ISTO: aquela olha literais de string.
# `ling_nome(f->idioma)` nao e um literal, e uma chamada — o texto que ela
# devolve so existe em tempo de execucao. Uma chamada direta a uma funcao de
# desenho (`txt_linha_corta(TXT_X, ling_nome(c), ...)`) ainda traduz, porque
# text.c aplica i18n() em CIMA do que a funcao devolver, seja literal ou nao.
# O buraco e o INVERSO: `ling_nome(...)` jogado dentro de um snprintf/vsnprintf
# que MONTA uma string composta ("%s  ·  %s") sem passar aquele pedaco por
# i18n() antes — a string final nunca bate com chave nenhuma da tabela, e nunca
# vai bater, entao fica em portugues para sempre. Ver video.c/video_tizen.c
# (rotulo de faixa de audio) e addons.c (rotulo de legenda do OpenSubtitles):
# os tres tinham exatamente este buraco.
FUNCOES_ROTULO_PT = ("ling_nome",)

RE_FUNC_ROTULO = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(FUNCOES_ROTULO_PT) + r")\s*\(")
# i18n( imediatamente antes da chamada: i18n(ling_nome(...)) esta correto.
RE_I18N_ANTES = re.compile(r"i18n\s*\(\s*$")

# CONFERIDAS UMA A UMA, cada uma com o motivo — mesma disciplina da IGNORAR lá
# em cima. Sem esta lista a secao (3) não pode virar teste (achar_rotulo_
# sem_i18n() sozinha nao sabe separar "seguro" de "esqueceram").
IGNORAR_FUNCAO = {
    # A propria DEFINICAO de ling_nome() em linguas.c contem "ling_nome(" no
    # cabecalho (`const char *ling_nome(const char *c) {`) — nao e uma
    # CHAMADA, e o regex nao distingue. So existe uma vez neste arquivo.
    "linguas.c",
    # ajustes.c:167 guarda ling_nome() CRU de proposito: rotulosDeIdioma() roda
    # uma vez por abertura da tela, nao por quadro. Quem traduz de verdade e
    # desenhaLinha->txt_linha_corta, a cada quadro, com o idioma CORRENTE —
    # exatamente como os outros rotulos desta tela (V_QUALIDADE etc.), que
    # tambem ficam em portugues no vetor e so viram ingles no desenho. i18n()
    # aqui prenderia a lista no idioma de quando a tela abriu.
    "ajustes.c:167",
}

def achar_rotulo_sem_i18n(txt, linha_de, ctx_fn, re_desenho, re_nao_e_tela, nome_arq):
    """Toda chamada a uma FUNCOES_ROTULO_PT que nem e i18n(...), nem e
    argumento DIRETO de uma funcao de desenho, nem log/comparacao — as tres
    formas seguras. O resto e uma string que vai ser MONTADA (snprintf) sem
    traducao, o buraco desta secao."""
    achados = []
    if nome_arq in IGNORAR_FUNCAO:
        return achados
    for m in RE_FUNC_ROTULO.finditer(txt):
        j = m.start()
        while j > 0 and txt[j-1] in " \t\n":
            j -= 1
        pedaco = txt[max(0, j-6):j]
        if RE_I18N_ANTES.search(pedaco):
            continue
        ctx = ctx_fn(txt, m.start())
        if re_desenho.search(ctx) or re_nao_e_tela.search(ctx):
            continue
        ln = linha_de(m.start())
        if f"{nome_arq}:{ln}" in IGNORAR_FUNCAO:
            continue
        achados.append((m.group(1), ln))
    return achados

# Chamadas cujo texto NAO vai para a tela: log, comparacao, arquivo, rede.
# E a lista que separa "[rede] OpenSSL travado para %d regioes" (log, fica em
# portugues de proposito) de "Nenhuma fonte" (interface, tem de traduzir).
NAO_E_TELA = ("printf", "fprintf", "puts", "fputs", "perror", "marco",
              "strcmp", "strncmp", "strcasecmp", "strstr", "strchr", "strrchr",
              "getenv", "setenv", "fopen", "unlink", "remove", "rename",
              "mkdir", "system", "dlopen", "dlsym", "js_", "jsw_", "rede_",
              "curl_", "SDL_Log", "addons_buscar", "cat_indice_por",
              "idioma_registrar", "assert", "_Static_assert",
              "EM_ASM", "MAIN_THREAD",
              # NOME DE ICONE NAO E TEXTO. gfx_icone recebe o basename do SVG
              # rasterizado ("guia-fontes", "menu_home"), e a heuristica de
              # portugues casou com "fontes" — a ferramenta pedia traducao para
              # um nome de arquivo. Acusar o que nao e defeito ensina a ignorar
              # o teste, que e como esta classe de defeito sobrevive.
              "gfx_icone", "gfx_icones_dir", "extras_caminho_marca_nome",
              "tex_obter", "tex_arquivo",
              # DADO DE MENTIRA da central de avisos (NUVIO_AVISOS_DEMO): imita
              # o que a rede traria, nao e rotulo de tela.
              "demoAviso")

RE_DESENHO = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(DESENHO) + r")\s*\(")

# i18n( logo antes do literal. O autor JA DISSE que aquilo e tela — nao ha o que
# heuristicar depois disso.
RE_I18N = re.compile(r"(?<![A-Za-z0-9_])i18n\s*\(\s*$")

RE_NAO_E_TELA = re.compile(
    r"(?<![A-Za-z0-9_])(" + "|".join(re.escape(f) for f in NAO_E_TELA) + r")")

# CABECALHO DE HTTP NAO E TELA, e a forma dele e inconfundivel: "Nome: valor",
# com o nome em ASCII e hifen. Foi preciso porque a heuristica de portugues casa
# com pedacos de User-Agent — o MAG250 do stalker.c manda "... ver: 2 rev: 250",
# e `ver` esta na lista de palavras de interface. Um cabecalho marcado como
# texto de tela faria a suite acusar uma traducao que nao existe, e teste que
# acusa o que nao e defeito ensina a ignorar o teste.
RE_CABECALHO_HTTP = re.compile(r"^[A-Za-z][A-Za-z0-9-]{1,40}: ")

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
    "abrir", "buscar", "erro",      # nomes de operacao do bridge JS (video_tizen.c)
    "fontes", "legenda", "mais", "nao", "poster",  # chaves internas, nao rotulo
    "fileiras", "ordem-da-conta",  # partes do contexto no log de homeestado.c
    # Pontos de parada da volta condenada (descoberta.c, CONDENADA): so log.
    "antes de pedir os catalogos", "depois da atividade dos amigos",
    "depois do continuar assistindo", "depois dos catalogos",
    "depois dos manifestos", "esperando os catalogos", "lendo os manifestos",
    "crédit", "crédito",            # palavra procurada no capitulo do MKV
    "episodio", "episódio",         # palavra procurada no nome do video TMDB (extras.c)
    # Nome proprio e sigla: iguais nos dois idiomas.
    "IMDb", "Trakt", "YouTube", "PIN", "AI-powered",
    # Tabela de acentos -> letra base da normalizacao de titulo (trailerapple.c):
    # dado, nao rotulo.
    "ÀÁÂÃÄÅàáâãäåÈÉÊËèéêëÌÍÎÏìíîïÒÓÔÕÖØòóôõöøÙÚÛÜùúûüÝýÿÑñÇç",
    # ISO 639-2 do portugues (linguas.c). Virou candidato quando "por" entrou
    # na lista de palavras de portugues; e codigo de idioma, nao rotulo.
    "por",
    # O MESMO CODIGO EM MAIUSCULA (fontepref.c). E o valor da tabela TERMOS,
    # que traduz "portugues"/"brazilian"/"nacional" num codigo de idioma para
    # COMPARAR duas fontes entre si. Nunca chega na tela: a assinatura inteira
    # ("BR+DUB+POR") so aparece em printf de diagnostico.
    "POR",
    # Nao sao tela: cabecalho do arquivo de preferencias e duas linhas de log
    # que a heuristica de portugues nao tem como distinguir das frases.
    "# Fileiras da Home, escolha DESTE aparelho. Nunca e enviada para\n"
    "# a conta nem para o Trakt.\n",
    "%d addons · %d progressos · %d vistos · %d na lista · %d coleções%s",
    "hdr do pipeline: %s (fonte DV=%d)",
    # Tres marcos de video.c/video_tizen.c. O buffer e montado numa instrucao
    # e entregue a marco() na SEGUINTE, entao marco — que ja esta em
    # NAO_E_TELA — fica fora do contexto que a varredura le. Sao log.
    "pipeline erro: %.60s",
    # Marcador de cabecalho do sidecar de fontes do mkvass (arquivo em disco) e
    # a linha de estado do assrender, que so vai para o log. "fontes" casou
    # com a lista de palavras de portugues; nenhum dos dois chega na tela.
    "NVASS-FONTES-1\n",
    "%s; eventos=%d fontes=%d fontselect=%d cobertura=%lld-%lldms render=%lluus quadro=%zuB",
    "mkv: %d faixas lidas, %d legendas com idioma",
    "seek para %ds",
    # Os quatro motivos do diagnostico [col] (descoberta.c). Sao DADOS DE UM
    # VETOR, e nao argumento de printf: a varredura olha o que vem antes do
    # literal e ali so ha uma chave de inicializacao, entao NAO_E_TELA nao tem
    # como reconhece-los. Ficam em portugues de proposito — quem le e quem
    # abre uma issue, e a linha inteira em volta deles ja e portugues.
    "nenhuma colecao usa este addon",
    "colecao tem o addon, mas com outro tipo",
    "colecao tem addon e tipo, mas outro id de catalogo",
    "CASOU — nao engoliu porque o grupo esta oculto",
    # Os motivos de motivoTV() (faixas.c, #92): por que a faixa ASS ficou com
    # a TV em vez do overlay do app. Sao `return "..."` de uma funcao e so vao
    # para o printf "[legenda] faixa N -> TV: motivo" — o mesmo caso dos
    # motivos do [col] acima: log em portugues de proposito, para quem manda
    # o registro na issue.
    # #92, v1.4.7: se o VIDEO ja estava aberto quando um Range do mkvass
    # falhou (momentoVideo, mkvass.c), e o porque do video solto na linha da
    # pre-busca (player.c). Os dois so vao a printf de diagnostico, por um
    # `return`/ternario que a varredura nao liga ao printf.
    # Os motivos de gifcol_motivo_texto() (gifcolecao.c, #141): por que o
    # cartaz de colecao em foco nao anima. `return "..."` que so vai ao
    # printf "[gif] cartaz ... nao anima: motivo" — log para a issue.
    "sem focusGifUrl e a capa nao e GIF",
    "sem focusGifUrl e a capa ainda nao chegou",
    "arquivo do GIF ainda nao chegou",
    "veio GIF mas o arquivo saiu do disco",
    "o arquivo nao e GIF",
    "GIF de 1 quadro",
    "recusado pelo orcamento de animacao (ver a linha [gif] acima)",
    "este aparelho nao anima GIF (webOS ou TV de 1 GB)",
    "antes do video",
    "com video aberto",
    "teto vencido, o resto segue em segundo plano",
    "faixa inexistente",
    "sem URL da fonte",
    "mkvass ja desistiu desta faixa nesta sessao",
    "fonte nao e MKV (nao ha sonda)",
    "sonda do cabecalho ainda nao voltou",
    "sonda voltou sem par para esta faixa (ver [mkv] legendas da TV x arquivo)",
    "codec nao e ASS/SSA: a TV desenha bem",
    "sem ordinal no arquivo",
    # Os de motivoNoGo() (faixas.c): o mesmo caso, para a linha "[legenda]
    # mkvass no-go N (motivo)". O que vai a TELA sao os avisos com i18n().
    "nao e MKV",
    "servidor sem Range",
    "faixa nao e ASS",
    "sem indice da faixa",
    "sem CueRelativePosition",
}

def sem_corpo_em_js(txt):
    """Apaga o corpo dos EM_JS/EM_ASM, trocando por espaco e mantendo o \n.

    O corpo de um EM_JS e JAVASCRIPT escrito dentro de um arquivo .c, e esta
    varredura le C. As aspas simples do JS ("don't", 'texto') nao sao literais
    de caractere, e os literais de string que estao la sao codigo de ponte —
    nunca texto de tela, porque quem desenha e o lado C.

    Sem isto, cada comentario ou string nova dentro do EM_JS de video_tizen.c
    virava um "literal desenhado sem chave na tabela". Aconteceu em 17/09 com
    nove achados de uma vez, todos falsos, e o sinal verdadeiro se perde no meio
    de ruido assim.

    Troca por espacos em vez de remover para que os deslocamentos — e portanto
    os numeros de linha de todo o resto do arquivo — continuem valendo.
    """
    saida = list(txt)
    for m in re.finditer(r"(?<![A-Za-z0-9_])EM_(?:JS|ASM|ASM_INT|ASM_DOUBLE)\s*\(", txt):
        i = m.end() - 1          # no "(" de abertura
        nivel, j = 0, i
        while j < len(txt):
            if txt[j] == "(": nivel += 1
            elif txt[j] == ")":
                nivel -= 1
                if nivel == 0: break
            j += 1
        for k in range(m.start(), min(j + 1, len(txt))):
            if saida[k] != "\n":
                saida[k] = " "
    return "".join(saida)


def varrer():
    chaves = chaves_da_tabela()
    faltando_tabela, faltando_i18n = {}, {}
    faltando_funcao = {}
    for arq in sorted((RAIZ / "src").glob("*.c")):
        if arq.name == "idioma.c":
            continue
        txt = arq.read_text(encoding="utf-8")
        txt = sem_corpo_em_js(txt)
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

        for nome, ln in achar_rotulo_sem_i18n(txt, linha_de, contexto, RE_DESENHO,
                                              RE_NAO_E_TELA, arq.name):
            faltando_funcao.setdefault(nome, []).append(f"{arq.name}:{ln}")

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
            #
            # TERCEIRA PORTA, e ela existe por um caso medido: um literal
            # dentro de i18n() que NAO vai direto a uma funcao de desenho
            # (tipicamente `snprintf(buf, tam, "%s", i18n("..."))`) so era
            # conferido se a heuristica de portugues gostasse dele. Ela nao
            # gostou de "Setas: mover  ·  OK: entrar" — sem marcador de PT
            # obvio — e a chave saiu da tela em portugues no ingles inteiro.
            # Escrever i18n( E a declaracao de que aquilo e interface; depois
            # dela, adivinhar o idioma so serve para errar.
            direto = (RE_DESENHO.search(ctx) is not None
                      or RE_I18N.search(ctx) is not None)
            if not direto and not PT.search(s):
                continue
            # POR NOME INTEIRO, e nao por substring: "printf" casa dentro de
            # "snprintf", e foi assim que "A seguir" (posplay.c) — uma das
            # telas da foto do #12 — passou batido nesta propria ferramenta.
            # Todo texto montado com snprintf estava sendo descartado calado.
            if RE_NAO_E_TELA.search(ctx):
                continue
            if RE_CABECALHO_HTTP.match(s):
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
    return faltando_tabela, faltando_i18n, faltando_funcao

if __name__ == "__main__":
    tab, fmt, func = varrer()
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
    print()
    print("=== (3) funcao que devolve rotulo em portugues, chamada sem i18n(): %d ===" % len(func))
    for s in sorted(func):
        print("  %-72s %s" % (s, ", ".join(func[s][:6])))
    sys.exit(1 if (tab or fmt or func) else 0)
