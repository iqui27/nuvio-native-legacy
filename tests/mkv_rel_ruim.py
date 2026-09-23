#!/usr/bin/env python3
# Fixture do tests/mkvass.sh: copia um MKV trocando o CueRelativePosition dos
# CuePoints de UMA faixa por 1 (mesma largura de bytes, entao nada mais se
# move). O indice continua dizendo em que Cluster o bloco esta, mas a posicao
# dentro dele cai no meio do Timestamp: e o "CuePoint com posicao invalida"
# que o mkvass precisa resolver lendo o Cluster inteiro.
#
#   python3 tests/mkv_rel_ruim.py <entrada.mkv> <saida.mkv> <faixa>
import sys

def vint(b, o, mascara=True):
    p = b[o]; w = 1; m = 0x80
    while w <= 8 and not (p & m):
        w += 1; m >>= 1
    v = (p & (m - 1)) if mascara else p
    for k in range(1, w):
        v = (v << 8) | b[o + k]
    return v, w

def elementos(b, ini, fim):
    o = ini
    while o < fim:
        eid, wi = vint(b, o, False)
        tam, wt = vint(b, o + wi)
        if tam == (1 << (7 * wt)) - 1:   # tamanho desconhecido: nao ha como pular
            return
        d = o + wi + wt
        yield eid, d, tam
        o = d + tam

entrada, saida, faixa = sys.argv[1], sys.argv[2], int(sys.argv[3])
b = bytearray(open(entrada, "rb").read())
trocados = 0
for eid, d, t in elementos(b, 0, len(b)):
    if eid != 0x18538067:                 # Segment
        continue
    for cid, cd, ct in elementos(b, d, d + t):
        if cid != 0x1C53BB6B:             # Cues
            continue
        for pid, pd, pt in elementos(b, cd, cd + ct):
            if pid != 0xBB:               # CuePoint
                continue
            for kid, kd, kt in elementos(b, pd, pd + pt):
                if kid != 0xB7:           # CueTrackPositions
                    continue
                filhos = list(elementos(b, kd, kd + kt))
                trk = [int.from_bytes(b[x:x + n], "big") for i, x, n in filhos if i == 0xF7]
                if trk != [faixa]:
                    continue
                for i, x, n in filhos:
                    if i == 0xF0:         # CueRelativePosition
                        b[x:x + n] = (1).to_bytes(n, "big")
                        trocados += 1
open(saida, "wb").write(b)
print("mkv_rel_ruim: %d CueRelativePosition trocados por 1" % trocados)
sys.exit(0 if trocados else 1)
