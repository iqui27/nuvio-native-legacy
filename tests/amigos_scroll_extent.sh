#!/bin/bash
# Style-5 touch extent includes the last expanded title panel; no GL required.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-amigos-extent.XXXXXX")
trap 'rm -rf "$work"' EXIT
src=src
# Compile the actual style-5 geometry without GL; assert the last panel fits at max scroll.
python3 - "$src/amigosfil.c" "$work/extent.c" <<'PY'
import pathlib
import sys
source = pathlib.Path(sys.argv[1]).read_text()
geometry = source[source.index("static float alturaCartao("):source.index("void amigosfil_retomar_foco(")]
defines = "\n".join(line for line in source.splitlines() if line.startswith("#define AF_"))
pathlib.Path(sys.argv[2]).write_text('''
#include <assert.h>
#include <math.h>
#define NV_TOUCH_UI
#define NV_TELA_W 800.0f
#define NV_TELA_H 1200.0f
#define NV_HOME_SAFE_RIGHT 48.0f
#define SV_AMIGOS_MAX 4
typedef struct { int nTit; } SvAmigo;
typedef struct { float x, y, w, h; } GfxRect;
static SvAmigo friends[4] = {{3}, {3}, {3}, {3}};
static float abre[5], scroll, maximum;
static int toque;
static const SvAmigo *socialvis_amigo(int i) { return &friends[i]; }
static float ajustes_rail_largura_fixa(void) { return 0; }
static float ajustes_conteudo_x(void) { return 48; }
static float gfx_escala(void) { return 1; }
static void toquerol_vincular(int *t, GfxRect r, float scale, float min,
                             float max, int axis, float *offset) { maximum = max; }
''' + defines + '\n' + geometry + '''
#endif
int main(void) {
  for (int n = 1; n <= 4; n++) {
    for (int step = 0; step <= 2; step++) {
      for (int i = 0; i < n; i++) abre[i] = 0;
      abre[n - 1] = step * 0.5f;
      scroll = 123;
      amigosfilToqueVincular(48, 200, 240, 0, n);
      float end = rostoX(n - 1, 48, 240) + scroll + AF_D;
      if (step) end += AF_VAO + larguraPainel(3, 240) * abre[n - 1];
      assert(end - maximum <= amigosfilDireita() + 0.01f);
    }
  }
  amigosfilToqueVincular(48, 200, 240, 0, 0);
  assert(maximum == 0);
}
''')
PY
cc "$work/extent.c" -o "$work/extent" -lm
"$work/extent"
echo "amigos scroll extent: ok"
