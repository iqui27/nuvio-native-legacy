#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Motor P2P embutido (src/p2pmotor.c) contra um motor FALSO: pedido, cancelar,
# eventos velhos, statvfs falho, ENOSPC, tetos de disco/RAM, vigia, ordem da
# limpeza e corrida pedir/cancelar/parar. Nao precisa de rede nem do motor.
# PASSAR AQUI NAO PROVA O nuvio-engine: isso e tests/p2pmotor_real.sh.
#
#   bash tests/p2pmotor.sh
#   SANITIZE=1 bash tests/p2pmotor.sh        # ASan + UBSan
#   SANITIZE=thread bash tests/p2pmotor.sh   # TSan
set -eu
cd "$(dirname "$0")/.."
OUT="${NV_TEST_OUT:-${TMPDIR:-/tmp}}/nuvio-p2pmotor-tests"
flags=(-O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined -DP2PM_DURO_MAX_MB=1
       -DP2PM_JANELA_MIN_MB=0)   # teto de 1 MB com arquivos de 2 GB: sem isto a janela recusaria tudo
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/p2pmotor.c src/p2pmotor_motor.c src/p2p.c src/js.c \
  tests/p2pmotor.c -o "$OUT" -lpthread
"$OUT"
# #334: a conta do teto e da janela com os valores COMPILADOS de verdade (sem os -D acima).
cc -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined src/p2pmotor.c src/p2pmotor_motor.c \
  src/p2p.c src/js.c tests/p2pmotor_teto.c -o "$OUT-teto" -lpthread
"$OUT-teto"
