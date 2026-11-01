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
lin_prot=$(grep -n 'protegerScreensaver();' src/video.c | cut -d: -f1)
lin_fio=$(grep -n 'pthread_create(&fio' src/video.c | head -1 | cut -d: -f1)
[ -n "$lin_prot" ] && [ "$lin_prot" -gt "$lin_fio" ] || {
  echo "protegerScreensaver fora do caminho de sucesso de iniciar()" >&2; exit 1; }

# protetorLigado so liga com o retorno do LSCall e zera em video_encerrar.
grep -q 'protetorLigado = lsChamar' src/video.c || {
  echo "protetorLigado nao depende do retorno do LSCall" >&2; exit 1; }
awk '/^void video_encerrar/,/^}/' src/video.c | grep -q 'protetorLigado = 0' || {
  echo "video_encerrar nao reseta protetorLigado" >&2; exit 1; }
echo "lsregistro.sh: ordem e reset do screensaver ok"
