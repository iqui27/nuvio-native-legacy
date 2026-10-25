"""Execute the approved portrait focus panel across focus and page fades."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/detail.c').read_text()
start = source.index('    if (f > .005f) {', source.index('static void desenhaEpisodio('))
tile = source[start:source.index('    float metaY', start)]
program = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct { float x,y,w,h; } GfxRect;
static int modern, draws, lights;
static float alpha, red, green, blue;
static int ajustes_borda_foco(void) { return modern; }
static void gfx_anel(GfxRect r,float radius,float width,float cr,float cg,float cb,float a) {
  (void)r; (void)radius; (void)width; (void)cr; (void)cg; (void)cb;
  assert(a>=0 && a<=.18f); lights++;
}
static void gfx_cor(GfxRect r,float radius,float cr,float cg,float cb,float a) {
  (void)r; (void)radius;
  draws++; alpha=a; red=cr; green=cg; blue=cb;
}
static void draw(float ft,float a) {
  float f=ft;
  GfxRect info={0,254,420,276}, th={0,0,420,236};
''' + tile + r'''
}
int main(void) {
  for(modern=0;modern<=1;modern++)
    for(int focus=0;focus<=100;focus++)
      for(int fade=1;fade<=2;fade++) {
        float a=fade*.5f; draws=lights=0; alpha=0;
        float ft=focus/100.0f;
        draw(ft,a);
        int shown=focus>0;
        assert(draws==shown && lights==(shown && modern));
        assert(fabsf(alpha-(shown?.64f*ft*a:0))<.00001f);
        if (shown) assert(red==.19f && green==.21f && blue==.23f);
      }
  puts("PASS: portrait panel absent at rest, translucent on focus across fades");
}
'''
with tempfile.TemporaryDirectory(prefix='episode-tint-') as tmp:
    cfile = Path(tmp) / 'tint.c'
    binary = Path(tmp) / 'tint'
    cfile.write_text(program)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(cfile),
                    '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
