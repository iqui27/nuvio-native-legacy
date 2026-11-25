#!/usr/bin/env bash
# Duas sondas MKV simultaneas (ver tests/mkv_sonda_concorrente.c): a escrita da
# classificacao DTS nao pode colidir entre sondas. Sob TSan o binario pode sair
# com rc 66 por corridas preexistentes do parser (a base tambem as tem); o que
# nao pode aparecer e conflito nos campos DTS.
#   SANITIZE=thread bash tests/mkv_sonda_concorrente.sh   (padrao)
#   SANITIZE=1 bash tests/mkv_sonda_concorrente.sh        (ASan+UBSan)
#   SANITIZE=0 bash tests/mkv_sonda_concorrente.sh        (sem sanitizador)
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkv-sonda-conc.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
ffmpeg -hide_banner -loglevel error \
  -f lavfi -i 'testsrc2=size=64x64:rate=25:duration=0.8' \
  -f lavfi -i 'sine=frequency=500:duration=0.8:sample_rate=48000' \
  -c:v libx264 -bf 0 -g 25 -c:a dca -strict -2 -b:a 768k "$DIR/sonda.mkv"
flags=(-Isrc -O1 -g -Wall -Wno-deprecated-declarations -pthread)
case "${SANITIZE:-thread}" in
  thread) flags+=(-fsanitize=thread) ;;
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
cc "${flags[@]}" tests/mkv_sonda_concorrente.c src/mkv.c -o "$DIR/test"
set +e
"$DIR/test" "$DIR/sonda.mkv" > "$DIR/run.log" 2>&1
rc=$?
set -e
if grep -Eq 'dtsListar|dtsTrecho|dtsPublicar|dtsPub|dtsLock|dtsLog|MkvDts|dtsN|dtsKind|dtsBytes|mkv_dts' "$DIR/run.log"; then
  echo 'FAIL: campos DTS da sonda colidem entre sondas simultaneas'
  cat "$DIR/run.log"
  exit 1
fi
if [[ $rc -ne 0 && $rc -ne 66 ]]; then
  cat "$DIR/run.log"
  exit "$rc"
fi
echo "PASS: duas sondas MKV simultaneas sem conflito nos campos DTS (rc=$rc)"
