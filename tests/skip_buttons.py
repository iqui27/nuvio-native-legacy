#!/usr/bin/env python3
"""Actual player button drawing, navigation gate and stale pointer callback."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/player.c').read_text()

def function(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    while ';' in source[start:opening]:
        start = source.index(signature, start + len(signature))
        opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

fixture = r'''
#include "intro.h"
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
typedef struct {int unused;} CatEp;
typedef struct {float x,y,w,h;} GfxRect;
typedef struct {float w,h;} TxtLinha;
enum {TXT_G21B};
#define NV_VTELA_W 1920
#define NV_VTELA_H 1080
#define PLR_MARGEM 96
static int mask=7,markerType,available=1,cached,draws,targets,wakes;
static int skipFoco,barraFoco,visivel,duracaoReal=1,epE=1;
static double posSeg=40,duracaoSeg=3000;
static float anim=1,entrada=1;
static int ajustes_show_skip_intro(void){return !!(mask&1);}
static int ajustes_show_skip_recap(void){return !!(mask&2);}
static int ajustes_show_skip_credits(void){return !!(mask&4);}
static int skipDisponivel(void){return available;}
int intro_botao_visivel(double *end,int *type){if(end)*end=90;if(type)*type=markerType;return cached;}
int intro_botao_config(double pos,double now,int osd,int focused,double *end,int *type,int intro,int recap,int credits){
 (void)pos;(void)now;(void)osd;(void)focused;
 assert(intro==!!(mask&1)&&recap==!!(mask&2)&&credits==!!(mask&4));
 int enabled=markerType==INTRO_ABERTURA?intro:markerType==INTRO_RESUMO?recap:credits;
 cached=enabled;if(enabled){*end=90;*type=markerType;}return enabled;
}
void intro_definir_duracao(double dur,int film){assert(dur==3000&&!film);}
static double monoSeg(void){return 100;}
static const CatEp *player_proximo_episodio(void){return NULL;}
static void acordar(void){wakes++;}
static int posplay_visivel(void){return 0;}
static int ponteiroNoPlayer(void){return 1;}
static int plrui_tinta(void){return 255;}
static float transporteBarraY(void){return 980;}
static TxtLinha txt_linha(int style,const char *text,int r,int g,int b,int a){(void)style;(void)text;(void)r;(void)g;(void)b;(void)a;return (TxtLinha){100,24};}
static void txt_desenhar_alpha(TxtLinha l,float x,float y,float a){(void)l;(void)x;(void)y;(void)a;}
static void ponteiro_alvo(float x,float y,float w,float h,void (*fn)(int,int),void *unused,int a,int b){(void)x;(void)y;(void)w;(void)h;(void)fn;(void)unused;(void)a;(void)b;targets++;}
static void gfx_veu_css(GfxRect r,int mode,float x,float y,float a){(void)r;(void)mode;(void)x;(void)y;(void)a;}
static void plrui_pilula_foco(GfxRect r,float a){(void)r;(void)a;draws++;}
static void plrui_disco_osd(GfxRect r,float a){(void)r;(void)a;draws++;}
static void gfx_icone(GfxRect r,const char *icon,float red,float g,float b,float a){(void)r;(void)icon;(void)red;(void)g;(void)b;(void)a;}
@HELPER@
@VISIBLE@
@POINTER@
@DRAW@
int main(void){
 for(mask=0;mask<8;mask++)for(markerType=INTRO_ABERTURA;markerType<=INTRO_CREDITOS;markerType++)
 for(visivel=0;visivel<2;visivel++){
  int enabled=!!(mask&(1<<(markerType-1)));
  draws=targets=wakes=0;cached=1;skipFoco=barraFoco=1;
  desenharAcoesEpisodioCorpo();
  assert(draws==enabled&&targets==enabled&&trechoPulavel(NULL)==enabled);
  if(!enabled)assert(!skipFoco);
  skipFoco=barraFoco=0;cached=1;ponteiroSkip(0,0);
  assert(skipFoco==enabled&&barraFoco==enabled&&wakes==enabled);
 }
 mask=7;markerType=INTRO_ABERTURA;cached=1;available=0;skipFoco=1;draws=targets=0;
 desenharAcoesEpisodioCorpo();assert(!draws&&!targets&&!skipFoco&&!trechoPulavel(NULL));
 puts("PASS: actual player drawing and focus/pointer gates hide only disabled buttons in all 8 settings combinations, with controls open/closed and stale callbacks");
}
'''
for token, signature in [('HELPER','static int skipBotaoPermitido('),('VISIBLE','static int trechoPulavel('),
                         ('POINTER','static void ponteiroSkip('),('DRAW','static void desenharAcoesEpisodioCorpo(')]:
    fixture = fixture.replace('@'+token+'@', function(signature))
with tempfile.TemporaryDirectory(prefix='nuvio-skip-buttons-') as directory:
    temp=Path(directory);(temp/'test.c').write_text(fixture)
    flags=['-std=gnu11','-O1','-g','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src')]
    if os.environ.get('SANITIZE')=='1':
        flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    subprocess.run([os.environ.get('CC','cc'),*flags,str(temp/'test.c'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
