#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import subprocess, tempfile
home=Path('src/home.c').read_text(); desc=Path('src/descoberta.c').read_text(); listas=Path('src/listas.c').read_text()
def fn(s,name):
    at=s.index(name+'('); start=s.rfind('\n',0,at)+1; brace=s.index('{',at); end=brace+1; depth=1
    while depth:
        depth+=(s[end]=='{')-(s[end]=='}'); end+=1
    return s[start:end]
assert 'desc_vertudo_n_bruto()' in fn(listas,'lst_itens_n')
assert 'desc_vertudo_item_bruto(i, dst)' in fn(listas,'lst_item')
assert 'fileiraItemIndice(f, coluna + k)' in fn(home,'desenhaPilha')
assert 'it->ranking' in Path('src/vertudo.c').read_text()
assert 'HOME_VISTOS_MAX (DESC_ITENS_POR_FILEIRA + 6)' in desc
code='''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define MAX_CARDS 33
#define DESC_ITENS_POR_FILEIRA 24
#define HOME_VISTOS_MAX 30
#define NV_ADDON_PEDIDO_MAX 512
#define VT_VISTOS_PAGINAS_MAX 3
typedef struct { char imdb[64],tipo[8],origem[96]; int watched,ranking; } CatItem;
#define FILEIRA_SOCIAL 9
typedef struct { int n,ini,usaItens,itens[33],verTudo,stackN,tipo; } Fileira;
typedef struct { int n,ini; char base[8],catId[8]; } CatFileira;
static CatItem items[48];
int ajustes_ocultar_filmes_vistos(void){return 1;}
int cat_visto(const CatItem *it){return it->watched;}
const CatItem *cat_item(int i){return &items[i];}
const CatItem *cat_item_exato(int i){return i>=0?&items[i]:NULL;}
int amigosfil_indice_cat(int c){return c;}
'''+fn(home,'fileiraItemIndice')+'\n'+fn(home,'fileiraRanking')+'\n'+fn(home,'filmeVistoOculto')+'\n'+fn(home,'filtrarFilmesVistos')+'''
static int vtFim,vtErro,projected;
int vtProjetar(void){return projected;}
'''+fn(desc,'vtPaginaSoDeVistos')+'''
static int calls;
int lerCatalogo(const char*a,const char*b,const char*c,CatItem*d,int e,int f,int*g){return 0;}
int addons_base_desligada(const char*a){return 0;}
void origemDaBase(const char*a,char*b,int n){snprintf(b,n,"addon");}
#include <stdarg.h>
int nv_addon_url(char*out,int n,const char*base,const char*fmt,...){va_list a;va_start(a,fmt);int w=vsnprintf(out,n,fmt,a);va_end(a);return w;}
int descPedidoCoube(const char*a,int w,int n){return w<n;}
char *rede_baixar(const char *url,int timeout){
  if(calls++) { assert(strstr(url,"skip=26")); return strdup("AB"); }
  return strdup("abcdefghijklmnopqrstuvwxAA");
}
const char *js_array(const char*s,const char*f,const char*k){return *s?s:NULL;}
const char *js_fim(const char*p){return p+1;}
const char *js_prox(const char*p){return *p?p:NULL;}
int deMeta(const char*p,const char*f,const char*t,CatItem*out){memset(out,0,sizeof *out);out->imdb[0]=*p;strcpy(out->tipo,t);out->watched=*p>='a';return 1;}
int descOcultaNaoLancado(const CatItem*i){return 0;}
int desc_filme_visto(const CatItem*i){return i->watched;}
static int prefix;
int primeira(const CatItem*i,int n,void*u){prefix=n;return 1;}
'''+fn(desc,'lerCatalogoHomeParcial')+'''
int main(void){
 Fileira f={.n=24,.verTudo=1};CatFileira cf={.n=48,.base="addon",.catId="top10"};
 for(int i=0;i<48;i++){strcpy(items[i].tipo,"movie");items[i].watched=1;}
 filtrarFilmesVistos(&f,&cf);assert(f.n==0 && f.verTudo==0);
 items[4].watched=0;items[7].watched=0;f.n=24;filtrarFilmesVistos(&f,&cf);assert(f.n==2 && f.itens[0]==4);
 f.stackN=f.n;f.n=1;assert(fileiraItemIndice(&f,1)==7);assert(fileiraRanking(&f,1)==8);
 items[7].ranking=28;assert(fileiraRanking(&f,1)==28);
 for(int p=1;p<=3;p++)assert(vtPaginaSoDeVistos(0,24,p)==(p<3));
 vtFim=1;assert(!vtPaginaSoDeVistos(0,24,1));vtFim=0;vtErro=1;assert(!vtPaginaSoDeVistos(0,24,1));vtErro=0;
 assert(!vtPaginaSoDeVistos(0,0,1));projected=1;assert(!vtPaginaSoDeVistos(0,24,1));
 CatItem out[30];int respondeu;int n=lerCatalogoHomeParcial("addon","movie","top10",out,30,3,1,&respondeu,primeira,NULL);
 assert(respondeu && calls==2 && prefix==25 && n==26);assert(out[24].imdb[0]=='A' && out[25].imdb[0]=='B');
 assert(out[25].ranking==28);
 puts("vistos_regressoes: PASS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(code);exe=Path(d)/'test'
 subprocess.run(['cc',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
PY
