#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibfiltro.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=()
if [ -n "${BIB_REV:-}" ]; then
  git show "$BIB_REV:src/biblioteca.c" > "$tmp/biblioteca.c"
  flags+=("-DBIB_FONTE=\"$tmp/biblioteca.c\"")
fi
for plat in tv android; do
  def=(); [ "$plat" = android ] && def=(-DNV_ANDROID)
  cc ${flags[@]+"${flags[@]}"} ${def[@]+"${def[@]}"} tests/biblioteca_filtro.c src/salvos.c src/focus.c \
    -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -o "$tmp/teste"
  "$tmp/teste" | tee "$tmp/log"
  if [ "${BIB_EXIGIR_LOG:-1}" = 1 ]; then
    if ! grep -q 'modo=0 tipo=0 ocultar=1 .*total=129 .*excl_meta=0 grade=129' "$tmp/log" ||
       ! grep -q 'modo=0 tipo=0 ocultar=1 .*total=129 .*excl_meta=2 grade=127' "$tmp/log"; then
      echo "FAIL: diagnostico deve manter desconhecidos e excluir apenas futuros ($plat)"
      exit 1
    fi
    echo "biblioteca diagnostico ($plat): PASS"
  fi
done
