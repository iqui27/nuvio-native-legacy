#!/usr/bin/env python3
# SPIKE S2 (#92): quanto custa colher a faixa ASS de um MKV por HTTP Range,
# sem baixar o filme. Estrategia: SeekHead -> Cues; CuePoints da faixa de
# legenda (CueTrack == numero da faixa S_TEXT/ASS) dao CueClusterPosition +
# CueRelativePosition; cada bloco e lido por um Range pequeno (cabecalho do
# cluster + o bloco). Conta bytes e pedidos. Se o arquivo NAO tiver cues
# para a faixa de legenda, cai no plano B: varrer clusters com janelas.
import sys, urllib.request, struct

URL = sys.argv[1]
bytes_lidos = 0; pedidos = 0

import os
def rng(ini, n):
    """Range real quando URL e http; leitura posicional quando e caminho
    (mesma contagem de bytes/pedidos — o que se mede e a semantica de Range)."""
    global bytes_lidos, pedidos
    if URL.startswith("http"):
        req = urllib.request.Request(URL, headers={"Range": "bytes=%d-%d" % (ini, ini + n - 1)})
        with urllib.request.urlopen(req) as r: d = r.read()
    else:
        with open(URL, "rb") as f: f.seek(ini); d = f.read(n)
    bytes_lidos += len(d); pedidos += 1
    return d

def tamanho_total():
    if URL.startswith("http"):
        req = urllib.request.Request(URL, method="HEAD")
        with urllib.request.urlopen(req) as r: return int(r.headers["Content-Length"])
    return os.path.getsize(URL)

def vint(b, i):
    """le um vint EBML em b[i:], devolve (valor, tamanho, mascara_removida)"""
    first = b[i]; ln = 1; mask = 0x80
    while ln <= 8 and not (first & mask): ln += 1; mask >>= 1
    v = first & (mask - 1)
    for k in range(1, ln): v = (v << 8) | b[i + k]
    return v, ln

def eid(b, i):
    """id EBML: vint com os bits de marcacao mantidos"""
    first = b[i]; ln = 1; mask = 0x80
    while ln <= 8 and not (first & mask): ln += 1; mask >>= 1
    v = 0
    for k in range(ln): v = (v << 8) | b[i + k]
    return v, ln

def elems(b, i, fim):
    """gera (id, dados_ini, dados_tam, proximo) dentro de b[i:fim]"""
    while i < fim:
        id_, l1 = eid(b, i); sz, l2 = vint(b, i + l1)
        di = i + l1 + l2
        yield id_, di, sz
        i = di + sz

def u(b):
    v = 0
    for x in b: v = (v << 8) | x
    return v

# --- cabecalho: EBML + Segment ------------------------------------------------
cab = rng(0, 4096)
i = 0
id_, l1 = eid(cab, 0); sz, l2 = vint(cab, l1); i = l1 + l2 + sz         # EBML header
id_, l1 = eid(cab, i); assert id_ == 0x18538067, "nao e Segment"
segsz, l2 = vint(cab, i + l1); seg_ini = i + l1 + l2
print("Segment comeca em", seg_ini)

# --- SeekHead: onde estao Tracks e Cues -------------------------------------
pos = {}
janela = cab
j = seg_ini
for id_, di, sz in elems(janela, j, len(janela)):
    if id_ == 0x114D9B74:   # SeekHead
        for sid, sdi, ssz in elems(janela, di, di + sz):
            if sid == 0x4DBB:   # Seek
                sk = None; sp = None
                for tid, tdi, tsz in elems(janela, sdi, sdi + ssz):
                    if tid == 0x53AB: sk = u(janela[tdi:tdi + tsz])
                    if tid == 0x53AC: sp = u(janela[tdi:tdi + tsz])
                if sk is not None: pos[sk] = seg_ini + sp
        break
print("SeekHead:", {hex(k): v for k, v in pos.items()})

# --- Tracks: numero da faixa ASS --------------------------------------------
tr_pos = pos.get(0x1654AE6B)
b = rng(tr_pos, 8192)
id_, l1 = eid(b, 0); sz, l2 = vint(b, l1)
faixa_ass = None; tipos = {}
for tid, tdi, tsz in elems(b, l1 + l2, l1 + l2 + sz):
    if tid == 0xAE:   # TrackEntry
        num = None; codec = None; tipo = None
        for eid_, edi, esz in elems(b, tdi, tdi + tsz):
            if eid_ == 0xD7: num = u(b[edi:edi + esz])
            if eid_ == 0x86: codec = b[edi:edi + esz].decode()
            if eid_ == 0x83: tipo = u(b[edi:edi + esz])
        tipos[num] = (tipo, codec)
        if codec and codec.startswith("S_TEXT/ASS"): faixa_ass = num
print("faixas:", tipos, "-> ASS =", faixa_ass)

# --- Cues ---------------------------------------------------------------------
cues_pos = pos.get(0x1C53BB6B)
total = tamanho_total()
print("Cues em", cues_pos, "tamanho do arquivo", total)
b = rng(cues_pos, 12)
id_, l1 = eid(b, 0); sz, l2 = vint(b, l1)
b = rng(cues_pos, l1 + l2 + sz)
print("Cues: %d bytes" % (l1 + l2 + sz))
pontos = []   # (cluster_pos, rel_pos, tempo)
n_cues_total = 0
for cid, cdi, csz in elems(b, l1 + l2, l1 + l2 + sz):
    if cid != 0xBB: continue
    tempo = None
    for pid, pdi, psz in elems(b, cdi, cdi + csz):
        if pid == 0xB3: tempo = u(b[pdi:pdi + psz])
        if pid == 0xB7:   # CueTrackPositions
            trk = None; cp = None; rp = None
            for qid, qdi, qsz in elems(b, pdi, pdi + psz):
                if qid == 0xF7: trk = u(b[qdi:qdi + qsz])
                if qid == 0xF1: cp = u(b[qdi:qdi + qsz])
                if qid == 0xF0: rp = u(b[qdi:qdi + qsz])
            n_cues_total += 1
            if trk == faixa_ass: pontos.append((seg_ini + cp, rp, tempo))
print("cue points: %d no total, %d da faixa ASS, %d com CueRelativePosition" %
      (n_cues_total, len(pontos), sum(1 for p in pontos if p[1] is not None)))

# --- colher os blocos ---------------------------------------------------------
falas = 0; bytes_ass = 0
if pontos and all(p[1] is not None for p in pontos):
    # Um Range por bloco: cabecalho do BlockGroup/SimpleBlock + o bloco. O
    # tamanho do bloco so se sabe lendo o cabecalho, entao le uma janela
    # de 256 B e completa se faltar.
    ultimo_cluster = None
    cab_cluster = {}
    for cp, rp, tempo in pontos:
        # CueRelativePosition conta a partir dos DADOS do Cluster (depois do
        # id + tamanho), nao do inicio do elemento. Um Range de 12 B por
        # cluster resolve isso; clusters repetidos nao pagam de novo.
        if cp not in cab_cluster:
            h = rng(cp, 12); cid, l1 = eid(h, 0); csz, l2 = vint(h, l1)
            assert cid == 0x1F43B675, "cue nao aponta para Cluster"
            cab_cluster[cp] = l1 + l2
        ini = cp + cab_cluster[cp] + rp
        w = rng(ini, 256)
        id_, l1 = eid(w, 0); sz, l2 = vint(w, l1)
        need = l1 + l2 + sz
        if need > len(w): w += rng(ini + len(w), need - len(w))
        if id_ == 0xA0:   # BlockGroup -> Block dentro
            for gid, gdi, gsz in elems(w, l1 + l2, need):
                if gid == 0xA1:
                    trk, lt = vint(w, gdi)
                    if trk == faixa_ass: falas += 1; bytes_ass += gsz
        elif id_ == 0xA3:
            trk, lt = vint(w, l1 + l2)
            if trk == faixa_ass: falas += 1; bytes_ass += sz
    print("colhidas %d falas, %d bytes de ASS" % (falas, bytes_ass))
else:
    print("SEM cues por bloco de legenda: seria preciso varrer clusters (plano B)")

print("TOTAL: %d pedidos, %d bytes lidos = %.2f%% do arquivo" % (pedidos, bytes_lidos, 100.0 * bytes_lidos / total))
