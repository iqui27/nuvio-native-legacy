#!/bin/bash
# Fim de fileira (issue #65): o foco chega a ultima coluna e continua recebendo
# seta, com republicacao concorrente do catalogo. Ver tests/fimfileira.c.
#
# SEM SANITIZE POR PADRAO, e nao por preguica: com -fsanitize=address este
# binario trava ANTES do main(), em inicializador do dyld (medido com `sample`
# em 18/09: pilha em dyld4::runInitializers com um fio do ASan aberto). Nao e o
# app — o mesmo binario sem ASan roda e passa. SANITIZE=1 continua aceito para
# quem quiser investigar o dyld.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/artehero.c src/cwordem.c tests/fimfileira.c \
  -Isrc -o /tmp/nuvio-fimfileira-tests -O1 -g -ffunction-sections -fdata-sections \
  -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-fimfileira-tests "$@"
