#!/bin/bash
# Sem fio (pthread_create falhou), o DELETE do Trakt e a RPC do syncprog de
# "Marcar como visto" / "Tirar de Continuar assistindo" NAO rodam no fio de
# desenho. Ver tests/ctxmenu_semfio.c.
#
#   bash tests/ctxmenu_semfio.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ctxsemfio-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
falhou=0
# Os dois pontos do menu (marcar visto e tirar de continuar): nenhum cai no
# fioTirarRemoto sincrono quando o fio nao sai.
if grep -nE 'else[[:space:]]+fioTirarRemoto\(' src/ctxmenu.c; then
  echo "  FALHOU: ctxmenu.c ainda roda fioTirarRemoto no quadro quando pthread_create falha"; falhou=1
fi
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ctxmenu.c|src/tirarremoto.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/ctxmenu_semfio.c -Isrc -o "$tmp/teste" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -w
NUVIO_DADOS="$tmp/dados" "$tmp/teste" || falhou=1
if [ "$falhou" != 0 ]; then echo "ctxmenu_semfio: FALHOU"; exit 1; fi
echo "ctxmenu_semfio.sh: ok"
