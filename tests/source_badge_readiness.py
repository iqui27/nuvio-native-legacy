#!/usr/bin/env python3
"""Real badge worker/readiness and both source-row draw policies, without SDL or network."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/streams.c").read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
#include "selospacote.h"
#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
 char url[64], rotulo[32]; unsigned short selosPacote[16];
 unsigned char nSelosPacote; unsigned selosPacoteVer;
 unsigned long long badges; int dolbyVision; char descricao[64];
} Stream;
static Stream rows[1001], *lista=rows;
static int n=2, pack=0, available=1, schedules, aberta=1;
static float anim=1;
static unsigned version=7, listaGeracao=1, selosPedVer, selosPedGer;
static pthread_mutex_t verTrava=PTHREAD_MUTEX_INITIALIZER;
int selospacote_ativo(void){return pack;}
unsigned selospacote_versao(void){return version;}
int selospacote_embutidos_ok(void){return available;}
void selospacote_colorido(int c){(void)c;}
static int ajustes_selos_coloridos(void){return 0;}
static void selosAgendar(int start,int count){assert(start==0&&count==n);schedules++;selosPedVer=version;selosPedGer=listaGeracao;}
@STATE@
@JOB_TYPE@
static int calculations, scenario;
static void selosCalcular(Stream *s){
 if(calculations==1 && scenario==0)assert(lista[0].selosPacoteVer==version);
 s->nSelosPacote=1;s->selosPacote[0]=1;s->selosPacoteVer=version;
 calculations++;
 if(scenario==1)version++;
 if(scenario==2)listaGeracao++;
}
@WORKER@
@NO_WORK@
static SeloFiltro filters[]={
 {.nome="",.imagem=""},
 {.nome="AC3",.imagem="a.png",.arte=SELO_ARTE_COR},
 {.nome="5.1",.imagem="b.png",.arte=SELO_ARTE_COR},
 {.nome="4K",.imagem="r.png",.arte=SELO_ARTE_COR,.resolucao=1}
};
static int loaded[4], failed[4], requested[4];
const SeloFiltro *selospacote_filtro(unsigned short id){return id<4?&filters[id]:NULL;}
static int imageId(const char *p){return p[0]=='a'?1:p[0]=='b'?2:3;}
static unsigned tex_obter_larg_qualquer(const char *p,float width){assert(width==128);int id=imageId(p);requested[id]++;return loaded[id];}
static int tex_falhou(const char *p){return failed[imageId(p)];}
@IMAGES@
static int nOrdem,ordem[1001];static float rolagem;
static float areaLista(void){return 200;}
static int primeiraVisivel(float y){int r=(int)y/100;return r<nOrdem?r:nOrdem-1;}
@VIEW@
static int drawnPack, drawnLegacy;
static float desenharSelosPacote(const Stream *s,float x,float y,float w,float h,float a,float t,int skip){(void)s;(void)x;(void)y;(void)w;(void)h;(void)a;(void)t;(void)skip;drawnPack++;return 20;}
static unsigned long long logosDa(const Stream *s,int skip){(void)skip;return s->badges;}
static unsigned long long badges_bit(const char *p){(void)p;return 1;}
static float badges_desenhar_selos(unsigned long long m,float x,float y,float w,float h,float a){(void)m;(void)x;(void)y;(void)w;(void)h;(void)a;drawnLegacy++;return 20;}
static float badges_desenhar_tom(unsigned long long m,float x,float y,float w,float h,float r,float g,float b,float a){(void)r;(void)g;(void)b;return badges_desenhar_selos(m,x,y,w,h,a);}
static int badges_fonte_hdr_marca(unsigned long long m){(void)m;return 1;}
enum {FMT_SDR};
#define FOLHA_SELO_H 24
static void marca_formato(int f,float x,float y,float h,float r,float g,float b,float a){(void)f;(void)x;(void)y;(void)h;(void)r;(void)g;(void)b;(void)a;}
static int selosPacoteVisiveis(Stream *s){return s->nSelosPacote>0;}
typedef struct {int w,h;} TxtLinha;
enum {TXT_PG_FIM};
static TxtLinha txt_linha_corta(int e,const char *p,int r,int g,int b,int a,float w){(void)e;(void)p;(void)r;(void)g;(void)b;(void)a;(void)w;return (TxtLinha){0,0};}
static void txt_desenhar_alpha(TxtLinha l,float x,float y,float a){(void)l;(void)x;(void)y;(void)a;}
// From addon rows: the badge row function itself.
@DRAW@
static void drawRowAddon(Stream *s){addonSelos(s,0,0,800,1,1);}
// From Nuvio rows: the badge block of corpoFolha, in its own locals.
static void drawRowNuvio(Stream *s){float tx0=0,cy=0,txtW0=800,mpW=0,t=.78f,lw=0;int sel=1,ehMp4=0;
 unsigned long long logos=logosDa(s,0);
@NUVIO@
 (void)lw;
}
static void (*drawRow)(Stream *s);
static SelosLote *job(int count){SelosLote *l=calloc(1,sizeof *l);assert(l);l->ger=listaGeracao;l->ver=version;l->k=count;return l;}
int main(int argc,char **argv){
 (void)argv;drawRow=argc>1?drawRowNuvio:drawRowAddon;
 int ready=9;strcpy(rows[0].url,"a");strcpy(rows[0].rotulo,"one");rows[0].badges=1;
 assert(selosPacoteEstado(rows,&ready)==0&&!ready&&schedules==1);
 drawRow(rows);assert(!drawnPack&&!drawnLegacy&&schedules==1);
 rows[0].selosPacoteVer=version;rows[0].nSelosPacote=1;rows[0].selosPacote[0]=1;
 drawRow(rows);assert(!drawnPack&&!drawnLegacy&&requested[1]);
 loaded[1]=1;drawRow(rows);assert(drawnPack==1&&!drawnLegacy);
 // Reproduce the screenshot: first row ready, second row delayed. The second
 // must show the same pack as soon as ready, without closing/reopening.
 drawRow(&rows[1]);assert(drawnPack==1&&!drawnLegacy);
 rows[1].selosPacoteVer=version;rows[1].nSelosPacote=1;rows[1].selosPacote[0]=1;
 drawRow(&rows[1]);assert(drawnPack==2&&!drawnLegacy);
 assert(selosVistaDa(rows,0)==SELOS_VISTA_PACOTE&&selosVistaDa(&rows[1],0)==SELOS_VISTA_PACOTE);
 selosVistaLimpar();rows[0].nSelosPacote=0;drawRow(rows);assert(drawnPack==2&&!drawnLegacy);
 selosVistaLimpar();pack=-1;available=0;rows[0].selosPacoteVer=0;drawRow(rows);assert(drawnLegacy==1);
 pack=0;available=1;
 selosVistaLimpar();rows[0].selosPacoteVer=version;rows[0].nSelosPacote=1;rows[0].selosPacote[0]=1;
 assert(selosVistaDa(rows,0)==SELOS_VISTA_PACOTE);
 version++;assert(selosVistaDa(rows,0)==SELOS_VISTA_PENDENTE);
 rows[0].selosPacoteVer=version;assert(selosVistaDa(rows,0)==SELOS_VISTA_PACOTE);
 rows[0].selosPacoteVer=version;rows[0].nSelosPacote=2;rows[0].selosPacote[0]=1;rows[0].selosPacote[1]=2;
 assert(!selosImagensProntas(rows,0)&&requested[2]);
 failed[2]=1;assert(selosImagensProntas(rows,0));
 rows[0].selosPacote[1]=3;assert(selosImagensProntas(rows,1)&&!requested[3]);
 Stream copy=rows[0];copy.selosPacoteVer=0;
 assert(selosPacoteEstado(&copy,&ready)==2&&ready);
 n=0;copy.selosPacoteVer=0;assert(selosPacoteEstado(&copy,&ready)==0&&ready);
 n=1000;memset(rows,0,sizeof rows);calculations=0;selosFio(job(n));assert(calculations==1000&&rows[999].selosPacoteVer==version);
 // A stale pack result must not erase a newer result already in the list.
 n=2;scenario=1;calculations=0;rows[0].selosPacoteVer=version+1;rows[0].nSelosPacote=3;selosFio(job(1));assert(rows[0].nSelosPacote==3);
 scenario=2;calculations=0;selosFio(job(1000));assert(calculations==1);
 // Stale jobs stop before copying or matching the replacement list/pack.
 scenario=0;calculations=0;SelosLote *stale=job(1000);listaGeracao++;
 selosFio(stale);assert(!calculations);
 stale=job(1000);version++;selosFio(stale);assert(!calculations);
 // Read each row from the live list, rather than uninitialized scratch.
 n=2;rows[0].selosPacoteVer=rows[1].selosPacoteVer=0;
 strcpy(rows[0].rotulo,"copied on worker");
 selosFio(job(n));assert(calculations==2&&rows[1].selosPacoteVer==version);
 rows[0].selosPacoteVer=0;selosSemTrabalho(0,1,listaGeracao,version);assert(rows[0].selosPacoteVer==version&&rows[0].nSelosPacote==0);
 rows[0].nSelosPacote=2;selosSemTrabalho(0,1,listaGeracao-1,version);assert(rows[0].nSelosPacote==2);
 selosVistaLimpar();scenario=0;n=1000;
 for(int i=0;i<n;i++){rows[i].selosPacoteVer=version;rows[i].nSelosPacote=1;rows[i].selosPacote[0]=1;}
 // Prefetch is closed-sheet only, capped at 8 rows and 2 completed rows/frame.
 int beforeWarm=requested[1];aberta=1;selosAquecerPasso();assert(requested[1]==beforeWarm);
 aberta=0;anim=0;selosAquecerPasso();assert(requested[1]-beforeWarm==2);
 for(int i=0;i<20;i++){selosAquecerPasso();}assert(requested[1]-beforeWarm==8);
 beforeWarm=requested[1];for(int i=0;i<20;i++){selosAquecerPasso();}assert(requested[1]==beforeWarm);
 // Current viewport 4..6, exactly two offscreen neighbours 3 and 7.
 nOrdem=n;rolagem=400;for(int i=0;i<n;i++)ordem[i]=n-1-i;
 rows[ordem[3]].selosPacote[0]=2;rows[ordem[7]].selosPacote[0]=2;
 int req1=requested[1],req2=requested[2];aberta=1;
 selosAquecerVizinhos();assert(requested[2]==req2+2&&requested[1]==req1);
 aberta=0;selosAquecerVizinhos();assert(requested[2]==req2+2);
 nOrdem=0;aberta=1;selosAquecerVizinhos();assert(requested[2]==req2+2);
 selosVistaVer=version;selosVistaSemMemoria=1;assert(selosVistaDa(rows,0)==SELOS_VISTA_PACOTE&&!selosVistaCap);selosVistaSemMemoria=0;
 for(int i=0;i<n;i++){rows[i].selosPacoteVer=version;rows[i].nSelosPacote=1;rows[i].selosPacote[0]=1;assert(selosVistaDa(&rows[i],0)==SELOS_VISTA_PACOTE);}
 int requestsBefore=requested[1], schedulesBefore=schedules;
 struct timespec begin,end;clock_gettime(CLOCK_MONOTONIC,&begin);
 volatile int result=0;
 for(int i=0;i<2000000;i++)result=selosVistaDa(&rows[i%1000],0);
 clock_gettime(CLOCK_MONOTONIC,&end);
 assert(result==SELOS_VISTA_PACOTE&&requested[1]==requestsBefore&&schedules==schedulesBefore);
 printf("badge view cache: 1000 cards, 2M hits, %.1f ns/hit, no repeated texture requests or matching\n",((end.tv_sec-begin.tv_sec)*1e9+end.tv_nsec-begin.tv_nsec)/2000000.0);
 assert(selosVistaCap<=2000);selosVistaLimpar();assert(!selosVista&&!selosVistaCap);
 puts("PASS: uniform pack for ready/delayed rows, cold textures, bounded prefetch, 1000-row publication, failures and view cache");
}
'''
fixture = fixture.replace("@STATE@", function("static int selosPacoteEstado("))
job_type = next(line for line in source.splitlines() if line.endswith('} SelosLote;'))
fixture = fixture.replace("@JOB_TYPE@", job_type)
fixture = fixture.replace("@WORKER@", function("static void *selosFio("))
fixture = fixture.replace("@NO_WORK@", function("static void selosSemTrabalho("))
fixture = fixture.replace("@IMAGES@", function("static int selosImagensProntas("))
view_start=source.index("enum { SELOS_VISTA_PENDENTE")
view_end=source.index("static float desenharSelosPacote(",view_start)
fixture=fixture.replace("@VIEW@",source[view_start:view_end])
# Both rows that draw badges: From addon (addonSelos) and From Nuvio (the
# block in corpoFolha). The same scenario runs once per row.
fixture = fixture.replace("@DRAW@", function("static void addonSelos("))
start = source.index("      int selosView = selosVistaDa(")
end = source.index("      if(ehMp4){ txt_desenhar_alpha(mp,", start)
fixture = fixture.replace("@NUVIO@", source[start:end])
with tempfile.TemporaryDirectory(prefix="nuvio-badge-ready-") as directory:
    temp = Path(directory)
    (temp / "test.c").write_text(fixture)
    flags = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-pthread"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(temp / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-I" + str(root / "src"),
                    str(temp / "test.c"), "-o", binary], check=True)
    subprocess.run([binary], check=True)
    subprocess.run([binary, "--nuvio-row"], check=True)

    # Exercise the production scheduler too: it must not read/copy a batch on
    # the UI thread. Model reallocations between row snapshot and publication.
    scheduler = r'''
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
 char url[8192],rotulo[192],descricao[2048],arquivo[1024];
 unsigned short selosPacote[16];unsigned char nSelosPacote;
 unsigned selosPacoteVer;
} Stream;
static Stream *lista;
static int n=2000,calculations,mode,failAllocation,failThread,creates;
static unsigned listaGeracao=1,version=7;
static size_t allocation;
static pthread_mutex_t verTrava=PTHREAD_MUTEX_INITIALIZER;
static unsigned selospacote_versao(void){return version;}
static int selospacote_ativo(void){return 0;}
static int selospacote_embutidos_ok(void){return 1;}
static void selospacote_colorido(int color){(void)color;}
static int ajustes_selos_coloridos(void){return 0;}
static void *(*queued)(void *);
static void *argument;
static void *recordAllocation(size_t bytes){allocation=bytes;return failAllocation?NULL:malloc(bytes);}
static int queueThread(pthread_t *thread,const pthread_attr_t *attr,void *(*fn)(void *),void *arg){
 (void)thread;(void)attr;creates++;if(failThread)return 1;assert(!argument);queued=fn;argument=arg;return 0;
}
static void selosCalcular(Stream *s){
 int row=atoi(s->rotulo);assert(row>=0&&row<n);
 assert(s->arquivo[0]=='f'&&s->descricao[0]=='d'&&s->url[0]=='u');
 s->selosPacote[0]=(unsigned short)row;s->nSelosPacote=1;s->selosPacoteVer=version;calculations++;
 if(mode==1&&calculations==2){
  Stream *replacement=calloc((size_t)n+3,sizeof *replacement);assert(replacement);
  memcpy(replacement,lista,(size_t)n*sizeof *replacement);free(lista);lista=replacement;n+=3;
 }
 if(mode==2)listaGeracao++;
 if(mode==3){version++;lista[row].selosPacoteVer=version;lista[row].nSelosPacote=3;}
}
@JOB_TYPE@
static unsigned selosPedVer,selosPedGer;
@WORKER@
@NO_WORK@
#define malloc recordAllocation
#define pthread_create queueThread
@SCHEDULER@
#undef malloc
#undef pthread_create
static void run(void){assert(argument);void *arg=argument;argument=NULL;queued(arg);}
int main(void){
 // NULL list deliberately detects any renderer-thread bulk copy.
 selosAgendar(0,n);assert(argument&&allocation==sizeof(SelosLote));
 assert(allocation<2*sizeof(Stream));
 lista=calloc((size_t)n,sizeof *lista);assert(lista);
 for(int i=0;i<n;i++){
  snprintf(lista[i].rotulo,sizeof lista[i].rotulo,"%d",i);
  strcpy(lista[i].arquivo,"file.mkv");strcpy(lista[i].descricao,"description");strcpy(lista[i].url,"url");
 }
 mode=1;run();assert(calculations==2000&&n==2003);
 for(int i=0;i<2000;i++)assert(lista[i].selosPacoteVer==version&&lista[i].selosPacote[0]==i);
 assert(!lista[2000].selosPacoteVer); // appended rows belong to their own job
 mode=0;calculations=0;selosAgendar(17,128);assert(allocation==sizeof(SelosLote));run();assert(calculations==128);
 calculations=0;selosAgendar(0,2000);listaGeracao++;run();assert(!calculations);
 selosAgendar(0,2000);version++;run();assert(!calculations);
 mode=2;selosAgendar(0,2000);run();assert(calculations==1);
 mode=3;calculations=0;selosAgendar(0,2000);run();assert(calculations==1&&lista[0].nSelosPacote==3);
 failThread=1;selosAgendar(17,128);assert(!argument);
 for(int i=17;i<145;i++)assert(lista[i].selosPacoteVer==version&&!lista[i].nSelosPacote);
 failThread=0;failAllocation=1;version++;selosAgendar(17,128);assert(!argument);
 for(int i=17;i<145;i++)assert(lista[i].selosPacoteVer==version&&!lista[i].nSelosPacote);
 int before=creates;
 selosAgendar(-1,2);selosAgendar(0,0);selosAgendar(INT_MAX,2);selosAgendar(1,INT_MAX);
 assert(creates==before);free(lista);
 puts("PASS: constant one-row badge job, no UI batch copy, 2000-row publication across realloc, bounded ranges, generation/pack cancellation and allocation/thread failure");
}
'''
    scheduler = scheduler.replace('@JOB_TYPE@', job_type)
    scheduler = scheduler.replace('@WORKER@', function('static void *selosFio('))
    scheduler = scheduler.replace('@NO_WORK@', function('static void selosSemTrabalho('))
    # Skip the forward declaration near list publication.
    offset = source.index('static void selosAgendar(int ini, int qtd) {')
    saved = source
    source = source[offset:]
    scheduler = scheduler.replace('@SCHEDULER@', function('static void selosAgendar('))
    source = saved
    (temp / 'test.c').write_text(scheduler)
    subprocess.run([os.environ.get('CC', 'cc'), *flags, str(temp / 'test.c'), '-o', binary], check=True)
    subprocess.run([binary], check=True)
