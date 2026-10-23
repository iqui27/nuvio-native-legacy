#!/bin/bash
# #497: catalogues must not wait for CW/social; stale profile replies cannot land.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cw-prioridade.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -Itests -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -ffunction-sections -fdata-sections -DNV_DESC_MIN_MS=0ull)
gc=(-Wl,--gc-sections)
if [ "$(uname -s)" = Darwin ]; then gc=(-Wl,-dead_strip); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  if [ "$(uname -s)" != Darwin ]; then flags+=(-fno-pie); gc+=(-no-pie); fi
fi
"${CC:-cc}" "${flags[@]}" tests/cw_prioridade.c src/perfis.c src/homeestado.c src/catalogo.c \
  src/progresso.c src/cwordem.c src/cotacat.c src/js.c src/metaprov.c src/colecoes.c \
  src/redeurl.c src/catordem.c "${gc[@]}" -o "$work/test"
for scenario in fresh cached clear stale; do
  mkdir "$work/data-$scenario"
  NV_T_DIR="$work/data-$scenario" \
    ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:halt_on_error=1}" \
    UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
    "$work/test" "$scenario"
done
echo 'cw_prioridade: four scenarios passed'
