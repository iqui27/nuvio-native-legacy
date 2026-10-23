#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-addon-subtitles.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} tests/addons_legendas.c tests/addons_legendas_prazo.c src/linguas.c src/js.c src/legextras.c src/watchstate.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -pthread -ffunction-sections -fdata-sections -Wl,-dead_strip -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -Wno-missing-field-initializers -o "$work/test"
saida=$("$work/test")
if printf '%s\n' "$saida" | rg -q 'https?://|fixture\.invalid|PRIVATE_TOKEN|PRIVATE_CONFIG|api_key=|config=|INJECTED_DIAGNOSTIC|tt123|Movie|Show\.S01'; then
  echo "subtitle diagnostic leaked fixture URL, token, or content id" >&2
  exit 1
fi
printf '%s\n' "$saida" | rg -q '^\[legendas\] addon avisou:'
printf '%s\n' "$saida" | rg -q 'resource=subtitles tipo=movie http=404'
printf '%s\n' "$saida" | rg -q 'array=1 recebidas=0 candidatas=0'
printf '%s\n' "$saida" | rg -q 'array=0 recebidas=0 candidatas=0'
printf '%s\n' "$saida"
cc ${flags[@]+"${flags[@]}"} tests/addons_legendas_faixas.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/menu"
"$work/menu"
