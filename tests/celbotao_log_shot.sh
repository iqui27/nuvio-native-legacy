#!/bin/bash
# Diagnostics "Download log" card, real render with NO visible window (see
# tests/celbotao_log_shot.c): local QR, same QR after OK, cloud QR, each
# compared module by module with the encoder, plus BMP captures. Out of the
# suite (*_shot.sh): needs SDL2 + GL (Mac host). No network service: the
# cloud upload hits an in-process fixture compiled into logcloud.o only.
#
#   TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp/x bash tests/celbotao_log_shot.sh [prefix]
set -euo pipefail
cd "$(dirname "$0")/.."
: "${TMPDIR:?Set TMPDIR to a bounded scratch directory on the external SSD}"
TMPDIR="${TMPDIR%/}"
saida="${1:-$TMPDIR/nuvio-celb-log}"
t=$(mktemp -d "$TMPDIR/celb-log-shot.XXXXXX"); trap 'rm -rf "$t"' EXIT
mkdir "$t/dados" "$t/snap"
inc=(-Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
cc -c src/logcloud.c "${inc[@]}" -o "$t/logcloud.o" -DNV_REC_URL='"https://fixture.invalid"' \
  -DNV_LOGLINK_TOKEN='"synthetic-upload-only"' -Drede_pedir=shot_rede_pedir -Drede_resposta_limpar=shot_rede_limpar
sources=()
for s in src/*.c src/dts/*.c; do
  case "$s" in src/main.c|src/celbotao.c|src/logcloud.c) continue;; esac   # celbotao.c: included by the test
  sources+=("$s")
done
cc "${sources[@]}" "$t/logcloud.o" tests/celbotao_log_shot.c "${inc[@]}" -o "$t/shot" -O1 -g \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations -Wno-macro-redefined
rc=0
NUVIO_DADOS="$t/dados" NV_SHOT_ESPERADO="$t/esperado" TMPDIR="$t/snap" "$t/shot" "$saida" > "$t/out" 2>&1 || rc=$?
if grep -qE 'fixture\.invalid|synthetic-upload-only|SHOT_SECRET|0123456789abcdef' "$t/out"; then
  echo 'FAIL: sensitive value in output'; exit 1
fi
cat "$t/out"
[ "$rc" = 0 ] || exit "$rc"
rmdir "$t/snap"   # every snapshot was deleted
# Optional real decode (no install): compare without printing the URLs.
if command -v zbarimg >/dev/null; then
  for n in local:1 local-ok:1 cloud:2; do
    if [ "$(zbarimg -q --raw "$saida-${n%%:*}.bmp")" = "$(sed -n "${n##*:}p" "$t/esperado")" ]; then
      echo "ok: zbarimg decodes ${n%%:*} to the expected URL"
    else echo "FAIL: zbarimg ${n%%:*}"; exit 1; fi
  done
else
  echo 'pending: zbarimg not installed; module comparison above is the decode-free check'
fi
