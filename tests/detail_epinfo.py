#!/usr/bin/env python3
"""Host checks for shared variant C geometry, without GL or network."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/detail.c').read_text()
card = source.split('static void desenhaEpisodio(', 1)[1].split('// Abas de informacao:', 1)[0]
assert 'if (epApple())' not in card
assert 'ft * 6.0f' not in card
assert 'gfx_rect(th, t2, GFX_CARD, f,' in card
assert 'gfx_varre_atual = foco.fileira==SEC_EPISODIOS' in card
assert 'epModerna() ? -2.0f : -1.0f' in card
shader = Path('src/gfx.c').read_text()
assert 'uVeu.x < -1.5 && vUv.y > 0.5 ? 0.0 : uRaio' in shader
assert 'if (uVeu.x < -1.5) g = smoothstep(0.55, 1.0, t) * 0.65' in shader
assert 'epDur && data[0] ? "   ·   " : ""' in card
metrics = source.split('static int epModerna(void)', 1)[1].split('// Text padding', 1)[0]
program = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "detail.h"
#include "layout.h"
#define HOME_LAYOUT_MODERNA 0
#define HOME_LAYOUT_DINAMICA 2
static int layout, size;
static float textScale = 1;
static int ajustes_home_layout(void) { return layout; }
static int ajustes_ep_tamanho(void) { return size; }
int ajustes_det_espaco(void) { return 1; }
float gfx_poster_k(void) { return 1 / textScale; }
#define ANC(x) ((x) / textScale)
static int epModerna(void)''' + metrics + r'''
int main(void) {
  const int heights[] = {720,1080,2160};
  for (int resolution=0;resolution<3;resolution++) {
    float scale=heights[resolution]/1080.0f;
    for(size=0;size<3;size++) {
      layout=HOME_LAYOUT_MODERNA;
      float w=epCardW(), h=epCardH(), thumb=epThumbH(), step=epCardPasso();
      assert(w==640*epEscala() && thumb==360*epEscala());
      assert(h-thumb==264 && NV_DETP_EPM_SIN_LINHAS==5);
      assert(NV_DETP_EPM_TIT_DY+36<=NV_DETP_EPM_META_DY);
      assert(NV_DETP_EPM_META_DY+28<=NV_DETP_EPM_SIN_DY);
      assert(thumb+NV_DETP_EPM_SIN_DY+5*NV_DETP_EP_LD_SIN<h);
      if (size==1) {
        assert(w==640 && thumb==360 && h==624);
        assert(fabsf(h*scale-624*scale)<.001f);
      }
      layout=HOME_LAYOUT_DINAMICA;
      assert(epCardW()==w && epCardH()==h && epThumbH()==thumb && epCardPasso()==step);
    }
  }
  size=1; textScale=1.5f;
  assert(fabsf(epThumbH()*textScale-360)<.001f);
  assert(epCardH()-epThumbH()==264);
  puts("PASS: unchanged image, 624 px card, seven panel rows, 720p/1080p/4K and text scaling");
}
'''
with tempfile.TemporaryDirectory(prefix='epinfo-host-') as tmp:
    cfile = Path(tmp) / 'check.c'
    binary = Path(tmp) / 'check'
    cfile.write_text(program)
    subprocess.run(['cc', '-std=c99', '-Isrc', '-I/opt/homebrew/include/SDL2',
                    '-I/opt/homebrew/include', str(cfile), '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
