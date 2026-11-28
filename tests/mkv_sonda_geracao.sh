#!/usr/bin/env bash
# F92: um worker TPK de geracao anterior nao pode sobrescrever a classificacao
# DTS da sonda ativa (ver tests/mkv_sonda_geracao.c). O corpo real de fioMkv e
# extraido de src/video_tpk.c src/tpk45.c src/dts/doador_tpk.c tests/doador_tpk_stub.c src/audioext.c — no pai ele chama mkv_faixas e rebaixa a sonda
# ativa; na ponta ele chama mkv_faixas_geracao e o resultado e descartado sob
# dtsLock.
#   SANITIZE=thread bash tests/mkv_sonda_geracao.sh   (padrao)
#   SANITIZE=1 bash tests/mkv_sonda_geracao.sh        (ASan+UBSan)
#   SANITIZE=0 bash tests/mkv_sonda_geracao.sh        (sem sanitizador)
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkv-sonda-ger.XXXXXX")
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
"$DIR/test" before-parser "$DIR/old.mkv" "$DIR/active.mkv" || rc=1
"$DIR/test" inside-parser "$DIR/old.mkv" "$DIR/active.mkv" || rc=1
if [[ $rc -eq 0 ]]; then
  echo 'PASS: worker TPK de geracao anterior nao sobrescreve a classificacao DTS da sonda ativa'
else
  echo 'FAIL: worker TPK de geracao anterior rebaixou a classificacao DTS da sonda ativa'
fi
exit "$rc"
