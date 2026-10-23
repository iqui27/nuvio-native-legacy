#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-orcamento.XXXXXX")
trap 'rm -rf "$dir"' EXIT
# Executa o bloco de consumo de app_atualizar sem inicializar todas as telas.
python3 - "$dir/consumir-fonte.h" <<'PY'
from pathlib import Path
import sys
app = Path('src/app.c').read_text()
inicio = app.index('  if (aguardandoFonte == 2 && fonteEscolhida != -2) {')
fim = app.index('\n  // Fonte de canal que nao abre', inicio)
Path(sys.argv[1]).write_text('static void consumirFonte(void) {\n' + app[inicio:fim] + '\n}\n')
PY
flags=(-std=gnu11 -O1 -g -Isrc -I"$dir" -pthread
       -Wall -Wextra -Wno-deprecated-declarations -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  linker=(-Wl,-dead_strip)
else
  flags+=(-DNV_LINUX_DESKTOP)
  linker=(-Wl,--gc-sections -lm)
  if pkg-config --exists sdl2; then
    read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
    flags+=("${sdk[@]}")
  fi
fi
if [ -n "${CFLAGS:-}" ]; then read -r -a extra <<< "$CFLAGS"; flags+=("${extra[@]}"); fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -DFONTESMORTAS_ORCAMENTO \
  -Dfa_concluir=teste_fa_concluir -Dfa_player_falhou_foi=teste_fa_player_falhou_foi \
  -Dstream_falha_verificar=teste_stream_falha_verificar \
  -c tests/fontesmortas.c -o "$dir/streams.o"
cc "${flags[@]}" tests/fontesmortas_orcamento.c "$dir/streams.o" src/fontesmortas.c src/fonteantecipa.c \
  src/fontefalha.c src/fontefiltro.c src/fontenota.c src/linguas.c "${linker[@]}" -o "$dir/teste"
"$dir/teste"
"$dir/teste" sucesso
