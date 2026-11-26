#!/usr/bin/env python3
"""Exercise the production chip renderer with recorded graphics operations."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/streams.c').read_text()
start = source.index('static float selosAspectos[')
end = source.index('// Warm only the bounded badge set', start)
renderer = source[start:end]
fixture = r'''
#include "selospacote.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef unsigned GLuint;
typedef struct {float x,y,w,h;} GfxRect;
typedef struct {int w,h;} TxtLinha;
enum {GFX_TEXTO,GFX_MARCA,TXT_MINI};
static float gfx_tex_aspect_atual;
static SeloFiltro filter;
static unsigned version=1,texture=1;
static float aspect=3;
static int colored,failed,requests;
static int backgrounds,borders,images,labels,mode;
static GfxRect bg,image;
static float bgAlpha,borderAlpha,imageAlpha,labelAlpha;
static int ajustes_selos_coloridos(void){return colored;}
static int ajustes_vidro(void){return 0;}
unsigned selospacote_versao(void){return version;}
const SeloFiltro *selospacote_filtro(unsigned short id){assert(id==0);return &filter;}
static GLuint tex_obter_larg_qualquer(const char *p,float width){assert(!strcmp(p,"a.png")&&width==128);requests++;return texture;}
static float tex_aspecto(const char *p){(void)p;return aspect;}
static int tex_falhou(const char *p){(void)p;return failed;}
static void gfx_cor(GfxRect p,float radius,float r,float g,float b,float a){
 assert(radius>0&&radius<=.5f);backgrounds++;bg=p;bgAlpha=a;
 assert(r>=0&&r<=1&&g>=0&&g<=1&&b>=0&&b<=1);
}
static void gfx_anel(GfxRect p,float radius,float width,float r,float g,float b,float a){
 (void)p;(void)radius;(void)width;(void)r;(void)g;(void)b;borders++;borderAlpha=a;
}
static void gfx_vidro_chip(GfxRect p,float radius,float opacity,float a){(void)p;(void)radius;(void)opacity;(void)a;assert(0);}
static void gfx_rect(GfxRect p,GLuint t,int type,float p0,float p1,float p2,float radius,float r,float g,float b,float a){
 (void)p0;(void)p1;(void)p2;(void)radius;assert(t==texture&&r==1&&g==1&&b==1);
 image=p;mode=type;images++;imageAlpha=a;
}
static TxtLinha txt_linha_corta(int style,const char *s,int r,int g,int b,int a,float width){
 (void)style;(void)r;(void)g;(void)b;assert(a==255);int w=(int)strlen(s)*8;
 return (TxtLinha){w<width?w:(int)width,16};
}
static void txt_desenhar_alpha(TxtLinha l,float x,float y,float a){(void)l;(void)x;(void)y;labels++;labelAlpha=a;}
@RENDERER@
static float render(float width,float alpha,int measure){
 unsigned short id=0;return selosDesenhar(&id,1,10,20,width,24,alpha,1,0,measure);
}
static void clear(void){backgrounds=borders=images=labels=0;}
static void near(float a,float b){assert(fabsf(a-b)<.001f);}
int main(void){
 filter=(SeloFiltro){.nome="Logo",.imagem="a.png",.preenchido=1,.temTag=1,.temBorda=1,
                     .tag={1,.8f,0,.25f},.borda={1,1,1,.5f}};
 float w=render(800,.8f,0);assert(w==78&&backgrounds==1&&borders==1&&images==1&&!labels);
 assert(mode==GFX_TEXTO);near(bgAlpha,.2f);near(borderAlpha,.4f);near(imageAlpha,.8f);
 near(image.x,13);near(image.w,72);near(bg.w,78);near(bg.h,30);
 clear();near(render(800,.8f,1),w);assert(!backgrounds&&!borders&&!images&&!labels);
 clear();assert(render(w-.01f,1,0)==0&&!backgrounds&&!borders&&!images);
 // Outlined and transparent packs must never acquire an invented dark fill.
 clear();filter.preenchido=0;render(800,1,0);assert(!backgrounds&&borders==1&&images==1);
 clear();filter.preenchido=1;filter.tag[3]=filter.borda[3]=0;
 render(800,1,0);assert(!backgrounds&&!borders&&images==1);
 // A previously displayed image evicted by the shared cache retains its slot.
 clear();texture=0;near(render(800,1,0),w);assert(labels==1&&!images&&!backgrounds);
 clear();failed=1;near(render(800,1,0),w);assert(labels==1);
 clear();texture=1;failed=0;near(render(800,1,0),w);assert(images==1&&!labels);
 // Version changes must not reuse aspect ratios from a previous selected pack.
 clear();version++;texture=0;assert(render(800,1,0)==48&&!labels);
 clear();failed=1;assert(render(800,1,0)==50&&labels==1);
 // Text-only fallback respects the same fill, outline and text alpha.
 clear();filter.imagem="";filter.temTexto=1;filter.texto[3]=.5f;
 filter.tag[3]=.25f;filter.borda[3]=.5f;render(800,.8f,0);
 assert(backgrounds==1&&borders==1&&labels==1&&!images);near(labelAlpha,.4f);
 // Builtin white and colored presentation is preserved.
 clear();filter.imagem="a.png";filter.arte=SELO_ARTE_BRANCA;texture=1;
 render(800,1,0);assert(images==1&&mode==GFX_MARCA&&!backgrounds&&!borders);
 clear();colored=1;filter.arte=SELO_ARTE_COR;render(800,1,0);
 assert(images==1&&mode==GFX_TEXTO&&backgrounds==1&&borders==1);
 // Wide and portrait imported logos fit without stretching or exceeding width.
 filter.arte=SELO_ARTE_PACOTE;version++;aspect=20;clear();
 near(render(220,1,0),220);near(image.w/image.h,20);assert(bg.w<=220);
 aspect=.2f;clear();near(render(800,1,0),10.8f);near(image.w/image.h,.2f);
 assert(sizeof selosAspectos<=8000&&requests>0);
 puts("PASS: filled/outlined/transparent image chips, alpha, measure/draw parity, eviction geometry, pack invalidation and builtin styles");
}
'''.replace('@RENDERER@', renderer)
with tempfile.TemporaryDirectory(prefix='nuvio-badge-render-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(fixture)
    flags = ['-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror']
    if os.environ.get('SANITIZE') == '1':
        flags += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run([os.environ.get('CC', 'cc'), *flags, '-I'+str(ROOT/'src'),
                    str(path/'test.c'), '-lm', '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
