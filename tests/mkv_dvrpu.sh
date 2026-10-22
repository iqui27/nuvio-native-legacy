#!/bin/bash
# 203-dvrpu: "fonte DV sem [dv] file:" (Duna, Debridio 4k DV|HDR10+, C9). A
# sonda do MKV precisa SEPARAR as tres causas no log:
#   (a) dvcC alem da janela de 320 KB (Tracks depois de anexos grandes);
#   (b) RPU de Dolby Vision so EM BANDA (NAL 62 nos quadros), sem dvcC;
#   (c) HDR10 puro (nem dvcC nem NAL 62).
# (b) e (c) sao MKV de verdade: x265 10 bits PQ/BT.2020 + ffmpeg -c copy; o (b)
# ganha um NAL 62 no fim de cada unidade de acesso, como um RPU de perfil 8.
# (a), o quadro cortado pela janela e o dvcC normal sao montados em tests/mkv_dvrpu.c.
#   bash tests/mkv_dvrpu.sh
set -eu
cd "$(dirname "$0")/.."
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || { echo "mkv_dvrpu.sh: ffmpeg nao encontrado em $FFMPEG"; exit 1; }
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dvrpu.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
"$FFMPEG" -v error -y -f lavfi -i "testsrc2=size=320x180:rate=24:duration=1" -pix_fmt yuv420p10le \
  -c:v libx265 -x265-params "log-level=error:aud=1:bframes=0:hdr10=1:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:master-display=G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,1):max-cll=1000,400" \
  -f hevc "$DIR/hdr10.hevc"
# RPU de mentira, cabecalho valido: prefixo 0x19, rpu_type 2, rpu_format 0,
# vdr_rpu_profile 1 (perfil 8), vdr_rpu_level 0. NAL header 7C 01 = tipo 62.
python3 - "$DIR/hdr10.hevc" "$DIR/rpu.hevc" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
rpu = b'\x00\x00\x01\x7c\x01\x19\x08\x00\x08\x00\xaa\xaa\xaa\xaa\x80'
aud = b'\x00\x00\x00\x01\x46\x01'
partes = d.split(aud)
assert partes[0] == b'' and len(partes) > 2
out = b''.join(aud + p + rpu for p in partes[1:])
open(sys.argv[2], 'wb').write(out)
PY
"$FFMPEG" -v error -y -fflags +genpts -f hevc -r 24 -i "$DIR/hdr10.hevc" -f lavfi -i "sine=duration=1" \
  -map 0:v -map 1:a -c:v copy -c:a ac3 -metadata:s:a:0 language=pol "$DIR/hdr10.mkv"
"$FFMPEG" -v error -y -fflags +genpts -f hevc -r 24 -i "$DIR/rpu.hevc" -f lavfi -i "sine=duration=1" \
  -map 0:v -map 1:a -c:v copy -c:a ac3 -metadata:s:a:0 language=pol "$DIR/rpu.mkv"
cc -Isrc tests/mkv_dvrpu.c src/mkv.c src/rede.c src/redeurl.c \
  -o "$DIR/t" -O1 -g -Wall -I/opt/homebrew/include -Wno-deprecated-declarations
"$DIR/t" "$DIR/rpu.mkv" "$DIR/hdr10.mkv"
