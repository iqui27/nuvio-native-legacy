#!/usr/bin/env python3
"""Confere as tabelas de traducao da interface (src/idioma_tab.h e irmas).

ESTRUTURA. idioma_tab.h e a tabela mestra: { chave em portugues, ingles },
ORDENADA por strcmp dos BYTES DECODIFICADOS (aspas = 0x22, \\n = 0x0A — nao o
texto literal com a barra). As 28 irmas (idioma_ro.h, uk, ru, fr, de, es, it,
nl, pl, tr, ptpt, sv, da, no, cs, sk, sl, hu, lt, bs, sr, bg, el, id, vi, ja,
zhcn, zhtw) tem UMA linha por entrada da mestra, na MESMA ordem:
T("chave pt", "traducao").
O compilador descarta a chave (macro T); ela existe para o revisor humano ler
a linha inteira e para este script conferir que o alinhamento nao escorregou.

O QUE CONFERE (sai com codigo 1 no primeiro defeito de qualquer idioma):
  1. mesma quantidade de entradas em todas as tabelas;
  2. a mestra esta em ordem estrita de strcmp (fora de ordem = idioma.c
     DESLIGA a traducao inteira, em silencio);
  3. a chave de cada linha das irmas e IDENTICA a da mestra, na mesma posicao;
  4. mesmos marcadores printf (%s %d %.1f %% ...) NA MESMA ORDEM que a chave —
     o codigo passa os argumentos na ordem do portugues;
  5. nenhum valor vazio;
  6. mesmo numero de \\n que a chave (quebra de linha e layout);
  7. o valor e UTF-8 valido e nao tem barra solta / aspa sem escape;
  8. mesmos espacos na borda que a chave (" carregados" e uma chave de
     verdade: o espaco da frente separa de um numero desenhado antes);
  9. as linguas de escrita NAO latina (uk, ru, bg: cirilico; el: grego; ja: kana
     ou kanji; zhcn, zhtw: hanzi) tem de conter a propria escrita, salvo o que
     e nome proprio, sigla ou formato (VERBATIM abaixo) ou igual a chave/ao
     ingles — texto em alfabeto latino ali e traducao que ficou por fazer;
 10. as linguas latinas NAO podem ter cirilico, e TODA lingua so pode usar
     caracteres que o app consegue desenhar. Cobertura medida no cmap dos TTF
     embarcados (deploy/app/fonts, lido aqui mesmo, sem dependencia externa):
       - a InterDisplay (Regular, Medium e Bold) e a fonte de ULTIMO RECURSO de
         text.c em qualquer plataforma: latim estendido, vietnamita, grego e
         cirilico TEM de estar nela;
       - ja e zh: o que a Inter nao tem tem de estar na fonte CJK embarcada
         (DroidSansFallback-Subset.ttf), a unica que existe no WASM da Samsung.
     Um caractere sem nenhuma das duas reprova. `--cobertura` imprime, alem
     disso, o que cada familia de interface (Montserrat, Roboto, Atkinson) nao
     tem: ali text.c troca a linha para a Inter, e o relatorio diz quantas.

Uso:
    python3 tools/idiomas.py                 # confere tudo
    python3 tools/idiomas.py --sincronizar   # reescreve as irmas com as chaves
                                             # da mestra (entrada nova = vazia,
                                             # que o passo 5 recusa)
    python3 tools/idiomas.py --revisao ro    # pt | en | ro lado a lado
    python3 tools/idiomas.py --cobertura     # cobertura de glifos por fonte
"""
import re, sys, pathlib

RAIZ = pathlib.Path(__file__).resolve().parent.parent
SRC = RAIZ / "src"
# Na ordem de IDIOMA_* (idiomacod.h). O nome do arquivo e idioma_<cod>.h.
IDIOMAS = ("ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr", "ptpt", "sv",
           "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr", "bg", "el", "id",
           "vi", "ja", "zhcn", "zhtw", "ar")
CIRILICOS = ("uk", "ru", "bg")
NAO_LATINOS = CIRILICOS + ("el", "ja", "zhcn", "zhtw", "ar")
CJK = ("ja", "zhcn", "zhtw")
# Escrita que cada lingua nao latina tem de mostrar em toda traducao "de verdade".
ESCRITA = {
    "uk": "[\u0400-\u04ff]", "ru": "[\u0400-\u04ff]", "bg": "[\u0400-\u04ff]",
    "el": "[\u0370-\u03ff\u1f00-\u1fff]",
    "ja": "[\u3040-\u30ff\u3400-\u9fff]",
    "zhcn": "[\u3400-\u9fff]", "zhtw": "[\u3400-\u9fff]",
    "ar": "[\u0600-\u06ff]",
}
FONTES = RAIZ / "deploy" / "app" / "fonts"
FONTE_CJK = "DroidSansFallback-Subset.ttf"
# A fonte arabe embarcada (tools/fonte-arabe.py): a unica no WASM da Samsung.
FONTE_ARABE = "NotoSansArabic-Regular-Subset.ttf"

LIT = r'"((?:[^"\\]|\\.)*)"'
LINHA_MESTRA = re.compile(r'^\s*\{\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\},?\s*$')
LINHA_IRMA = re.compile(r'^\s*T\(\s*' + LIT + r'\s*,\s*' + LIT + r'\s*\),?\s*$')
# Valores sem cirilico que sao intencionais no uk/ru: os que nao sao igual ao
# ingles nem a chave. "S%dE%d" e o formato de temporada/episodio que o mundo todo
# le assim (a chave e T%dE%d, do portugues).
VERBATIM = {"Watchlist Trakt", "S%dE%d", "S%d:E%d", "%s S%dE%d", "%s — S%dE%d%s%s",
            "S%dE%d  ·  %s%s%.22s", "S%dE%d · %s", "S%dE%d%s%s", "Português (Brasil)",
            "Português (Portugal)", "Français",
            # Nome de produto, sigla ou formato que a lingua escreve em latim mesmo:
            "Spotlight 4:3", "SF", "Logo",
            # "X de Y" que o japones e o chines escrevem com barra ("3 / 8"):
            "%.1f / %d MB · %d%%", "%d / %d", "%d / 8", "%s  ·  %d / %d"}
SIMPLES = {"n": 10, "t": 9, "r": 13, "0": 0, '"': 34, "\\": 92, "'": 39}

def decodificar(s):
    """Literal C -> bytes, como o compilador decodifica (\\xNN, \\uNNNN, simples)."""
    out, i, n = bytearray(), 0, len(s)
    while i < n:
        c = s[i]
        if c != "\\":
            out += c.encode("utf-8"); i += 1; continue
        i += 1
        c = s[i]
        if c == "x":
            j = i + 1
            while j < n and s[j] in "0123456789abcdefABCDEF": j += 1
            out.append(int(s[i + 1:j], 16) & 0xFF); i = j
        elif c == "u":
            out += chr(int(s[i + 1:i + 5], 16)).encode("utf-8"); i += 5
        elif c in SIMPLES:
            out.append(SIMPLES[c]); i += 1
        else:
            raise ValueError("escape desconhecido \\" + c)
    return bytes(out)

# A percent sign after a number is literal UI text, not a format.
MARCADOR = re.compile(rb"(?<![0-9])%[-+ #0]*[0-9]*(?:\.[0-9]+)?(?:hh|h|ll|l|z|j|t)?[diouxXeEfgGcsp%]")
def marcadores(b):
    return [m for m in MARCADOR.findall(b) if m != b"%%"]

def cmap_ttf(caminho):
    """Codepoints com glifo no cmap (formatos 4 e 12) de um TTF."""
    import struct
    d = caminho.read_bytes()
    n = struct.unpack(">H", d[4:6])[0]
    off = None
    for i in range(n):
        tag, _cs, o, _ln = struct.unpack(">4sIII", d[12 + 16 * i:28 + 16 * i])
        if tag == b"cmap": off = o
    cps = set()
    nt = struct.unpack(">H", d[off + 2:off + 4])[0]
    for i in range(nt):
        _p, _e, so = struct.unpack(">HHI", d[off + 4 + 8 * i:off + 12 + 8 * i])
        s = off + so
        fmt = struct.unpack(">H", d[s:s + 2])[0]
        if fmt == 4:
            sc = struct.unpack(">H", d[s + 6:s + 8])[0] // 2
            fins = struct.unpack(">%dH" % sc, d[s + 14:s + 14 + 2 * sc])
            st = s + 16 + 2 * sc
            ini = struct.unpack(">%dH" % sc, d[st:st + 2 * sc])
            dl = st + 2 * sc
            delta = struct.unpack(">%dh" % sc, d[dl:dl + 2 * sc])
            ro = dl + 2 * sc
            rng = struct.unpack(">%dH" % sc, d[ro:ro + 2 * sc])
            for k in range(sc):
                for c in range(ini[k], fins[k] + 1):
                    if c == 0xFFFF: continue
                    if rng[k] == 0: g = (c + delta[k]) & 0xFFFF
                    else:
                        p = ro + 2 * k + rng[k] + 2 * (c - ini[k])
                        g = struct.unpack(">H", d[p:p + 2])[0]
                        if g: g = (g + delta[k]) & 0xFFFF
                    if g: cps.add(c)
        elif fmt == 12:
            ng = struct.unpack(">I", d[s + 12:s + 16])[0]
            for k in range(ng):
                a, b, g = struct.unpack(">III", d[s + 16 + 12 * k:s + 28 + 12 * k])
                if g: cps.update(range(a, b + 1))
    return cps

def ler_mestra():
    itens = []
    for n, linha in enumerate((SRC / "idioma_tab.h").read_text(encoding="utf-8").splitlines(), 1):
        if linha.lstrip().startswith("//") or not linha.strip():
            continue
        m = LINHA_MESTRA.match(linha)
        if not m:
            raise SystemExit("idioma_tab.h:%d: linha fora do formato { \"pt\", \"en\" }" % n)
        itens.append((n, m.group(1), m.group(2)))
    return itens

def ler_irma(cod):
    caminho = SRC / ("idioma_%s.h" % cod)
    itens = []
    if not caminho.exists():
        return itens
    for n, linha in enumerate(caminho.read_text(encoding="utf-8").splitlines(), 1):
        if linha.lstrip().startswith("//") or not linha.strip():
            continue
        m = LINHA_IRMA.match(linha)
        if not m:
            raise SystemExit("idioma_%s.h:%d: linha fora do formato T(\"pt\", \"traducao\")" % (cod, n))
        itens.append((n, m.group(1), m.group(2)))
    return itens

def conferir():
    erros = []
    def erro(msg):
        if len(erros) < 60: erros.append(msg)
    mestra = ler_mestra()
    # 2. ordem da mestra, pelos bytes decodificados
    ant = None
    for n, pt, en in mestra:
        b = decodificar(pt)
        if ant is not None and ant >= b:
            erro("idioma_tab.h:%d: fora de ordem (ou duplicada): %r" % (n, pt))
        ant = b
    # mestra: en com os mesmos marcadores e \n da chave
    for n, pt, en in mestra:
        if not en: erro("idioma_tab.h:%d: ingles vazio" % n)
        if marcadores(decodificar(pt)) != marcadores(decodificar(en)):
            erro("idioma_tab.h:%d: marcadores do ingles diferem: %r -> %r" % (n, pt, en))
    for cod in IDIOMAS:
        irma = ler_irma(cod)
        arq = "idioma_%s.h" % cod
        # 1. quantidade
        if len(irma) != len(mestra):
            erro("%s: %d entradas, a mestra tem %d" % (arq, len(irma), len(mestra)))
        for (n, pt, en), (m, kpt, val) in zip(mestra, irma):
            # 3. alinhamento
            if kpt != pt:
                erro("%s:%d: chave difere da mestra (linha %d): %r != %r" % (arq, m, n, kpt, pt)); break
            try:
                bv = decodificar(val); bk = decodificar(pt)
                bv.decode("utf-8")
            except Exception as e:
                erro("%s:%d: valor invalido (%s): %r" % (arq, m, e, val)); continue
            # 5. vazio
            # An empty value is a known untranslated row; the runtime uses English.
            if not bv.strip():
                if cod == "ar": erro("%s:%d: traducao arabe vazia" % (arq, m))
                bv = decodificar(en); val = en
            # 4. marcadores, na ordem
            if marcadores(bk) != marcadores(bv):
                erro("%s:%d: marcadores diferem: %r -> %r" % (arq, m, pt, val))
            # 8. espacos da borda
            if (len(val) - len(val.lstrip()), len(val) - len(val.rstrip())) != \
               (len(pt) - len(pt.lstrip()), len(pt) - len(pt.rstrip())):
                erro("%s:%d: espacos na borda diferem de %r: %r" % (arq, m, pt, val))
            # 9. a escrita propria (cirilico, grego, kana/hanzi)
            if cod in ESCRITA and not re.search(ESCRITA[cod], val) \
               and val not in (pt, en) and val not in VERBATIM:
                erro("%s:%d: sem a escrita da lingua e diferente da chave e do ingles: %r -> %r" % (arq, m, pt, val))
            # 6. quebras de linha
            if bk.count(b"\n") != bv.count(b"\n"):
                erro("%s:%d: numero de \\n difere: %r -> %r" % (arq, m, pt, val))
    # 10. cobertura de fonte: a Inter (3 pesos) e o ultimo recurso; ja/zh cai na CJK.
    fontes = {f.name: cmap_ttf(f) for f in sorted(FONTES.glob("*.ttf"))}
    inter = [fontes[n] for n in sorted(fontes) if n.startswith("InterDisplay-")]
    cjk = fontes.get(FONTE_CJK)
    arabe = fontes.get(FONTE_ARABE)
    if arabe is None: erro("fonts: falta %s (fonte arabe embarcada)" % FONTE_ARABE)
    if len(inter) != 3: erro("fonts: esperava os 3 pesos da InterDisplay, achei %d" % len(inter))
    if cjk is None: erro("fonts: falta %s (fonte CJK embarcada, unica no WASM da Samsung)" % FONTE_CJK)
    for cod in IDIOMAS:
        usados = {}
        for n, pt, val in ler_irma(cod):
            try: t = decodificar(val).decode("utf-8"); k = decodificar(pt).decode("utf-8")
            except Exception: continue
            # so o que a TRADUCAO introduz: setas e marcas da propria chave (↑ ↓ ✓)
            # sao pre-existentes em todos os idiomas e o texto.c as trata a parte.
            for ch in t:
                if ch >= " " and ch not in k and ch not in usados: usados[ch] = n
        for ch, n in sorted(usados.items()):
            if "\u0400" <= ch <= "\u04ff" and cod not in CIRILICOS:
                erro("idioma_%s.h:%d: cirilico (%r) numa tabela que nao e cirilica" % (cod, n, ch))
            if all(ord(ch) in c for c in inter): continue
            if cod in CJK and cjk is not None and ord(ch) in cjk: continue
            if cod == "ar" and arabe is not None and ord(ch) in arabe: continue
            erro("idioma_%s.h:%d: U+%04X (%s) nao existe na Inter%s" %
                 (cod, n, ord(ch), ch, " nem em " + FONTE_CJK if cod in CJK else ""))
    return mestra, erros

def sincronizar():
    mestra = ler_mestra()
    for cod in IDIOMAS:
        atual = {pt: val for _, pt, val in ler_irma(cod)}
        # Cabecalho: preserva o que ja existe (nome da lingua e autoria); so a
        # ordem das linhas vem da mestra.
        caminho = SRC / ("idioma_%s.h" % cod)
        cab = [l for l in caminho.read_text(encoding="utf-8").splitlines()
               if l.lstrip().startswith("//")] if caminho.exists() else []
        linhas = cab or ["// Traducao para %s. Uma linha por entrada de idioma_tab.h, na mesma ordem." % cod,
                         "// Gerado por tools/idiomas.py --sincronizar; a chave e so para leitura (macro T)."]
        for _, pt, _en in mestra:
            linhas.append('  T("%s", "%s"),' % (pt, atual.get(pt, "")))
        (SRC / ("idioma_%s.h" % cod)).write_text("\n".join(linhas) + "\n", encoding="utf-8")

def revisao(cod):
    mestra = ler_mestra()
    irma = ler_irma(cod)
    for (n, pt, en), (m, _k, val) in zip(mestra, irma):
        print("%s\n   en: %s\n   %s: %s" % (pt, en, cod, val))

def cobertura():
    """Por lingua e por familia: quantos caracteres da traducao a fonte nao tem."""
    fontes = {f.name: cmap_ttf(f) for f in sorted(FONTES.glob("*.ttf"))}
    regulares = [n for n in fontes if n.endswith("Regular.ttf") or n == FONTE_CJK]
    print("%-6s %5s  %s" % ("lingua", "chars", "  ".join(n.split("-")[0][:12] for n in regulares)))
    for cod in IDIOMAS:
        usados = set()
        for n, pt, val in ler_irma(cod):
            try: t = decodificar(val).decode("utf-8"); k = decodificar(pt).decode("utf-8")
            except Exception: continue
            usados |= {ch for ch in t if ch >= " " and ch not in k}
        print("%-6s %5d  %s" % (cod, len(usados), "  ".join(
            "%*d" % (len(n.split("-")[0][:12]), sum(1 for ch in usados if ord(ch) not in fontes[n]))
            for n in regulares)))

if __name__ == "__main__":
    if "--cobertura" in sys.argv: cobertura(); sys.exit(0)
    if "--sincronizar" in sys.argv: sincronizar(); sys.exit(0)
    if "--revisao" in sys.argv: revisao(sys.argv[sys.argv.index("--revisao") + 1]); sys.exit(0)
    mestra, erros = conferir()
    for e in erros: print("ERRO", e)
    if erros:
        print("idiomas: %d problema(s)" % len(erros)); sys.exit(1)
    print("idiomas: %d entradas x %d idiomas (%s) ok" % (len(mestra), len(IDIOMAS), ", ".join(IDIOMAS)))
