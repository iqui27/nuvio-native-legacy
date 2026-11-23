#!/bin/bash
# FAROL38C — varredura e gravacoes fora do fio da UI. Ver tests/farol_varre.c:
# 700 entradas no mesmo `quando` nao podem passar do teto por passada no fio
# chamador, a varredura termina sem perder/repetir, e nenhuma escrita volta
# ao chamador (o --wrap=fopen conta fopen de escrita por fio). Reusa os
# falsos do farol_sinal.c (diario/catalogo/progresso), como a sonda da r2.
#   bash tests/farol_varre.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_varre.sh       (sem sanitizador)
#   SANITIZE=thread bash tests/farol_varre.sh  (TSan, com setarch -R)
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
bin="${TMPDIR:-/tmp}/nuvio-farol-varre-test"
wrap=-Wl,--wrap,fopen
if [ "$(uname -s)" = Darwin ]; then
  # ld64 nao aceita --wrap; o farol_varre.c interpoe fopen direto no
  # binario (simbolo fopen + RTLD_NEXT) quando __APPLE__.
  wrap=
fi
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -DNV_MAPA_PURO \
  -Isrc -Itests tests/farol_varre.c src/farolsinal.c src/farolgosto.c \
  src/farolficha.c src/farolanel.c src/mapa.c src/js.c -lpthread -lm \
  $wrap -o "$bin"
$prefix "$bin"
rm -f "$bin"
echo "farol_varre: tudo ok"
