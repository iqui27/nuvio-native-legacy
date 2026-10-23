#!/bin/bash
# #205: motivo do P2P desabilitado.
set -eu
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpunivel-ajuda.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc "${sources[@]}" tests/ajustes_p2p_motivo.c -Isrc -o "$work/teste" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" "$work/teste"
cc "${sources[@]}" tests/ajustes_p2p_motivo.c -Isrc -o "$work/teste_android" -DP2PM_ANDROID_VISIVEL \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$work" "$work/teste_android"
# #205: a linha precisa estar na TELA do Android, nao so fora de foraDestaPlataforma.
cc -E -P -DNV_ANDROID -DNV_P2P_MOTOR -x c src/ajustes_ux_tela.inc 2>/dev/null | grep -q "OPC(AJ_P2P_DESTINO)" \
  || { echo "FALHA: Cache do P2P fora da TELA no Android"; exit 1; }
if cc -E -P -DNV_ANDROID -x c src/ajustes_ux_tela.inc 2>/dev/null | grep -q 'OPC(AJ_P2P_DESTINO)'; then
  echo 'FAIL: local P2P cache setting present in Android lite'; exit 1
fi
echo "ajustes_p2p_motivo: tela android ok"
