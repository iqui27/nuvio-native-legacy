#!/bin/bash
# Politica do LSRegister (src/lsregistro.c). Sem webOS: o video.c chama a lib
# por dlopen, e o que decide quando tentar e o que o codigo quer dizer mora
# num modulo puro justamente para rodar aqui.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/lsregistro.c tests/lsregistro.c -o /tmp/nuvio-lsregistro
/tmp/nuvio-lsregistro

# #706: o registro do screensaver tem de acontecer no CAMINHO DE SUCESSO de
# iniciar() — depois do pthread_create que sobe o laco LS2 — e nao no ramo de
# desistencia, onde `bus` ainda e NULL. Falha antes do fix: a chamada morava
# ~100 linhas antes.
lin_prot=$(grep -n 'protegerScreensaver();' src/video.c | head -1 | cut -d: -f1)
lin_fio=$(grep -n 'pthread_create(&fio' src/video.c | head -1 | cut -d: -f1)
[ -n "$lin_prot" ] && [ "$lin_prot" -gt "$lin_fio" ] || {
  echo "protegerScreensaver fora do caminho de sucesso de iniciar()" >&2; exit 1; }

# protetorLigado so liga com a resposta do hub e zera em video_encerrar.
grep -q 'protetorLigado = 1' src/video.c || {
  echo "protetorLigado nao e confirmado na resposta do hub" >&2; exit 1; }
awk '/^void video_encerrar/,/^}/' src/video.c | grep -q 'protetorLigado = 0' || {
  echo "video_encerrar nao reseta protetorLigado" >&2; exit 1; }
awk '/^void video_encerrar/,/^}/' src/video.c | grep -q 'protetorPendente = 0' || {
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
