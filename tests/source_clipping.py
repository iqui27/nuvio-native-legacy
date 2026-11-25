#!/usr/bin/env python3
"""Compare the actual From addon row drawing with/without offscreen culling."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
s = (ROOT / "src/streams.c").read_text()


def function(signature):
    start = s.index(signature)
    opening = s.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (s[end] == "{") - (s[end] == "}")
        end += 1
    return s[start:end]


fast = function("static void addonDesenhar(")
legacy = re.sub(r"      // Lines outside the list clip.*?< FOLHA_TOPO - 8\.0f\) pula = 1;\n", "", fast, flags=re.S)
assert legacy != fast
legacy = legacy.replace("static void addonDesenhar(", "static void legacyDesenhar(")
fixture = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {float x,y,w,h;} GfxRect;
typedef struct {int w,h;} TxtLinha;
typedef struct {char *vis;size_t nVis;} TextoAddon;
enum {TXT_CALLOUT,TXT_PG_FIM};
#define FOLHA_ADDON_FECHADA 2
#define FOLHA_ADDON_LD 28.0f
#define FOLHA_ADDON_TD 40.0f
#define ADDON_ICONE_W 30.0f
#define ADDON_BANDEIRA_W 36.0f
#define ADDON_GAP 18.0f
static float FOLHA_TOPO=249.25f,folhaBase=972.75f,scale=1;
static int fontHeight=29,calls,lookups,visibleCount,minimal;
static TextoAddon data;
static const TextoAddon *textoAddonDa(int i){(void)i;return &data;}
static int ajustes_fonte_icones_minimalista(void){return minimal;}
static float gfx_escala_ui(void){return scale;}
static const char *addonIcone(int idx,int min,int *cor){*cor=!min && idx%2;return idx==7?NULL:"emoji";}
typedef struct {GfxRect r;float red,green,blue,alpha;int icon;char name[40];} Draw;
static Draw visible[4096],reference[4096];
static void record(GfxRect r,float red,float green,float blue,float alpha,int icon,const char *name){
 calls++;if(r.y+r.h<=FOLHA_TOPO-8.0f || r.y>=folhaBase)return;
 assert(visibleCount<4096);
 Draw d;memset(&d,0,sizeof d);d.r=r;d.red=red;d.green=green;d.blue=blue;d.alpha=alpha;d.icon=icon;
 snprintf(d.name,sizeof d.name,"%s",name);visible[visibleCount++]=d;
}
static int gfx_icone_cor(GfxRect r,const char *name,float alpha){record(r,1,1,1,alpha,2,name);return 1;}
__attribute__((unused)) static void gfx_icone(GfxRect r,const char *name,float red,float green,float blue,float alpha){record(r,red,green,blue,alpha,1,name);}
static void gfx_icone_fmt(GfxRect r,const char *name,float red,float green,float blue,float alpha){record(r,red,green,blue,alpha,3,name);}
static TxtLinha txt_linha_corta(int style,const char *t,int r,int g,int b,int a,float max){
 (void)r;(void)g;(void)b;(void)a;lookups++;
 return (TxtLinha){(int)fminf(max,(float)strlen(t)*7),style==TXT_CALLOUT?fontHeight+8:fontHeight};
}
static void txt_desenhar_tom(TxtLinha l,float x,float y,float r,float g,float b,float a){record((GfxRect){x,y,(float)l.w,(float)l.h},r,g,b,a,0,"");}
@PULA@
@FAST@
@OLD@
static char buf[16384];static size_t used;
static void put(const char *piece,size_t len){assert(used+len<sizeof buf);memcpy(buf+used,piece,len);used+=len;}
static void visual(int logical){char v[5];snprintf(v,sizeof v,"V%03d",logical);put(v,4);}
static void icon(int idx){char v[5];snprintf(v,sizeof v,"I%03d",idx);put(v,4);}
static void text(char kind,const char *t){put(&kind,1);put(t,strlen(t)+1);}
int main(void){
 // Name: three visual lines. Description: 60 visual lines over 20 logical lines,
 // with style blocks, icons (one with no name), flags and text.
 for(int k=0;k<3;k++){visual(0);icon(k);text('T',"The.Lord.of.the.Rings.2160p.UHD.BluRay.REMUX");}
 for(int k=0;k<60;k++){
  visual(1+k/3);put("S",1);icon(k%9);text('B',"pl");text('T',"REMUX DTS-HD MA 5.1 155.48 GB");
  put("E",1);icon(k%5+3);text('T',"filename.mkv");
 }
 data.vis=buf;data.nVis=used;
 int reduced=0,cases=0;
 for(int size=0;size<3;size++)for(int sel=0;sel<2;sel++)for(int open=0;open<2;open++)
 for(int section=0;section<2;section++)for(int offset=-120;offset<120;offset++){
   scale=size==0?.7f:size==1?1:1.5f;fontHeight=size==0?22:size==1?29:48;minimal=offset&1;
   float y=offset*11.13f,abr=open?1.0f:0.0f;
   calls=lookups=visibleCount=0;legacyDesenhar(0,section,90,y,600,640,sel,0,abr,1);
   int count=visibleCount,oldCalls=calls,oldLookups=lookups;
   memcpy(reference,visible,sizeof reference);
   calls=lookups=visibleCount=0;addonDesenhar(0,section,90,y,600,640,sel,0,abr,1);
   assert(visibleCount==count);
   for(int i=0;i<count;i++)assert(!memcmp(&visible[i],&reference[i],sizeof(Draw)));
   assert(calls<=oldCalls && lookups<=oldLookups);if(calls<oldCalls)reduced++;cases++;
 }
 assert(reduced>0);
 printf("PASS: visible From addon text/icons/flags unchanged in %d scroll/scale/focus/open/section cases; fewer offscreen draws in %d\n",cases,reduced);
}
'''
fixture = fixture.replace("@PULA@", function("static size_t addonPula(")).replace("@FAST@", fast).replace("@OLD@", legacy)
with tempfile.TemporaryDirectory(prefix="nuvio-source-clip-") as directory:
    path = Path(directory) / "test.c"
    path.write_text(fixture)
    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(Path(directory) / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(path), "-lm", "-o", binary], check=True)
    subprocess.run([binary], check=True)
