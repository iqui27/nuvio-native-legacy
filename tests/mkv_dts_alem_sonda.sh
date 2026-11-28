#!/usr/bin/env bash
# F92: a leitura complementar de DTS (mkv_dts_alem, src/mkv.c) nao pode aplicar
# os bytes do cabecalho antigo sobre a sonda que um cabecalho novo publicou
# durante o I/O (ver tests/mkv_sonda_geracao.c, cenario long-read).
#   SANITIZE=thread bash tests/mkv_dts_alem_sonda.sh   (padrao)
#   SANITIZE=1 bash tests/mkv_dts_alem_sonda.sh        (ASan+UBSan)
#   SANITIZE=0 bash tests/mkv_dts_alem_sonda.sh        (sem sanitizador)
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkv-dts-alem.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
sed -n '/^static void \*fioMkv(void \*arg) {$/,/^}$/p' src/video_tpk.c src/tpk45.c src/dts/doador_tpk.c tests/doador_tpk_stub.c src/audioext.c > "$DIR/worker.c"
grep -q 'mkv_faixas' "$DIR/worker.c"
for name in old active; do
  ffmpeg -hide_banner -loglevel error -f lavfi -i 'testsrc2=size=64x64:rate=25:duration=0.8' \
    -f lavfi -i 'sine=frequency=500:duration=0.8:sample_rate=48000' \
    -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -b:a 768k \
    -metadata:s:a:0 'title=DTS-HD Master Audio' "$DIR/$name.mkv"
done
flags=(-Isrc -I"$DIR" -O1 -g -Wall -pthread)
case "${SANITIZE:-thread}" in
  thread) flags+=(-fsanitize=thread) ;;
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
${CC:-cc} "${flags[@]}" tests/mkv_sonda_geracao.c src/mkv.c -o "$DIR/test"
rc=0
"$DIR/test" long-read "$DIR/old.mkv" "$DIR/active.mkv" || rc=1
if [[ $rc -eq 0 ]]; then
  echo 'PASS: leitura complementar descartada quando uma sonda nova publica durante o I/O'
else
  echo 'FAIL: leitura complementar aplicou bytes do cabecalho antigo sobre a sonda nova'
fi
exit "$rc"
