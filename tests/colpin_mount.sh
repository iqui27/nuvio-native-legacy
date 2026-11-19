#!/bin/bash
# #700 root fixture: real montar() publishes one genre window per pinned
# collection folder. Offline; only transport/account are doubles.
#   bash tests/colpin_mount.sh        (SANITIZE=1 para ASan/UBSan)
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colpin-mount.XXXXXX")
trap 'rm -rf "$work"' EXIT
export TMPDIR="$work"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/discovery_boundary_stub.c ${flags[@]+"${flags[@]}"} -DNV_DESC_MIN_MS=0ull -DCAT_ESPERA_SILENCIO_MS=300 -DCAT_ESPERA_MIN_MS=600 \
  src/cwordem.c tests/colpin_mount.c src/homeestado.c src/catalogo.c src/homefil.c \
  src/progresso.c src/js.c src/metaep.c src/metaprov.c src/colecoes.c src/redeurl.c src/catordem.c src/cotacat.c \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$work/test" -O1 -g -pthread \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-unused-function
"$work/test"
