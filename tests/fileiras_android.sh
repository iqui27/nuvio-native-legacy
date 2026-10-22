#!/bin/bash
# Fileiras ilimitadas no Android (2.0.4): o mesmo teste nos dois lados, com e
# sem -DNV_ANDROID. Sem SDL de verdade, sem rede, sem disco; descoberta.c entra
# so pelas duas funcoes de ritmo (-dead_strip, receita de manifesto_cache.sh).
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -ffunction-sections -fdata-sections -Wl,-dead_strip \
       -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
       -Wall -Wno-unused-function -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
tmp="${TMPDIR:-/tmp}"
for lado in tv android; do
  extra=()
  [ "$lado" = android ] && extra=(-DNV_ANDROID)
  cc "${flags[@]}" ${extra[@]+"${extra[@]}"} -DFIL_TESTE \
    src/fileiras.c src/descoberta.c src/cwordem.c tests/fileiras_android.c \
    -o "$tmp/nuvio-fileiras-android-$lado"
  echo "== $lado"
  "$tmp/nuvio-fileiras-android-$lado"
done
