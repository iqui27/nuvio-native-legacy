#!/usr/bin/env python3
"""Exercise the actual webOS backdrop wrapper with deterministic renderer state."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
body = (ROOT / "src/source_backdrop.inc").read_text().replace('#include "corviva.h"', '')
assert not any(name in body for name in ["glGenTextures", "glGenFramebuffers", "malloc(", "calloc("])
fixture = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define NV_TELA_W 1920
#define NV_TELA_H 1080
#define NV_COR_FUNDO_R .05f
#define NV_COR_FUNDO_G .05f
#define NV_COR_FUNDO_B .05f
#define GL_COLOR_BUFFER_BIT 1
static unsigned now,tex_n_falta,generation,revision=1,color,content,episodes;
static uint64_t profile=1;
static int opened=1,detail=1,covered=1,player,mini,retained,trailer,trailerOpen,saved,context,menu,all,logOpen,spot,glass;
static int gfx_hdr_output,nv_grad_ativo,anim_politica_reduzida,available=1,snapshot,empty,title,language;
static float nv_ambiente_forca,scale=1;
static int captures,copies,live,draws,pending;
static unsigned SDL_GetTicks(void){return now;}
#define GET(name,value) static int name(void){return value;}
GET(stream_folha_aberta,opened) GET(detail_aberto,detail) GET(detail_cobre_tela,covered)
GET(player_aberto,player) GET(player_mini_ativo,mini) GET(player_retido,retained) GET(trailer_tocando,trailer)
GET(trailer_aberto,trailerOpen)
GET(spainel_visivel,saved) GET(ctx_aberto,context) GET(menu_aberto,menu) GET(vertudo_aberta,all)
GET(registro_aberto,logOpen) GET(spot_cheio,spot) GET(ajustes_vidro,glass) GET(gfx_snap_ok,available)
GET(gfx_snap_ativo,snapshot) GET(corviva_retargets,color) GET(detail_indice,title) GET(ajustes_idioma,language)
GET(cat_revisao_itens,content) GET(cat_versao_episodios,episodes)
static const char *transition="-";
static const char *detail_transicao(void){return transition;}
static uint64_t nv_perfil_sessao_ler(void){return profile;}
static float gfx_escala_ui(void){return scale;}
static unsigned gfx_snap_geracao(void){return generation;}
static void gfx_snap_comecar(void){assert(!snapshot);snapshot=1;captures++;generation++;}
static void gfx_snap_terminar(void){assert(snapshot);snapshot=0;}
static void gfx_snap_desenhar(void){assert(!snapshot);copies++;}
static void gfx_sem_recorte(void){}
static void glClearColor(float a,float b,float c,float d){(void)a;(void)b;(void)c;(void)d;}
static void glClear(int a){assert(a==GL_COLOR_BUFFER_BIT);}
static int gfx_snap_vazio(int *max){*max=empty?0:200;return empty;}
static void background(void *ctx){assert(ctx==(void *)123);draws++;if(pending)tex_n_falta++;}
static void spainel_fundo(int p,unsigned rev,void (*draw)(void *),void *ctx){assert(!p && rev==revision);live++;draw(ctx);}
@BODY@
static void frame(unsigned tick){now=tick;fontes_fundo(0,revision,background,(void *)123);}
int main(void){
 frame(0);frame(699);assert(live==2 && !captures);
 frame(700);assert(captures==1 && copies==1 && draws==3);
 for(unsigned i=701;i<770;i++)frame(i);
 assert(captures==1 && draws==3 && copies==70);
 frame(2700);assert(captures==1); // unchanged background stays cached
 generation++;frame(2701);assert(captures==2); // another modal used the shared FBO
 content++;frame(2702);frame(3402);assert(captures==3); // late title metadata
 revision++;frame(3403);assert(captures==3);frame(4103);assert(captures==4);
 title++;frame(4104);frame(4804);assert(captures==5);
 profile++;frame(4805);frame(5505);assert(captures==6);
 color++;frame(5506);frame(6206);assert(captures==7);
 language++;frame(6207);frame(6907);assert(captures==8);
 scale=1.2f;frame(6908);frame(7608);assert(captures==9);
 episodes++;frame(7609);frame(8309);assert(captures==10);
 for(unsigned i=0;i<17647;i++)frame(now+17);
 assert(captures==10); // no two-second spikes during five minutes of scrolling
 int *guards[]={&player,&mini,&retained,&trailer,&trailerOpen,&saved,&context,&menu,&all,&logOpen,&spot,&glass,&gfx_hdr_output};
 for(unsigned i=0;i<sizeof guards/sizeof *guards;i++){
  int previous=captures;*guards[i]=1;frame(now+1);assert(captures==previous);*guards[i]=0;
  frame(now+1);frame(now+700);assert(captures==previous+1);
 }
 int previous=captures;
 opened=0;frame(now+1);opened=1;frame(now+1);frame(now+700);assert(captures==previous+1);
 previous=captures;transition="desce";frame(now+1);transition="-";frame(now+1);frame(now+700);assert(captures==previous+1);
 previous=captures;nv_grad_ativo=1;frame(now+1);assert(captures==previous);
 anim_politica_reduzida=1;frame(now+1);frame(now+700);assert(captures==previous+1);
 nv_grad_ativo=anim_politica_reduzida=0;
 previous=captures;nv_ambiente_forca=.5f;frame(now+1);assert(captures==previous);nv_ambiente_forca=0;
 available=0;frame(now+1);assert(captures==previous);available=1;
 snapshot=1;frame(now+1);assert(captures==previous);snapshot=0;
 covered=0;frame(now+1);assert(captures==previous);covered=1;
 detail=0;frame(now+1);assert(captures==previous);detail=1;
 frame(now+1);pending=1;frame(now+700);assert(captures==previous+1);
 frame(now+149);assert(captures==previous+1);frame(now+1);assert(captures==previous+2);
 pending=0;frame(now+150);assert(captures==previous+3);
 int settledDraws=draws,settledCopies=copies;
 frame(now+1);frame(now+348);
 assert(captures==previous+3 && copies==settledCopies && draws==settledDraws+2);
 frame(now+1);assert(captures==previous+4); // final logo fade is captured
 frame(now+2000);assert(captures==previous+4);
 // Permanently missing art cannot keep the fast retry cadence forever.
 profile++;frame(now+1);pending=1;frame(now+700);previous=captures;
 for(int i=0;i<89;i++)frame(now+150);
 assert(captures==previous+79);
 pending=0;frame(now+2000);assert(captures==previous+80);
 profile++;frame(0xffffff00u);previous=captures;frame(now+700);assert(captures==previous+1);
 // Empty captures immediately fall back, and never get copied again in this opening.
 opened=0;frame(now+1);opened=1;frame(now+1);empty=1;
 int oldCopies=copies,oldLive=live;frame(now+700);
 assert(copies==oldCopies && live==oldLive+1 && !snapshot);
 previous=captures;frame(now+2000);assert(captures==previous && copies==oldCopies);
 puts("PASS: five-minute settled snapshot reuse without periodic repaints, metadata/episode revisions, late-art fade, ownership, profile/title/theme invalidation, live-video/transition/nested-modal guards and empty-capture fallback; no texture allocations");
}
'''.replace("@BODY@", body)
with tempfile.TemporaryDirectory(prefix="nuvio-source-backdrop-") as directory:
    path = Path(directory) / "test.c"
    path.write_text(fixture)
    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-DNV_SOURCE_BACKDROP_TEST"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(Path(directory) / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(path), "-o", binary], check=True)
    subprocess.run([binary], check=True)
    # Other platform builds retain the original background callback exactly.
    disabled = r'''
#include <assert.h>
static int calls;
static void draw(void *ctx){assert(ctx==(void *)123);calls++;}
static void spainel_fundo(int allow,unsigned revision,void (*fn)(void *),void *ctx){
 assert(allow==1 && revision==42);fn(ctx);
}
@BODY@
int main(void){fontes_fundo(1,42,draw,(void *)123);assert(calls==1);}
'''.replace("@BODY@", body)
    path.write_text(disabled)
    platform_flags = [f for f in flags if f != "-DNV_SOURCE_BACKDROP_TEST"]
    for platform in ["NV_LINUX_DESKTOP", "NV_ANDROID", "NV_TPK", "__EMSCRIPTEN__", "__APPLE__"]:
        subprocess.run([os.environ.get("CC", "cc"), *platform_flags, "-D" + platform, str(path), "-o", binary], check=True)
        subprocess.run([binary], check=True)
    print("PASS: Linux, Android, Tizen and Apple retain the original background drawing")
