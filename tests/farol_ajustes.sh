#!/bin/bash
# Diferenca de ajustes por lista branca (farol-26): ajustes.c de verdade.
#   bash tests/farol_ajustes.sh
#
# Dois passos: (1) o conferidor roda o ajlog_chave_sensivel real sobre cada
# linha de farolajustes.h e gera sensivel.h com "#error" por violacao —
# sensivel na lista branca e erro de COMPILACAO, como manda o PLANO §5;
# (2) o teste inclui sensivel.h + ajustes.c e prova os diffs em execucao.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-ajustes.XXXXXX")
trap 'rm -rf "$dir"' EXIT
mkdir "$dir/data"

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
  libs=($(pkg-config --libs sdl2 SDL2_image SDL2_ttf gl 2>/dev/null) -lz -lm)
fi

# (1) chave sensivel na lista branca -> "#error" no header gerado.
"${CC:-cc}" tests/farol_ajustes_sensivel.c src/ajlog.c -Isrc \
  -O1 -o "$dir/sensivel"
"$dir/sensivel" > "$dir/sensivel.h"

# (2) teste com o ajustes.c de verdade (padrao AJUSTES_TESTE).
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
"${CC:-cc}" "${sources[@]}" tests/farol_ajustes.c -Isrc -I"$dir" "${sdl[@]}" \
  -D_GNU_SOURCE -DAJUSTES_TESTE -O1 -g -ffunction-sections -fdata-sections "${strip[@]}" \
  "${libs[@]}" \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$dir/teste"
"$dir/teste" "$dir/data"
echo "farol_ajustes: tudo ok"
