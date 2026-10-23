"""Execute approved Modern C panel across focus and page fades."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/detail.c').read_text()
start = source.index('  if (epModerna() && ft > .005f)', source.index('static void desenhaEpisodio('))
tile = source[start:source.index('  // DESFOCAR NAO ASSISTIDOS', start)]
program = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
typedef struct { float x,y,w,h; } GfxRect;
static int modern, draws, lights;
static float alpha, red, green, blue;
static int epModerna(void) { return modern; }
static void gfx_cor(GfxRect r,float radius,float cr,float cg,float cb,float a) {
  (void)r; (void)radius;
  draws++; alpha=a; red=cr; green=cg; blue=cb;
}
static void gfx_luz_canto(GfxRect r,float radius,float x,float y,float w,
                          float cr,float cg,float cb,float a) {
  (void)r; (void)radius; (void)x; (void)y; (void)w;
  assert(cr==.72f && cg==.77f && cb==.86f && a>=0 && a<=.04f);
  lights++;
}
static void draw(float ft,float a) {
  GfxRect epTile={0,328,640,296};
''' + tile + r'''
}
int main(void) {
  for(modern=0;modern<=1;modern++)
    for(int focus=0;focus<=100;focus++)
      for(int fade=1;fade<=2;fade++) {
        float a=fade*.5f; draws=lights=0; alpha=0;
        float ft=focus/100.0f;
        draw(ft,a);
        int shown=modern && focus>0;
        assert(draws==shown && lights==shown);
        assert(fabsf(alpha-(shown?.65f*ft*a:0))<.00001f);
        if (shown) assert(red==.071f && green==.075f && blue==.086f);
      }
  puts("PASS: Modern C panel absent at rest, translucent on focus across fades");
}
'''
with tempfile.TemporaryDirectory(prefix='episode-tint-') as tmp:
    cfile = Path(tmp) / 'tint.c'
    binary = Path(tmp) / 'tint'
    cfile.write_text(program)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', str(cfile),
                    '-lm', '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
