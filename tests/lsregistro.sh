#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Politica do LSRegister (src/lsregistro.c). Sem webOS: o video.c chama a lib
# por dlopen, e o que decide quando tentar e o que o codigo quer dizer mora
# num modulo puro justamente para rodar aqui.
set -eu
cd "$(dirname "$0")/.."
binary=$(mktemp "${TMPDIR:-/tmp}/nuvio-lsregistro.XXXXXX")
trap 'rm -f "$binary"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Linux ]; then flags+=(-no-pie); fi
fi
cc -O1 -g -Wall -Wextra "${flags[@]}" -Isrc src/lsregistro.c tests/lsregistro.c -o "$binary"
"$binary"

# #706: subscribe only after the LS2 loop starts, then retry from the pump.
lin_fio=$(awk '/^static int iniciar\(int automatico\) \{$/,/^}/ {if (/pthread_create\(&fio/) {print NR; exit}}' src/video.c)
lin_ligado=$(awk '/^static int iniciar\(int automatico\) \{$/,/^}/ {if (/ligado = 1;/) {print NR; exit}}' src/video.c)
lin_prot=$(awk '/^static int iniciar\(int automatico\) \{$/,/^}/ {if (/protegerScreensaver\(\);/) {print NR; exit}}' src/video.c)
[ -n "$lin_prot" ] && [ "$lin_prot" -gt "$lin_fio" ] && [ "$lin_prot" -gt "$lin_ligado" ] || {
  echo "protegerScreensaver fora do caminho de sucesso de iniciar()" >&2; exit 1; }

grep -q '__atomic_store_n(&protetorLigado, 1' src/video.c || {
  echo "protetorLigado nao e confirmado na resposta do hub" >&2; exit 1; }
awk '/^void video_encerrar/,/^}/' src/video.c | grep -q '__atomic_store_n(&protetorLigado, 0' || {
  echo "video_encerrar nao reseta protetorLigado" >&2; exit 1; }
awk '/^void video_encerrar/,/^}/' src/video.c | grep -q '__atomic_store_n(&protetorPendente, 0' || {
  echo "video_encerrar nao reseta protetorPendente" >&2; exit 1; }
grep -q 'protetorPendente' src/video.c || {
  echo "falta estado pendente do screensaver" >&2; exit 1; }
awk '/^void video_bombear/,/^}/' src/video.c | grep -q protegerScreensaver || {
  echo "video_bombear nao retenta screensaver" >&2; exit 1; }
grep -q 'trailer_abrir_preview_home' src/home.c || {
  echo "home nao marca preview automatico explicitamente" >&2; exit 1; }
grep -q 'previewHome' src/trailer.c src/lsregistro.c || {
  echo "politica preview Home ausente" >&2; exit 1; }
echo "lsregistro.sh: ordem, reset e retry do screensaver ok"
