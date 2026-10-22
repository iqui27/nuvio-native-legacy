#!/bin/bash
# Audio no .tpk da Samsung: a TV esconde a faixa que nao decodifica (DTS, TrueHD)
# e lista MENOS que o cabecalho do MKV. O casamento por ordinal desistia e o
# rotulo do arquivo se perdia ("casamento audio=none"). Sem ffmpeg: o arquivo e
# montado a mao.
#   bash tests/faixasmkv_casar.sh
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-faixasmkv-casar.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -Isrc tests/faixasmkv_casar.c src/faixasmkv.c src/mkv.c src/linguas.c src/rede.c src/redeurl.c src/audioinfo.c \
  -o "$DIR/t" -O1 -g -Wall -I/opt/homebrew/include -Wno-deprecated-declarations
"$DIR/t"
