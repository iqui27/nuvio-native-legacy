#!/bin/bash
# feat/mkv-chapter-segments: Simkl e Anime-Skip sem rede (ver tests/segfontes.c).
#   bash tests/segfontes.sh
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-segfontes.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
# rede_pedir nunca roda (os dubles trocam o buscador e o postador); o stub so
# satisfaz o link.
cat > "$DIR/rede_stub.c" <<'EOF'
#include "rede.h"
int rede_pedir(const RedePedido *q, RedeResposta *r) { (void)q; (void)r; return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
EOF
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -Wno-deprecated-declarations \
  tests/segfontes.c src/segfontes.c src/js.c "$DIR/rede_stub.c" -lpthread -o "$DIR/test"
"$DIR/test"
