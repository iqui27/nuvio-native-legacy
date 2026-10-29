#!/usr/bin/env python3
# MATERIAIS DO TEMA "TEXTURA" (deploy/app/art/texturas/): os recortes ruins da
# arte (chapados, sem material) sao trocados por estes. Gerado a PARTIR DE UM
# REPOSITORIO DE ASSETS CC0 — nada de rede em runtime: os JPG ficam no pacote.
#
# BASE: ambientCG "Paper002" (https://ambientcg.com/get?file=Paper002_1K-JPG.zip),
# licenca CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/). O
# arquivo original nao e versionado: baixe o zip e aponte TXT_BASE para o
# _Displacement.jpg dele (o grao do papel mora no displacement — o Color.jpg e
# quase chapado, 182-194), ou regenere a partir da copia da ultima geracao.
#
# O QUE FAZ: tira a LUMINANCIA do displacement (o grao), limita os pontos
# quentes a 1,6x (fibra isolada nao vira ruido na pilula), tinge cada
# variante num matiz de 30 em 30 graus (OKLCh, C 0,09, L normalizado para
# Y 0,42), e grava 13 JPG de 512: material-neutro.jpg e
# material-000..011.jpg. A tinta/veu do rotulo usa o par de percentis p10/p90
# MEDIDO DE CADA ARQUIVO (impresso no fim e copiado para NV_TX_ASSET_Y).
#
#   /tmp/pilvenv/bin/python3 tools/build-texturas.py /tmp/paper002/Paper002_1K-JPG_Color.jpg
#
# Precisa de Pillow (so aqui, no build; o app nao tem dependencia nova).
import math, sys, os
from PIL import Image

TXT_BASE = sys.argv[1] if len(sys.argv) > 1 else "deploy/app/art/texturas/material-neutro.jpg"
SAIDA = "deploy/app/art/texturas"
TAM = 512
Y_ALVO = 0.42          # luminancia relativa media de cada variante
C_TINTA = 0.09         # croma OKLCh da tinta
GRAFO_MAX = 1.6        # teto do grao (1 = media): fibra isolada nao vira ruido

def s2l(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def l2s(c):
    c = 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055
    return min(255, max(0, round(c * 255)))

def ylin(rgb):
    return 0.2126 * s2l(rgb[0]) + 0.7152 * s2l(rgb[1]) + 0.0722 * s2l(rgb[2])

def oklch(L, C, h):
    # OKLCh -> sRGB com caber no gamute (baixando o croma), como corviva.c
    for k in range(24):
        a, b = C * math.cos(h), C * math.sin(h)
        l_, m_, s_ = L + 0.3963377774 * a + 0.2158037573 * b, \
                     L - 0.1055613458 * a - 0.0638541728 * b, \
                     L - 0.0894841775 * a - 1.2914855480 * b
        l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
        r = +4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s
        g = -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s
        bb = -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s
        if 0 <= r <= 1 and 0 <= g <= 1 and 0 <= bb <= 1:
            return (l2s(r), l2s(g), l2s(bb))
        C *= 0.9
    return (0, 0, 0)

def tinta_com_y(h):
    # L tal que a tinta tenha a luminancia-alvo: sem escala depois (multiplicar
    # o grao por uma tinta ESCALA estouraria 255 nas fibras claras e viraria
    # branco puro, derrubando o matiz).
    lo, hi = 0.2, 0.95
    for _ in range(40):
        mid = (lo + hi) / 2
        if ylin(oklch(mid, C_TINTA, h)) < Y_ALVO:
            lo = mid
        else:
            hi = mid
    return oklch((lo + hi) / 2, C_TINTA, h)

def main():
    os.makedirs(SAIDA, exist_ok=True)
    base = Image.open(TXT_BASE).convert("RGB").resize((TAM, TAM), Image.LANCZOS)
    px = base.load()
    # luminancia do papel, normalizada para media 1
    ys = []
    for y in range(TAM):
        for x in range(TAM):
            g = ylin(px[x, y])
            ys.append(g)
    ys.sort()
    media = sum(ys) / len(ys)
    p10g, p90g = ys[len(ys) // 10] / media, ys[len(ys) * 9 // 10] / media
    variantes = [("material-neutro.jpg", None)] + [
        ("material-%03d.jpg" % h, math.radians(h)) for h in range(0, 360, 30)]
    for nome, h in variantes:
        tinta = (l2s(Y_ALVO),) * 3 if h is None else tinta_com_y(h)
        im = Image.new("RGB", (TAM, TAM))
        q = im.load()
        for y in range(TAM):
            for x in range(TAM):
                g = min(ylin(px[x, y]) / media, GRAFO_MAX)
                q[x, y] = tuple(min(255, int(c * g + 0.5)) for c in tinta)
        im.save(os.path.join(SAIDA, nome), quality=80)
        print("%s: tinta #%02x%02x%02x" % (nome, *tinta))
    # AS CONSTANTES que o runtime usa (NV_TX_ASSET_Y, ajustes.c): medidas do
    # ARQUIVO FINAL de CADA variante, nao da conta intermediaria — o clamp e o
    # JPEG contam, e cada matiz mediu um par diferente (p90 0,69-0,79).
    for h in [None] + list(range(0, 360, 30)):
        n = "material-neutro.jpg" if h is None else "material-%03d.jpg" % h
        im = Image.open(os.path.join(SAIDA, n)).convert("RGB")
        px = im.load()
        ys = sorted(ylin(px[x, y]) for y in range(TAM) for x in range(TAM))
        print("NV_TX_ASSET_Y %s {%.4f, %.4f}" % (n, ys[len(ys) // 10], ys[len(ys) * 9 // 10]))

main()
