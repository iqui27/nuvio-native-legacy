#!/bin/bash
# Bloqueador 2.0.3 (TCL, auto-play abriu plugin com "Somente add-ons"): o
# carimbo da lista e o episodio pedido aos addons, e o mesmo alvo pedido de
# novo com a busca no ar devolve o que ja chegou. addons.c real; ver
# tests/autoplay_alvo.c.
#
#   bash tests/autoplay_alvo.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-autoplay-alvo.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wextra -ffunction-sections -fdata-sections -Wl,-dead_strip)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/autoplay_alvo.c src/addons.c src/js.c -o "$dir/teste"
"$dir/teste"
