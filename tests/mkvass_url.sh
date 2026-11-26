#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-mkvass-url.XXXXXX)
awk '
 /^static char  urlAtual\[/ || /^const char \*video_url_atual\(void\)/ ||
 /^static VideoSimulacao SIM;/ ||
 /^int  video_tocar\(const char \*u\) \{ snprintf\(urlAtual/ {
     printf "#line %d \"src/video.c\"\n%s\n", NR, $0
 }
 ' src/video.c > tests/video_url.inc
trap 'rm -f tests/video_url.inc; rm -rf "$DIR"' EXIT
cc -I/opt/homebrew/include -Itests tests/video_url.c -o "$DIR/video-url"
"$DIR/video-url"
${FFMPEG:-/opt/homebrew/bin/ffmpeg} -v error -y \
  -f lavfi -i 'testsrc2=size=320x180:rate=24:duration=15' \
  -i tests/fixtures/ass/simples.ass -map 0:v -map 1:s \
  -c:v libx264 -c:s ass "$DIR/test.mkv"
cc -Isrc -O1 -g tests/mkvass_url.c src/mkvass.c src/pgs_mkv_restore.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/assperf.c src/asspeso.c src/assfios.c src/dados.c -lz -lm \
  -o "$DIR/test"
mkdir "$DIR/dados"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/test.mkv"
