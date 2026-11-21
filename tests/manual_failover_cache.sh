#!/bin/bash
# F67: semeadura do failover manual com o filtro de cache "Em cache" —
# streams.c de verdade (incluido no TU do teste) + fontefalha/fontefiltro/
# linguas reais; stubs gerados pelos simbolos que restarem indefinidos.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/mnt/block-volume}/nuvio-manual-failover.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/data"
cc=${CC:-cc}
flags=(-std=c11 -D_GNU_SOURCE -O1 -g -Isrc -pthread -ffunction-sections
       -fdata-sections -Wno-unused-function)
sanitize=${NV_TEST_SANITIZE:-${SANITIZE:-0}}
if [ "$sanitize" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
elif [ "${TSAN:-0}" = 1 ]; then
  flags+=(-fsanitize=thread -fno-omit-frame-pointer)
fi
if [ "$(uname -s)" = Darwin ]; then
  linker=(-Wl,-dead_strip)
else
  linker=(-Wl,--gc-sections)
fi
"$cc" "${flags[@]}" -c tests/manual_failover_cache.c -o "$work/test.o"
"$cc" "${flags[@]}" -c src/fontefalha.c -o "$work/fontefalha.o"
"$cc" "${flags[@]}" -c src/fontefiltro.c -o "$work/fontefiltro.o"
"$cc" "${flags[@]}" -c src/linguas.c -o "$work/linguas.o"
"$cc" "${flags[@]}" "$work/test.o" "$work/fontefalha.o" "$work/fontefiltro.o" \
  "$work/linguas.o" -o /dev/null "${linker[@]}" -lm -lpthread 2>&1 \
  | grep -oP "undefined reference to \`\K[^']+" | sort -u > "$work/undef.txt" || true
{
  echo '#include <stddef.h>'
  echo 'typedef unsigned int Uint32;'
  while read -r s; do
    case "$s" in
      __*) continue;;
      SDL_GetTicks) echo 'Uint32 SDL_GetTicks(void){return 0;}';;
      *) echo "int $s(void) { return 0; }";;
    esac
  done < "$work/undef.txt"
} > "$work/stubs.c"
"$cc" "${flags[@]}" -c "$work/stubs.c" -o "$work/stubs.o"
"$cc" "${flags[@]}" "$work/test.o" "$work/fontefalha.o" "$work/fontefiltro.o" \
  "$work/linguas.o" "$work/stubs.o" -o "$work/test" "${linker[@]}" -lm -lpthread
"$work/test" "$work/data"
printf 'manual_failover_cache.sh: ok\n'
