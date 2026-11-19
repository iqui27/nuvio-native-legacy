#!/bin/bash
# #458: dedup por copia DEPOIS dos chips em "Todas" — chama passaFiltro de
# streams.c de verdade (incluido no TU do teste), com stubs gerados por nm.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-d1-458.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Simbolos realmente usados = os que o linker pede DEPOIS de --gc-sections
# (secoes mortas nao entram; gerar stub para elas quebrava tipos/dados).
gcc -Isrc -ffunction-sections -fdata-sections -c tests/d1_fontes_duplicadas.c -o "$work/d458.o" 2>/dev/null
gcc "$work/d458.o" -o /dev/null -Wl,--gc-sections -lm -lpthread 2>&1 \
  | grep -oP "undefined reference to \`\K[^']+" | sort -u > "$work/undef.txt" || true
{
  echo '#include <stddef.h>'
  echo 'typedef unsigned int Uint32;'
  while read -r s; do
    case "$s" in
      SDL_GetTicks) echo 'Uint32 SDL_GetTicks(void){return 0;}';;
      addonmodo_permite|fontefiltro_permite|perfis_sessao_atual|nv_perfil_sessao_atual|nv_perfil_sessao_travar|fonteregra_no_escopo|stream_filtro_permite)
        echo "int $s(void) { return 1; }";;
      *) echo "int $s(void) { return 0; }";;
    esac
  done < "$work/undef.txt"
} > "$work/stubs.c"
gcc "$work/d458.o" "$work/stubs.c" -o "$work/d458" -Wl,--gc-sections -lm -lpthread
"$work/d458"
printf 'd1_fontes_duplicadas.sh: ok\n'
