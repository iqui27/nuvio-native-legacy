#!/bin/bash
# Bancada do decodificador nativo de GIF (#84), no Mac.
#
#   bash tests/gifbanco.sh              gera os GIFs e mede
#   bash tests/gifbanco.sh --so-gerar   so gera (tests/gifbanco-tizen.sh usa)
#
# OS GIFS SAO GERADOS, com o ffmpeg, nos tamanhos dos registros do #84 e do
# orcamento da 1.4.6: 35 quadros 512x512 (avatar de 1050 ms), 75 quadros
# 500x375 e 198 quadros 250x250 (o do rawldon; 198 x 250 x 250 x 4 cabe nos
# 48 MB de 2 GB). O conteudo e FOTO em movimento (tests/amostra.jpg com zoom e
# pan): quadro inteiro mudando e 256 cores e o pior caso do LZW — um avatar
# de desenho animado custa menos. O encoder do ffmpeg recorta cada quadro ao
# retangulo que mudou e usa transparencia, como os GIFs de verdade.
#
# Cada GIF tambem e decodificado pelo ffmpeg e comparado pixel a pixel.
set -euo pipefail
cd "$(dirname "$0")/.."
command -v ffmpeg >/dev/null || { echo "gifbanco.sh: precisa do ffmpeg" >&2; exit 127; }
OUT="${NV_GIFBANCO_DIR:-${TMPDIR:-/tmp}/nuvio-gifbanco}"
mkdir -p "$OUT"
J="$PWD/tests/amostra.jpg"
gerar() {  # nome quadros tamanho fps filtro-de-movimento
  [ -s "$OUT/$1.gif" ] && return 0
  ffmpeg -v error -y -loop 1 -i "$J" \
    -vf "scale=1600:-1,zoompan=$5:d=1:s=$3:fps=$4,trim=end_frame=$2,split[a][b];[a]palettegen[p];[b][p]paletteuse" \
    -frames:v "$2" "$OUT/$1.gif"
}
gerar a35  35  512x512 100/3 "z='1.1+0.01*on':x='iw/2-(iw/zoom/2)':y='ih/2-(ih/zoom/2)'"
gerar b75  75  500x375 25    "z='1.2':x='on*6':y='ih/2-(ih/zoom/2)'"
gerar c198 198 250x250 20    "z='1.5+0.002*on':x='iw/2-(iw/zoom/2)+40*sin(on/10)':y='ih/2-(ih/zoom/2)'"
[ "${1:-}" = --so-gerar ] && { echo "$OUT"; exit 0; }

cc -O2 src/gif.c tests/gifbanco.c -Isrc -o "$OUT/gifbanco" -Wno-deprecated-declarations -Wno-macro-redefined
for g in a35:200 b75:480 c198:200; do
  nome=${g%%:*}; larg=${g##*:}
  ffmpeg -v error -y -i "$OUT/$nome.gif" -f rawvideo -pix_fmt rgba "$OUT/$nome.rgba"
  "$OUT/gifbanco" "$OUT/$nome.gif" "$larg" "$OUT/$nome.rgba"
done
