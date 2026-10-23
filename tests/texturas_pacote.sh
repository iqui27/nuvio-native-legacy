#!/bin/bash
# Materiais CC0 de botao precisam viajar no pacote: tizen-art.sh estagia
# deploy/app/art/texturas/ e tanto tizen.sh quanto android.sh herdam.
#
#   bash tests/texturas_pacote.sh
set -eu
cd "$(dirname "$0")/.."
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
NUVIO_ARTE_ESTAGIO="$stage" bash tools/tizen-art.sh >/dev/null
src_count=$(find deploy/app/art/texturas -name 'material-*.jpg' | wc -l | tr -d ' ')
pkg_count=$(find "$stage/texturas" -name 'material-*.jpg' 2>/dev/null | wc -l | tr -d ' ')
[ "$src_count" -gt 0 ] || { echo "texturas pacote: sem materiais na fonte FAIL"; exit 1; }
[ "$pkg_count" = "$src_count" ] || { echo "texturas pacote: $pkg_count/$src_count estagiados FAIL"; exit 1; }
echo "texturas pacote: $pkg_count materiais estagiados PASS"
