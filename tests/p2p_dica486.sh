#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-p2p486.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# O teste inclui src/streams.c para observar a ordem realmente montada.
srcs=(tests/p2p_dica486.c tests/p2p_dica486_fila.c src/fontesmortas.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/badges.c src/streamfit.c src/naovideo.c)
cc "${flags[@]}" "${srcs[@]}" -pthread -o "$dir/test"
"$dir/test"
# De novo, como o .wgt: no navegador o P2P fica oculto nos Ajustes e a dica
# "ligue o P2P" seria impossivel de seguir (__EMSCRIPTEN__ tambem vira o bloco
# equivalente dentro de src/streams.c, que o teste inclui).
cc "${flags[@]}" -D__EMSCRIPTEN__ "${srcs[@]}" -pthread -o "$dir/test_wgt"
"$dir/test_wgt"
