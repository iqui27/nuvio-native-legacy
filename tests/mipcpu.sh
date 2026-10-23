#!/bin/bash
# Piramide de mipmaps na CPU (#201): tamanhos, cor media e acesso fora da
# superficie.
#
# Guard Malloc (libgmalloc) e nao ASan: com -fsanitize o sdl2-compat do
# Homebrew nao carrega a SDL3 e trava num dialogo antes do main (o mesmo
# ambiente em que cwordem/cwremover abortam sob ASan). Com MALLOC_STRICT_SIZE
# cada bloco termina colado numa pagina protegida: ler ou escrever um byte
# alem da superficie (ou de qualquer buffer do tex_reduzir) e SIGBUS na hora.
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
out="${TMPDIR:-/tmp}/nuvio-mipcpu"
cc "${sources[@]}" tests/mipcpu.c -Isrc -o "$out" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib MALLOC_STRICT_SIZE=1 "$out" 2>&1 |
  grep -v '^GuardMalloc\['
exit "${PIPESTATUS[0]}"
