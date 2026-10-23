#!/usr/bin/env python3
"""Execute production card placement and hole geometry at every text scale."""
from pathlib import Path
import subprocess
import sys
import tempfile


def function(source, name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


home = (subprocess.check_output(['git', 'show', sys.argv[1] + ':src/home.c'], text=True)
        if len(sys.argv) > 1 else Path('src/home.c').read_text())
trailer = Path('src/trailer.c').read_text()
video = Path('src/video_tpk.c').read_text()
# Exercise the exact hole statements used by pintarCartao, including the
# platform conversion, rather than a second implementation of that conversion.
start = home.index('            GfxRect v = cartaoVideoRect(card);')
end = home.index('            if (v.x != card.x', start)
hole = home[start:end]
code = r'''
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct { float x,y,w,h; } GfxRect;
#define TRAILER_DONO_HOME 1
static float scale;
static int opened, heroTrailerItem, heroAtual, heroTrailerNoCartao, mobile;
static int emTrailer=1, emCartao=1, planoW, planoH, planoX, planoY;
static GfxRect rect;
static int failures, aberto=1, recortePendente;
static void nativoAplicar(void) {}
static void video_janela(int x,int y,int w,int h) {
  planoX=x; planoY=y; planoW=w; planoH=h;
}
static float gfx_escala_tela_txt(void) { return scale; }
static int telefoneui_ativo(void) { return mobile; }
static int trailer_aberto(void) { return opened; }
static int trailer_cheia(void) { return 0; }
static int trailer_dono(void) { return TRAILER_DONO_HOME; }
static int video_tpk_cartao_cover(void) { return 1; }
static int largura=1920, altura=1080;
'''
code += function(trailer, 'naTela') + '\n'
# Keep the production layer conversion and rectangle storage; the platform
# edge records the exact pixels delivered to the native plane / browser.
code += function(trailer, 'trailer_rect') + '\n'
code += 'static void plane(GfxRect r) { trailer_rect(r); }\n'
code += function(video, 'video_tpk_cartao_destino') + '\n'
code += function(video, 'video_tpk_cartao_furo') + '\n'
code += function(home, 'cartaoVideoRect') + '\n'
code += function(home, 'heroTrailerAtualizaRect') + '\n'
code += 'static GfxRect cardHole(GfxRect card) {\n' + hole + '\nreturn v;\n}\n'
code += r'''
static void equal(const char *name, GfxRect actual, GfxRect art) {
  int ok=fabsf(actual.x-art.x)<1.1f && fabsf(actual.y-art.y)<1.1f &&
         fabsf(actual.w-art.w)<1.1f && fabsf(actual.h-art.h)<1.1f;
  printf("%s %s %.0f%%: %.1f,%.1f %.1fx%.1f art %.1f,%.1f %.1fx%.1f\n",
    ok?"PASS":"FAIL",name,scale*100,actual.x,actual.y,actual.w,actual.h,
    art.x,art.y,art.w,art.h);
  failures+=!ok;
}
int main(void) {
  for (int percent=85;percent<=130;percent+=5) {
    scale=percent/100.0f;
    // Final expanded 16:9 artwork, expressed in physical layout pixels.
    GfxRect art={300,580,606,341};
    GfxRect card={art.x/scale,art.y/scale,art.w/scale,art.h/scale};
    heroTrailerItem=heroAtual=0; heroTrailerNoCartao=opened=1;
    // Android TV shares the touch-capable build; mobile also exercises the
    // draw-time hero update that must never steal an active card's plane.
    for (mobile=0;mobile<=1;mobile++) {
      plane(cartaoVideoRect(card));
#ifdef NV_TOUCH_UI
      heroTrailerAtualizaRect((GfxRect){0,0,1920/scale,1080/scale},0);
#endif
      equal("expanded card video plane",rect,art);
      equal("expanded card backend destination",(GfxRect){planoX,planoY,planoW,planoH},art);
    }
    GfxRect hole=naTela(cardHole(card));
    equal("expanded card visible video",hole,art);
    // Hero/detail keep the text-layer conversion introduced by 1ae9bf582.
    heroTrailerNoCartao=0; mobile=1;
    heroTrailerAtualizaRect((GfxRect){0,0,1920/scale,1080/scale},0);
    equal("hero plane",rect,(GfxRect){0,0,1920,1080});
    trailer_rect((GfxRect){0,0,1920/scale,1080/scale});
    equal("detail plane",rect,(GfxRect){0,0,1920,1080});
  }
  return failures?1:0;
}
'''
with tempfile.TemporaryDirectory(prefix='nv-card-rect-') as directory:
    source = Path(directory) / 'rect.c'
    binary = Path(directory) / 'rect'
    source.write_text(code)
    failures = 0
    for platform in ([], ['-DNV_ANDROID', '-DNV_TOUCH_UI'], ['-DNV_TPK']):
        subprocess.run(['cc', '-std=c99', '-O0', *platform, str(source), '-lm', '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)])
        failures += result.returncode != 0
    raise SystemExit(1 if failures else 0)
