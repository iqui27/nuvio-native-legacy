#!/bin/bash
# Xtream Codes (src/xtream.c): cadastro por perfil, lista de canais e URL de
# reproducao, com rede e disco dublados. Ver tests/xtream.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/xtream.c src/js.c tests/xtream.c \
  -Isrc -o /tmp/nuvio-xtream-tests -O1 -g -Wall -Wextra -lpthread \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-xtream-tests
# De novo com o ramo do Tizen ligado (#112): a lista http tem de sair pelo
# proxy do servico de recomendacoes, e o resto (https, video) direto.
cc ${flags[@]+"${flags[@]}"} ${NUVIO_CFLAGS:-} src/xtream.c src/js.c tests/xtream.c \
  -Isrc -o /tmp/nuvio-xtream-tests-tizen -O1 -g -Wall -Wextra -lpthread \
  -Wno-deprecated-declarations -Wno-macro-redefined \
  -DNV_XTREAM_PROXY_TESTE '-DNV_REC_URL="https://rec.teste"'
/tmp/nuvio-xtream-tests-tizen
