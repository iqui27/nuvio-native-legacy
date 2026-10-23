#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Capturas da Biblioteca renovada, em BMP, sem interacao e sem rede.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/biblioteca_shot.sh /tmp/nuvio-biblioteca
#
# NUVIO_DADOS e uma pasta temporaria e o programa recusa rodar se dados_dir()
# nao for ela: fixar uma lista GRAVA.
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-${TMPDIR:-/tmp}/nuvio-biblioteca}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibshot-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
mkdir -p "$(dirname "$saida")"
compiler=(cc)
if command -v ccache >/dev/null; then
  export CCACHE_DIR="${CCACHE_DIR:-${TMPDIR:-/tmp}/nuvio-biblioteca-ccache}"
  compiler=(ccache cc)
fi
objects=()
for source in "${sources[@]}" tests/biblioteca_shot.c; do
  object="$tmp/$(basename "$source" .c).o"
  "${compiler[@]}" -c "$source" -Isrc -o "$object" \
    -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined
  objects+=("$object")
done
cc "${objects[@]}" -o "$tmp/shot" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
if [ "${NV_MODO:-}" = mock ] || [ "${NV_MODO:-}" = servers ]; then
  export NUVIO_RAIL=moderna NUVIO_SHOT_RELOGIO=1 NUVIO_SHOT_PT=1
fi
NUVIO_DADOS="$tmp/dados" "$tmp/shot" "$saida" "${NV_MODO:-}"
if [ "${NV_MODO:-}" = mock ] || [ "${NV_MODO:-}" = servers ]; then exit 0; fi
# SEGUNDA VOLTA so para a MEDIDA: comeca em lista, com o cache de textura frio.
# Ver a nota em tests/biblioteca_shot.c sobre por que uma volta so nao mede.
echo "--- segunda volta: lista com o cache frio ---"
NUVIO_DADOS="$tmp/dados2" "$tmp/shot" "$tmp/descarte" lista | grep "\[arte\]"
