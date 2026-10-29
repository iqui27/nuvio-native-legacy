#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Cache por fonte do guia: fonte removida nao volta do disco; a que so falhou
# a rede fica. Ver tests/guiacache.c.
#
#   bash tests/guiacache.sh
#   SANITIZE=1 bash tests/guiacache.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-guiacache.XXXXXX)
trap 'rm -rf "$work"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${GUIACACHE_BASELINE:-0}" = 1 ]; then
  git show ba0639cf0:src/guia.c > "$work/guia.c"
  flags+=(-DGUIACACHE_BASELINE "-DGUIACACHE_SOURCE=\"$work/guia.c\"")
fi
backup_extra=()
if [ -f src/backupcategories.c ]; then backup_extra+=(src/backupcategories.c); fi
cc ${backup_extra[@]+"${backup_extra[@]}"} ${flags[@]+"${flags[@]}"} tests/guiacache.c \
  src/ltv.c src/js.c src/idcanal.c src/dados.c src/marco.c src/guiaaddons.c src/guiafiltro.c \
  src/epg.c src/xtepg.c src/lembrete.c src/guialembrete.c \
  src/fontecache.c src/cachearte.c src/cachedisco.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -Wno-macro-redefined -lpthread -lm -o "$work/test"
NUVIO_DADOS="$work/dados" "$work/test" "$work/dados"
if [ "${GUIACACHE_BASELINE:-0}" != 1 ]; then
  GUIACACHE_CONTRAST_ONLY=1 "$work/test" "$work/dados"
  # The historical baseline must compile, then fail on retained removed channels.
  rc=0
  GUIACACHE_BASELINE=1 bash tests/guiacache.sh > "$work/baseline.log" 2>&1 || rc=$?
  if [ "$rc" != 134 ] || ! rg -q 'nCanais == 1 && nLtvF == 1'  "$work/baseline.log"; then
    printf 'FAIL: baseline must reach the removed-source assertion (rc=%s)\n' "$rc"
    printf '%s\n' "$(< "$work/baseline.log")"
    exit 1
  fi
  printf 'PASS: baseline compiled and reached the removed-source assertion (rc=134)\n'
fi
