#!/bin/bash
# #206: no .tpk da Samsung o player so da o idioma de cada faixa. O rotulo
# ("Forced", "5.1", nome da faixa) sai do cabecalho do MKV. Monta um MKV de
# verdade com ffmpeg (2 audios, 4 legendas: uma com nome "Forced", uma so com
# a FlagForced) e confere o que a folha de faixas mostraria.
#   bash tests/faixasmkv.sh
set -eu
cd "$(dirname "$0")/.."
FFMPEG=${FFMPEG:-/opt/homebrew/bin/ffmpeg}
[ -x "$FFMPEG" ] || { echo "faixasmkv.sh: ffmpeg nao encontrado em $FFMPEG"; exit 1; }
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-faixasmkv.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
printf '1\n00:00:01,000 --> 00:00:02,000\nx\n' > "$DIR/s.srt"
"$FFMPEG" -v error -y \
  -f lavfi -i "testsrc2=size=160x90:rate=24:duration=3" \
  -f lavfi -i "sine=frequency=440:duration=3" \
  -f lavfi -i "sine=frequency=880:duration=3" \
  -i "$DIR/s.srt" -i "$DIR/s.srt" -i "$DIR/s.srt" -i "$DIR/s.srt" \
  -map 0:v -map 1:a -map 2:a -map 3:s -map 4:s -map 5:s -map 6:s \
  -c:v libx264 -preset ultrafast -c:a aac -ac:a:0 6 -ac:a:1 2 -c:s srt \
  -metadata:s:a:0 language=hun -metadata:s:a:0 title="DD 5.1" \
  -metadata:s:a:1 language=eng \
  -metadata:s:s:0 language=hun \
  -metadata:s:s:1 language=hun -metadata:s:s:1 title="Forced" \
  -metadata:s:s:2 language=eng -disposition:s:2 forced \
  -metadata:s:s:3 language=eng -metadata:s:s:3 title="English SDH" -disposition:s:3 hearing_impaired \
  "$DIR/f.mkv"
cc -Isrc tests/faixasmkv.c src/faixasmkv.c src/mkv.c src/linguas.c src/rede.c src/redeurl.c src/audioinfo.c \
  -o "$DIR/t" -O1 -g -Wall -I/opt/homebrew/include -Wno-deprecated-declarations
"$DIR/t" "$DIR/f.mkv"
