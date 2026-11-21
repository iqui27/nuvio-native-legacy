#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
run=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detail-ausente.XXXXXX")
trap 'rm -rf "$run"' EXIT
export NUVIO_DADOS="$run/dados"
mkdir -p "$NUVIO_DADOS"
flags=()
case "${SANITIZE:-}" in
  address) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
fontes() {
  local s
  for s in "$1"/*.c "$1"/dts/*.c; do
    case "$s" in */main.c|*/detail.c) continue;; esac
    printf '%s\n' "$s"
  done
}
compilar() {
  local srcs=() s
  while IFS= read -r s; do srcs+=("$s"); done < <(fontes "$1")
  cc "${srcs[@]}" "$2" -I"$1" -o "$3" \
    -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined "${flags[@]}" ${4:-}
}

# Caso 1: o detalhe sobrevive ao catalogo inteiro ausente por ciclos.
compilar src tests/detail_catalogo_ausente.c "$run/test"
"$run/test"

# Caso 2 (sonda da temporada, cenario da revisao Sol em DETAIL1431): a recarga
# publica n = 0 entre a contagem dos episodios e a segunda consulta de
# cat_item() em resumoTemporada, e conclui a republicacao do episodio antes do
# acesso ao item. A instrumentacao (duas barreiras) e aplicada numa copia de
# src/, nunca nos fontes do repositorio.
sonda="$run/sonda"
mkdir -p "$sonda/tests"
cp -aL src "$sonda/src"
cp tests/detail_catalogo_ausente.c "$sonda/tests/"
python3 - "$sonda/src" <<'PY'
from pathlib import Path
import sys

src = Path(sys.argv[1])
file = src / 'detail.c'
source = file.read_bytes()
lookup = b'      const CatItem *ciC = cat_item(idx);'
assert source.count(lookup) == 1
file.write_bytes(source.replace(
    lookup,
    b'      esperar_catalogo_vazio();\n' + lookup +
    b'\n      concluir_recarga(ciC);'))
file = src / 'catalogo.c'
source = file.read_bytes()
function = source.index(b'void cat_definir_tudo(const CatItem *lista')
store = b'    __atomic_store_n(&n, 0, __ATOMIC_RELEASE);'
start = source.index(store, function) + len(store)
file.write_bytes(source[:start] +
    b'\n    extern void sonda_temporada_n_zero(void);\n'
    b'    sonda_temporada_n_zero();' + source[start:])
PY
compilar "$sonda/src" "$sonda/tests/detail_catalogo_ausente.c" "$run/test-sonda" "-DNV_SONDA_TEMPORADA"
"$run/test-sonda"
