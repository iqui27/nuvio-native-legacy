#!/bin/bash
# Scrobble do Simkl (issue #459) contra rede falsa; corpo compartilhado com
# Trakt, decisao propria. Sem SDL linkado nem rede. Regressao de logout usa
# o intervalo real de 1100 ms; shutdown mata o processo antes da resposta.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-scr-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
export SIMKL_TEST_DIR="$tmp"
compilar() {
  cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined \
    -DSMK_SCR_ESPACO_MS="$1" -DSMK_SCR_ESPERA_MS=150 -Dusleep=simkl_teste_usleep \
    -c src/simkl.c -o "$tmp/simkl.o"
  cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined \
    "$tmp/simkl.o" src/js.c src/traktscrobble.c src/metaep.c tests/simkl_scrobble.c \
    -o "$tmp/simkl_scrobble" -lpthread
}
if [ "${1:-}" = logout ]; then
  compilar 1100
  "$tmp/simkl_scrobble" logout
else
  compilar 0
  if [ "$#" -gt 0 ]; then "$tmp/simkl_scrobble" "$1"; else
    "$tmp/simkl_scrobble"
    "$tmp/simkl_scrobble" pause
    "$tmp/simkl_scrobble" shutdown
    "$tmp/simkl_scrobble" perfil
    compilar 1100
    "$tmp/simkl_scrobble" logout
  fi
fi
