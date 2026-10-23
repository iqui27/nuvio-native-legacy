#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-texto.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
# MKV minimo: Tracks, Cluster com dois BlockGroup e Cues (sem video real).
python3 - "$DIR" <<'PY'
import sys, pathlib
p=pathlib.Path(sys.argv[1])
def el(i,b):
    return i.to_bytes((i.bit_length()+7)//8,'big') + (0x10000000|len(b)).to_bytes(4,'big') + b
def u(i,n): return el(i,n.to_bytes(4,'big'))
for codec in ('UTF8','WEBVTT'):
    tr=el(0x1654ae6b,el(0xae,u(0xd7,6)+u(0x83,17)+el(0x86,('S_TEXT/'+codec).encode())+el(0x22b59c,b'ara')))
    body=u(0xe7,1000); rel=[]
    for t,d,text in [(250,1750,'<i>مرحبا بالعالم</i>\n<b>Olá &amp; TV</b>'),(4000,1250,'<b>نص ثان</b>')]:
        rel.append(len(body))
        body+=el(0xa0,el(0xa1,b'\x86'+t.to_bytes(2,'big',signed=True)+b'\x00'+text.encode())+u(0x9b,d))
    cluster=el(0x1f43b675,body)
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,0)))
    sh=el(0x114d9b74,el(0x4dbb,el(0x53ab,bytes.fromhex('1c53bb6b'))+u(0x53ac,len(sh)+len(tr)+len(cluster))))
    cues=el(0x1c53bb6b,b''.join(el(0xbb,u(0xb3,t)+el(0xb7,u(0xf7,6)+u(0xf1,len(sh)+len(tr))+u(0xf0,r))) for t,r in zip((1250,5000),rel)))
    p.joinpath(codec+'.mkv').write_bytes(el(0x1a45dfa3,el(0x4282,b'matroska'))+el(0x18538067,sh+tr+cluster+cues+el(0xec,b'\0'*1024)))
PY
cc -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 tests/mkvass_texto.c "${MKVASS_SRC:-src/mkvass.c}" src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -o "$DIR/test"
mkdir "$DIR/dados"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/UTF8.mkv" "$DIR/WEBVTT.mkv"
for alvo in LG NV_TPK NV_ANDROID; do
  cc -D"$alvo" tests/faixas_texto.c src/faixasmkv.c -Isrc -O1 \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-macro-redefined -o "$DIR/escolha"
  "$DIR/escolha"
done
