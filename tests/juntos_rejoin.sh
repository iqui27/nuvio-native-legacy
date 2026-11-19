#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-rejoin.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Exercise the actual Back exit/retry branch alongside the room motor.
python3 - "$work/player_back.h" <<'PY'
import sys
from pathlib import Path
source = Path('src/player.c').read_text()
start = source.rfind('#ifdef NV_ANDROID', 0, source.index('      player_voltar_a_esperar(); pedFontes = 1;', source.index('// Back dismisses exactly one layer.')))
end = source.index('\n  }', start)
Path(sys.argv[1]).write_text(source[start:end])
# Compile the common teardown entry before unrelated progress/backend cleanup.
start = source.index('static void fecharSessao(')
end = source.index('  saiuVisto = 0;', start)
Path(sys.argv[1]).with_name('player_close.h').write_text(
    source[start:end] + '  (void)manter; (void)trocaFonte; playerOpen = prepared = 0;\n}\n')
# Every flagged build uses the same exit sink; verify lifecycle routes there too.
start = source.index('void player_ciclo_evento(')
end = source.index('\nvoid player_validar_retido(', start)
assert 'if (aberto && juntos_no_player()) player_encerrar();' in source[start:end]
app = Path('src/app.c').read_text()
for caller in ('static void abrirParte(', 'static void tocarFonteEscolhida('):
    start = app.index(caller)
    end = app.index('juntos_retomar_player(0)', start)
    block = app[start:end]
    assert block.index('int retomarSala = juntos_no_player()') < block.index('player_encerrar')
start = app.index('static void juntosAbrirEFonte(')
assert 'juntos_retomar_player(escolherFonteSala)' in app[start:]

# Compile the production input dispatch prefix and complete drawing entry point.
# Rendering primitives are counters; no SDL window or GPU is needed.
ui = Path('src/juntosui.c').read_text()
def function(signature):
    start = ui.index(signature)
    brace = ui.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (ui[end] == '{') - (ui[end] == '}')
        end += 1
    return ui[start:end]
event = function('int juntosui_evento_sdl(')
end = event.index('  if (camada == CAMADA_NADA) return 0;')
event = event[:end] + '  return camada != CAMADA_NADA;\n}'
Path(sys.argv[1]).with_name('juntos_ui_boundary.h').write_text(
    event + '\n' + function('void juntosui_desenhar('))
PY
for platform in desktop NV_ANDROID NV_WEBOS __EMSCRIPTEN__ NV_TPK NV_TPK40; do
flags=()
if [ "$platform" != desktop ]; then flags+=("-D$platform"); fi
cc "${flags[@]}" -std=gnu11 -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -I"$work" \
  -ffunction-sections -fdata-sections tests/juntos_rejoin.c src/juntosfonte.c \
  src/juntosmotor.c src/juntosproto.c src/juntossync.c src/js.c \
  -L/opt/homebrew/lib -lSDL2 -Wl,-dead_strip -o "$work/rejoin"
printf 'teardown platform: %s\n' "$platform"
"$work/rejoin"
done
