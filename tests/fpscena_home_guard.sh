#!/bin/bash
# Exercise the production FPS callback with each keyboard-capturing Home layer.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fpscena-guard.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/guard.c" <<'PY'
import pathlib, re, sys
app = pathlib.Path('src/app.c').read_text()
main = pathlib.Path('src/main.c').read_text()
def function(source, name):
    match = re.search(r'(?:static )?int ' + name + r'\([^)]*\) \{.*?\n\}', source, re.S)
    assert match, name
    return match[0]
functions = [function(app, 'app_na_home')]
if 'int app_home_recebe_teclas(void)' in app:
    functions += [function(app, 'relogioCamadaAberta'), function(app, 'app_home_recebe_teclas')]
functions += [function(main, 'fpsEstado')]
if 'int app_home_recebe_teclas(void)' in app:
    startup = main[main.index('// FPS REGRESSION SCENES'):]
    assert re.search(r'if \(osd \?.* : app_home_recebe_teclas\(\)\)', startup)
names = set(re.findall(r'\b(\w+)\(\)', '\n'.join(functions)))
names -= {'app_na_home', 'app_home_recebe_teclas', 'relogioCamadaAberta'}
layers = ['ctx_aberto', 'menu_aberto', 'spainel_aberto', 'vertudo_aberta',
          'central_aberta', 'ilha_modal_aberto', 'celb_aberto', 'spot_aberto',
          'registro_aberto', 'registro_envio_aberto', 'estatvivo_aberto',
          'avisos_aberto', 'faixas_aberta', 'episodios_aberto',
          'stream_folha_aberta', 'guia_overlay_aberta', 'novidades201_aberto',
          'diagnostico_intro_aberto', 'recenviar_aberto', 'pessoas_aberto',
          'teclado_aberto', 'texto_sistema_aberto']
names.update(layers)
code = '#include <assert.h>\n#include <string.h>\n#include <stdio.h>\n#include "fpscena.h"\n'
code += 'enum { TELA_HOME }; static int tela = TELA_HOME, homePronta = 1;\n'
for name in sorted(names):
    code += f'static int v_{name}; static int {name}(void) {{ return v_{name}; }}\n'
code += '\n'.join(functions) + '\nint main(void) {\n'
code += 'assert(fpsEstado("home-scroll")); fpscena_estado(fpsEstado);\n'
for name in layers:
    code += f'''v_{name} = 1;
    if (fpsEstado("home-scroll")) {{ puts("FAIL: {name} accepted as Home"); return 1; }}
    fpscena_iniciar("home-scroll", "home-scroll 1.0 0\\n");
    for (int i = 0; i < 5000 && fpscena_ativo(); i++)
      fpscena_quadro(0.8, 0);
    assert(!fpscena_ativo() && fpscena_falhas() == 1);
    v_{name} = 0; assert(fpsEstado("home-scroll"));
'''
code += 'puts("ok: Home overlays reject FPS measurements"); return 0; }\n'
pathlib.Path(sys.argv[1]).write_text(code)
PY
cc "$work/guard.c" src/fpscena.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -lm -o "$work/guard"
"$work/guard"
