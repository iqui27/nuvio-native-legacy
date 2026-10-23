#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re, subprocess, tempfile
s = Path('src/vertudo.c').read_text()
def function(name):
    m = re.search(r'static [^\n]+\b' + name + r'\([^\n]*\)\s*\{', s)
    assert m, 'missing function: ' + name
    end = m.end(); depth = 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}'); end += 1
    return s[m.start():end]
# --- Streaming page tabs: catalog order, first tab at the logo, sliding focus.
# Order/geometry/animation live in small pure helpers extracted from vertudo.c.
code = '''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "src/anim.h"
#define COL_SOURCE_MAX 16
typedef struct { char title[128], type[16], catId[64]; } ColSource;
typedef struct { char title[128]; ColSource *sources; int nSources; } ColFolder;
static ColFolder *collection;
static int tabFocus, tabCursor;
#define VT_ABA_FADE 72.0f
#define NV_MOLA_FOCO 25.0f
#define NV_MOLA_DESFOCO 25.0f
static float capX, capW = -1.0f, capAlvoX, capAlvoW, capA;
enum { VT_AB_CHIPS, VT_AB_MENU, VT_AB_BARRA, VT_AB_PAGINAS, VT_AB_TOPO };
static int estAbas = VT_AB_CHIPS;
static int estiloAbas(void) { return estAbas; }
static int cabN(void) { return collection ? collection->nSources : 0; }
static const char *i18n(const char *s) { return s; }
static void col_nome_fonte(const ColSource *s, const char *m, char *dst, unsigned n)
  { (void)m; snprintf(dst, n, "%s", s->title); }
static const char *desc_nome_catalogo(const char *b, const char *t, const char *c)
  { (void)b; (void)t; (void)c; return ""; }
static const char *baseDaFonte(const ColSource *s) { (void)s; return ""; }
''' + function('abaRotulos') + function('abaRolRecentrar') + function('abaFade') + \
    function('abasCapsulaAtualizar') + '''
int main(void) {
  ColSource fs[4]; ColFolder pasta;
  char rot[4][180];
  memset(fs, 0, sizeof fs); memset(&pasta, 0, sizeof pasta);
  strcpy(fs[0].title, "Latest Netflix");    strcpy(fs[0].type, "movie");
  strcpy(fs[1].title, "Latest Netflix");    strcpy(fs[1].type, "series");
  strcpy(fs[2].title, "Top Rated Netflix"); strcpy(fs[2].type, "movie");
  strcpy(fs[3].title, "Top Rated Netflix"); strcpy(fs[3].type, "series");
  pasta.sources = fs; pasta.nSources = 4; collection = &pasta;

  // 1) TAB ORDER: labels follow the catalog (sources) order.
  abaRotulos(rot, 4);
  assert(strstr(rot[0], "Latest Netflix") && strstr(rot[0], "Filmes"));
  assert(strstr(rot[1], "Latest Netflix") && strstr(rot[1], "Séries"));
  assert(strstr(rot[2], "Top Rated Netflix") && strstr(rot[2], "Filmes"));
  assert(strstr(rot[3], "Top Rated Netflix") && strstr(rot[3], "Séries"));
  assert(strncmp(rot[0], "Latest", 6) == 0 && strncmp(rot[2], "Top Rated", 9) == 0);

  // 1b) more sources than COL_SOURCE_MAX must not overflow the label buffers.
  { static ColSource many[20]; static ColFolder big; static char r2[16][180];
    for (int i = 0; i < 20; i++) { snprintf(many[i].title, 128, "Cat %d", i); strcpy(many[i].type, "movie"); }
    big.sources = many; big.nSources = 20; collection = &big;
    int n = big.nSources > COL_SOURCE_MAX ? COL_SOURCE_MAX : big.nSources;
    abaRotulos(r2, big.nSources);
    assert(n == 16 && strstr(r2[15], "Cat 15"));
    collection = &pasta; }

  // 2) FIRST TAB X: the strip rolls to the logo when the target fits whole.
  {
    float W = 1271.0f, GAP = 12.0f, larg = 330.0f, px = 7.0f * (larg + GAP);
    float fimMax = fmaxf(0.0f, px - GAP - W), rol;
    // cursor on the LAST tab: rolls toward the end, clamped at the strip end.
    rol = abaRolRecentrar(0.0f, 6.0f * (larg + GAP), 6.0f * (larg + GAP) + larg, fimMax, W);
    assert(rol > 0.0f && rol <= fimMax + 0.01f);
    assert(6.0f * (larg + GAP) - rol < W);   // the target tab starts on screen
    // back to the FIRST tab: rol 0, first tab left-aligned with the logo.
    rol = abaRolRecentrar(rol, 0.0f, larg, fimMax, W);
    assert(rol == 0.0f);
    // back to the SECOND: fits whole without rolling, row stays on the logo.
    rol = abaRolRecentrar(1117.0f, 330.0f, 660.0f, fimMax, W);
    assert(rol == 0.0f);
    // a late tab that does NOT fit: minimal roll, target whole, out of the fade.
    rol = abaRolRecentrar(0.0f, 5.0f * 342.0f, 5.0f * 342.0f + 330.0f, fimMax, W);
    assert(5.0f * 342.0f + 330.0f - rol <= W - VT_ABA_FADE + 0.01f);
    assert(5.0f * 342.0f - rol >= VT_ABA_FADE - 0.01f);
    assert(rol <= fimMax + 0.01f);
  }
  // 2b) a tab half off the edge stays visible (no vanishing sliver on the left).
  assert(abaFade(-200.0f, 330.0f, 0.0f, 1271.0f, 1, 0) > 0.99f);
  { float f = abaFade(-300.0f, 330.0f, 0.0f, 1271.0f, 1, 0);
    assert(f > 0.0f && f < 1.0f); }                       // the sliver fades, not vanish
  assert(abaFade(-340.0f, 330.0f, 0.0f, 1271.0f, 1, 0) == 0.0f);  // fully out
  assert(abaFade(1300.0f, 330.0f, 0.0f, 1271.0f, 0, 1) == 0.0f);  // not yet entering
  assert(abaFade(1200.0f, 330.0f, 0.0f, 1271.0f, 0, 1) > 0.0f);   // sliver entering: visible

  // 3) FOCUS CAPSULE INTERPOLATES over frames (spring, not a one-frame jump).
  estAbas = VT_AB_CHIPS; tabFocus = 1; tabCursor = 0;
  capAlvoX = 0.0f; capAlvoW = 330.0f;
  abasCapsulaAtualizar(1.0f / 60.0f);
  assert(capX == 0.0f && capW == 330.0f && capA > 0.0f);
  tabCursor = 3; capAlvoX = 1020.0f; capAlvoW = 340.0f;
  { float x0 = capX;
    abasCapsulaAtualizar(1.0f / 60.0f);
    assert(capX > x0 && capX < capAlvoX);   // mid-flight: between the two tabs
    for (int i = 0; i < 40; i++) abasCapsulaAtualizar(1.0f / 60.0f);
    assert(fabsf(capX - capAlvoX) < 1.0f);  // settles on the target tab
    assert(fabsf(capW - capAlvoW) < 1.0f); }
  estAbas = VT_AB_TOPO; tabCursor = 2;      // the Topo style uses the same capsule
  abasCapsulaAtualizar(1.0f / 60.0f);
  assert(capA > 0.05f);
  estAbas = VT_AB_BARRA;                    // other styles: no capsule
  for (int i = 0; i < 90; i++) abasCapsulaAtualizar(1.0f / 60.0f);
  assert(capA < 0.02f);
  printf("streamtabs: order, logo-aligned strip, spring capsule ok\\n");
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d) / 'test.c'; p.write_text(code)
    subprocess.run(['cc', '-fsanitize=address', str(p), '-I.', '-o', d + '/test'], check=True)
    subprocess.run([d + '/test'], check=True)

# Anchors of the fixes that the helpers alone cannot see.
assert 'botaoEstiloAbas' not in s                          # style toggle is inside the layout pill
assert 'ponteiro_alvo(seg.x' in function('botaoExibicao')  # the segment is the D-pad target
assert 'aj_layout-list' in function('botaoExibicao')
assert 'float cx=x0+capX-tabRol;' in function('themeHeader')  # capsule is absolute: strip offset + the logo inset
abaBloco = function('themeHeader')
caps = abaBloco.index('if(capA>.01f)')
texto = abaBloco.index('txt_desenhar_alpha(t,x+PAD')      # capsule drawn BEFORE the labels
assert caps < texto, 'focus capsule must draw before the tab labels'
assert 'capW,H},1.0f,ca)' in abaBloco                  # capsule alpha applied once (ca), not capA*capA
assert 'foco_anel(pill,NV_RAIO_PILL,1,ah)' not in abaBloco   # stiff ring gone
assert 'selecionada&&!f' in abaBloco                      # open tab marker hides when the capsule is on it
assert 'faixas' in function('degradeMarca')               # brand band fades out in alpha
assert 'tabRol = 0.0f;   // a faixa renasce' in s      # reset on open, not only on touch builds
assert 'capAlvoX=pos[focoAba]' in s                       # capsule follows the cursor tab
assert 'abasCapsulaAtualizar(dt);' in s                   # animated every frame
g = Path('src/gfx.c').read_text()
assert 'uPar.x > 0.0' in g                                # GFX_FUNDO_DIN alpha fade
assert 'if(fa>.001f) focoAbaFonte(pill, fa, ah)' not in s  # per-tab instant focus gone
print('vertudo_streamtabs: PASS (catalog order, first tab on the logo, sliding focus, smooth band)')
PY
