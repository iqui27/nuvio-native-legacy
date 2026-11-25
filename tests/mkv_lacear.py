#!/usr/bin/env python3
"""Rewrite an MKV so one audio track is Xiph-laced with no DefaultDuration.

Usage: mkv_lacear.py <in.mkv> <out.mkv> <CodecID, e.g. A_OPUS> [frames per block]

Reproduces mkvmerge-style Opus tracks seen on the TV (16/11): only the first
frame of each block carries a timestamp in FFmpeg, the others come with none
and no length. Input: an FFmpeg-written MKV with known sizes and no CRC-32
(-write_crc32 0). Every cluster is rebuilt; Cues and SeekHead positions are
recomputed (8-byte fixed width), CueRelativePosition is dropped.
"""
import struct
import sys

SEGMENT, CLUSTER, TRACKS, TRACKENTRY = 0x18538067, 0x1F43B675, 0x1654AE6B, 0xAE
CUES, CUEPOINT, CUETRACKPOS, SEEKHEAD, SEEK = 0x1C53BB6B, 0xBB, 0xB7, 0x114D9B74, 0x4DBB
SIMPLEBLOCK, TRACKNUMBER, CODECID, DEFAULTDURATION = 0xA3, 0xD7, 0x86, 0x23E383
CUECLUSTERPOS, CUERELPOS, SEEKID, SEEKPOS = 0xF1, 0xF0, 0x53AB, 0x53AC
MASTERS = {SEGMENT, CLUSTER, TRACKS, TRACKENTRY, CUES, CUEPOINT, CUETRACKPOS, SEEKHEAD, SEEK}


def vint(b, i):
    first = b[i]
    n = 1
    while n <= 8 and not first & (0x80 >> (n - 1)):
        n += 1
    return b[i:i + n], n


def ler(b, i, fim):
    out = []
    while i < fim:
        idb, n = vint(b, i)
        eid = int.from_bytes(idb, 'big')
        i += n
        szb, n = vint(b, i)
        sz = int.from_bytes(szb, 'big') & ((1 << (7 * n)) - 1)
        i += n
        if sz == (1 << (7 * n)) - 1:
            raise SystemExit('unknown sizes are not supported')
        if eid in MASTERS:
            out.append([eid, ler(b, i, i + sz)])
        else:
            out.append([eid, b[i:i + sz]])
        i += sz
    return out


def tam(n):
    for k in range(1, 9):
        if n < (1 << (7 * k)) - 1:
            return bytes([(1 << (8 - k)) | (n >> (8 * (k - 1)))]) + n.to_bytes(k, 'big')[1:] if k > 1 else bytes([0x80 | n])
    raise ValueError(n)


def eid_bytes(e):
    return e.to_bytes((e.bit_length() + 7) // 8, 'big')


def escrever(els):
    out = bytearray()
    for eid, v in els:
        corpo = escrever(v) if isinstance(v, list) else bytes(v)
        out += eid_bytes(eid) + tam(len(corpo)) + corpo
    return bytes(out)


def uint8(v):
    return v.to_bytes(8, 'big')


def faixa_de(bloco):
    tb, n = vint(bloco, 0)
    return int.from_bytes(tb, 'big') & ((1 << (7 * n)) - 1), n


def lacear(blocos, ntrack_len):
    """Xiph-lace several SimpleBlocks of one track into one."""
    cab = bytearray(blocos[0][:ntrack_len + 3])
    cab[ntrack_len + 2] = (cab[ntrack_len + 2] & ~0x06) | 0x02
    quadros = [bytes(b[ntrack_len + 3:]) for b in blocos]
    lac = bytearray([len(quadros) - 1])
    for q in quadros[:-1]:
        n = len(q)
        while n >= 255:
            lac.append(255)
            n -= 255
        lac.append(n)
    return bytes(cab) + bytes(lac) + b''.join(quadros)


def main():
    src, dst, codec = sys.argv[1], sys.argv[2], sys.argv[3].encode()
    por = int(sys.argv[4]) if len(sys.argv) > 4 else 5
    b = open(src, 'rb').read()
    top = ler(b, 0, len(b))
    seg = next(e for e in top if e[0] == SEGMENT)[1]
    alvo = None
    for e in seg:
        if e[0] != TRACKS:
            continue
        for te in e[1]:
            if te[0] != TRACKENTRY:
                continue
            num = next((int.from_bytes(v, 'big') for k, v in te[1] if k == TRACKNUMBER), None)
            cid = next((bytes(v).rstrip(b'\0') for k, v in te[1] if k == CODECID), b'')
            if cid == codec:
                alvo = num
                te[1] = [x for x in te[1] if x[0] != DEFAULTDURATION]
    if alvo is None:
        raise SystemExit('track %s not found' % codec.decode())
    laceados = 0
    for e in seg:
        if e[0] != CLUSTER:
            continue
        novos, grupo, nlen = [], [], 0

        def fechar():
            nonlocal grupo, laceados
            if len(grupo) == 1:
                novos.append([SIMPLEBLOCK, grupo[0]])
            elif grupo:
                novos.append([SIMPLEBLOCK, lacear(grupo, nlen)])
                laceados += 1
            grupo = []
        for k, v in e[1]:
            if k == SIMPLEBLOCK:
                t, n = faixa_de(v)
                if t == alvo:
                    nlen = n
                    grupo.append(bytes(v))
                    if len(grupo) == por:
                        fechar()
                    continue
            # A block of another track does not end the group: frames keep
            # their order inside the track, which is all lacing needs.
            novos.append([k, v])
        fechar()
        e[1] = novos
    # Positions: fixed 8-byte values so sizes do not move between passes.
    for e in seg:
        if e[0] == CUES:
            for cp in e[1]:
                for ctp in cp[1]:
                    if ctp[0] == CUETRACKPOS:
                        ctp[1] = [x for x in ctp[1] if x[0] != CUERELPOS]
                        for x in ctp[1]:
                            if x[0] == CUECLUSTERPOS:
                                x[1] = uint8(int.from_bytes(x[1], 'big'))
        if e[0] == SEEKHEAD:
            for s in e[1]:
                for x in s[1]:
                    if x[0] == SEEKPOS:
                        x[1] = uint8(0)
    # Old cluster positions (from the input), in order -> new ones.
    segi = b.find(eid_bytes(SEGMENT))
    _, n = vint(b, segi + 4)
    p0, j, antigos = segi + 4 + n, segi + 4 + n, []
    while j < len(b):
        idb, n1 = vint(b, j)
        szb, n2 = vint(b, j + n1)
        if int.from_bytes(idb, 'big') == CLUSTER:
            antigos.append(j - p0)
        j += n1 + n2 + (int.from_bytes(szb, 'big') & ((1 << (7 * n2)) - 1))
    pos, novos_cl, lvl1 = 0, [], {}
    for e in seg:
        if e[0] == CLUSTER:
            novos_cl.append(pos)
        lvl1.setdefault(e[0], pos)
        pos += len(escrever([e]))
    mapa = dict(zip(antigos, novos_cl))
    for e in seg:
        if e[0] == CUES:
            for cp in e[1]:
                for ctp in cp[1]:
                    if ctp[0] == CUETRACKPOS:
                        for x in ctp[1]:
                            if x[0] == CUECLUSTERPOS:
                                x[1] = uint8(mapa[int.from_bytes(x[1], 'big')])
        if e[0] == SEEKHEAD:
            for sk in e[1]:
                sid = next(int.from_bytes(v, 'big') for k, v in sk[1] if k == SEEKID)
                for x in sk[1]:
                    if x[0] == SEEKPOS and sid in lvl1:
                        x[1] = uint8(lvl1[sid])
    out = escrever([e for e in top if e[0] != SEGMENT]) + escrever([[SEGMENT, seg]])
    open(dst, 'wb').write(out)
    print('mkv_lacear: track %d (%s) laced, %d block(s) of up to %d frames, DefaultDuration removed'
          % (alvo, codec.decode(), laceados, por))


if __name__ == '__main__':
    main()
