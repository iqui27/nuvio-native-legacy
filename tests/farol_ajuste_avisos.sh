#!/bin/bash
# Teste dirigido da issue #27 do Farol: "Avisos do Farol" em Ajustes > Sobre e ajuda
# ligado a politica de silencio (farolilha + ilhasinais) e ao Resolver sob demanda.
#   bash tests/farol_ajuste_avisos.sh
#   SANITIZE=1 bash tests/farol_ajuste_avisos.sh
set -euo pipefail
cd "$(dirname "$0")/.."

dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-avisos.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir -p "$dir/data"

sdl=()
if command -v pkg-config >/dev/null && pkg-config --exists sdl2; then
  read -r -a sdl <<< "$(pkg-config --cflags sdl2)"
fi
if [ "$(uname -s)" = Darwin ]; then
  sdl+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  strip=(-Wl,-dead_strip)
  libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  strip=(-Wl,--gc-sections)
  libs=($(pkg-config --libs sdl2 SDL2_image SDL2_ttf gl 2>/dev/null) -lz -lm -lpthread)
fi

flags=(-O1 -g -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined -ffunction-sections -fdata-sections)
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done

"$ccbin" "${sources[@]}" tests/farol_ajuste_avisos.c -Isrc "${sdl[@]}" \
  -D_GNU_SOURCE -DAJUSTES_TESTE -DRESOLVER_TESTE "${flags[@]}" "${strip[@]}" \
  "${libs[@]}" -o "$dir/teste"
"$dir/teste" "$dir/data"
echo "farol_ajuste_avisos: tudo ok"
