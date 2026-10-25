#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Linka somente a lógica exercitada, sem inicializar SDL/AppKit nem vídeo.
# Além de ser rápido, permite ASAN no macOS beta sem o diálogo do loader SDL.
#
# src/fileiras.c src/homefil.c ENTRA NA LISTA porque home_layout.c chama sincronizarFileiras,
# que passou a consultar fil_unir/fil_limite quando a escolha local de fileiras
# nasceu. A lista e escrita a mao, entao ela nao acompanha sozinha: o teste
# ficou sem linkar ("_fil_unir referenced from _sincronizarFileiras") e o
# script morria antes de rodar um caso sequer.
cc ${flags[@]+"${flags[@]}"} src/catalogo.c src/progresso.c src/focus.c src/ajustes.c src/posterprov.c src/redeurl.c src/colecoes.c src/js.c src/catordem.c src/fileiras.c src/homefil.c src/artehero.c src/corviva.c tests/home_tex_duble.c src/cwordem.c src/cwretido.c src/curadoria.c tests/home_layout.c tests/amigosfil_stub.c tests/home_boundary_stub.c \
  -Isrc -o ${TMPDIR:-/tmp}/nuvio-home-tests -O1 -g -ffunction-sections -fdata-sections \
  -DCURADORIA_TESTE -Wl,-dead_strip -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
# Exercise Stage's production draw step with touch ownership enabled.
python3 - <<'PY'
from pathlib import Path
import subprocess
import tempfile

source = Path('src/home.c').read_text()
start = source.index('static void palcoPasso(')
step = source[start:source.index('\n}\n', start) + 3]
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'stage-touch.c'
    test.write_text('''
#include <assert.h>
#define NV_TOUCH_UI 1
#define NV_PALCO_TEXT_DELAY_MS 80
#define NV_PALCO_TEXT_MS 200
typedef unsigned Uint32;
static int toqueLivreX[1], anim_politica_reduzida, column;
static float scrollX[1], palcoPos[1];
static Uint32 palcoUlt[1];
static struct {
  float de, scrollDe, scrollAlvo, texto;
  int alvo, anterior;
} palcoMov[1];
static int colunaDaFileira(int r) { return column; }
static float alvoScrollFil(int r, int c, float x, float abre) { return c * 100; }
static float palcoProgresso(Uint32 ms) { return ms >= 300 ? 1 : ms / 300.f; }
static float anim_mistura(float a, float b, float k) { return a + (b - a) * k; }
static float anim_clamp(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
''' + step + '''
int main(void) {
  palcoPasso(0, 1000);
  column = 4;
  palcoPasso(0, 1100);
  toqueLivreX[0] = 1;
  scrollX[0] = 170; // Drag while the focus animation is running.
  palcoPasso(0, 1200);
  assert(scrollX[0] == 170);
  scrollX[0] = 230; // Inertia after the focus animation has settled.
  palcoPasso(0, 1500);
  assert(scrollX[0] == 230);
  toqueLivreX[0] = 0;
  column = 3;
  palcoPasso(0, 1600);
  assert(scrollX[0] == 230);
  palcoPasso(0, 2000);
  assert(scrollX[0] == 300);
}
''')
    executable = str(Path(directory) / 'stage-touch')
    subprocess.run(['cc', str(test), '-o', executable], check=True)
    subprocess.run([executable], check=True)
print('Stage touch ownership: ok')
PY
${TMPDIR:-/tmp}/nuvio-home-tests
# Exercise the production suppression guard, including Up from the first Split row.
python3 - <<'PY'
from pathlib import Path
import re
import subprocess
import tempfile

source = Path('src/home.c').read_text()
guard = re.search(r'if \(([^;{}]+)\) aTexto = 0\.0f;', source)
assert guard, 'hero copy suppression guard missing'
with tempfile.TemporaryDirectory() as directory:
    test = Path(directory) / 'hero-focus.c'
    test.write_text('''
#include <assert.h>
enum { FILEIRA_NORMAL, FILEIRA_HEROI_DIVIDIDO };
int main(void) {
  struct { int tipo; } fileiras[1];
  struct { int fileira; } foco;
  int nFileiras = 1;
  for (int tipo = FILEIRA_NORMAL; tipo <= FILEIRA_HEROI_DIVIDIDO; tipo++)
    for (int focoHero = 0; focoHero <= 1; focoHero++)
      for (int row = -1; row <= nFileiras; row++) {
        fileiras[0].tipo = tipo;
        foco.fileira = row;
        float aTexto = 1.0f;
        ''' + guard.group(0) + '''
        assert(aTexto == ((!focoHero && row == 0 &&
                            tipo == FILEIRA_HEROI_DIVIDIDO) ? 0.0f : 1.0f));
      }
}
''')
    executable = str(Path(directory) / 'hero-focus')
    subprocess.run(['cc', str(test), '-o', executable], check=True)
    subprocess.run([executable], check=True)
print('hero focus suppression: ok')
PY
