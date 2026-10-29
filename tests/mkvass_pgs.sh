#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-mkvass-pgs.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
# MKV minimo com uma faixa S_HDMV/PGS: um display set (objeto 6x2 em 1920x1080)
# e outro que so limpa a tela. Sem video real.
python3 - "$DIR" <<'PY'
import sys, pathlib
p=pathlib.Path(sys.argv[1])
def el(i,b):
    return i.to_bytes((i.bit_length()+7)//8,'big') + (0x10000000|len(b)).to_bytes(4,'big') + b
def u(i,n): return el(i,n.to_bytes(4,'big'))
def seg(t,d): return bytes([t])+len(d).to_bytes(2,'big')+d
RLE=bytes([0x01,0x00,0x83,0x02,0x00,0x02,0x00,0x00,0x00,0x40,0x03,0x00,0xC0,0x03,0x01,0x00,0x00])
def pcs(estado,n):
    d=(1920).to_bytes(2,'big')+(1080).to_bytes(2,'big')+b'\x10\x00\x01'+bytes([estado,0,0,n])
    if n: d+=b'\x00\x07\x00\x00'+(100).to_bytes(2,'big')+(900).to_bytes(2,'big')
    return seg(0x16,d)
mostra=(pcs(0x80,1)+seg(0x14,bytes([0,0,1,235,128,128,255,2,16,128,128,128]))
        +seg(0x15,b'\x00\x07\x00\xC0\x00'+(4+len(RLE)).to_bytes(2,'big')+b'\x00\x06\x00\x02'+RLE)+seg(0x80,b''))
limpa=pcs(0x80,0)+seg(0x80,b'')
tr=el(0x1654ae6b,el(0xae,u(0xd7,6)+u(0x83,17)+el(0x86,b'S_HDMV/PGS')+el(0x22b59c,b'eng')))
body=u(0xe7,1000); rel=[]
for t,d,dados in [(250,1750,mostra),(4000,None,limpa)]:
    rel.append(len(body))
    blk=el(0xa1,b'\x86'+t.to_bytes(2,'big',signed=True)+b'\x00'+dados)
    body+=el(0xa0,blk+(u(0x9b,d) if d else b''))
cluster=el(0x1f43b675,body)
sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,0)))
sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,len(sh)+len(tr)+len(cluster))))
cues=el(0x1c53bb6b,b''.join(el(0xbb,u(0xb3,t)+el(0xb7,u(0xf7,6)+u(0xf1,len(sh)+len(tr))+u(0xf0,r))) for t,r in zip((1250,5000),rel)))
p.joinpath('pgs.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,sh+tr+cluster+cues+el(0xec,b'\0'*1024)))
PY
cc -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 tests/mkvass_pgs.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -o "$DIR/test"
mkdir "$DIR/dados"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/pgs.mkv"
