#!/bin/bash
# CATREV — guarda de cat_revisao() nos indices de cat_item copiados pelos
# climas (mapa.c climaLocal/climaPub, explorar.c climas) e nas leituras de
# farolsinal. Ver tests/catrev.c: monta os climas e a tela sobre um catalogo
# real, troca o catalogo inteiro (cat_definir_tudo sobe a revisao) e exige
# que nenhum cartaz/pedido de pagina aponte para a obra errada — nem para um
# homonimo na posicao velha. Compila mapa.c, explorar.c, catalogo.c, js.c e
# farolgosto.c de producao; o resto e duble (sem rede, sem GL, sem disco).
#
#   bash tests/catrev.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/catrev.sh       (sem sanitizador)
#   SANITIZE=thread bash tests/catrev.sh  (TSan, com setarch -R)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
prefix=""
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  thread) flags+=(-fsanitize=thread)
          [ "$(uname -s)" = Linux ] && prefix="setarch -R" ;;
  *) flags+=(-fsanitize="$SANITIZE") ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-catrev-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
  -Isrc -I/usr/include/SDL2 \
  tests/catrev.c src/mapa.c src/explorar.c src/catalogo.c src/js.c \
  src/farolgosto.c -lpthread -lm -o "$bin"
$prefix "$bin"
rm -f "$bin"
echo "catrev: tudo ok"
