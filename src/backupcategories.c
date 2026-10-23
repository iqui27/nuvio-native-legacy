#define _POSIX_C_SOURCE 200809L
#include "backupcategories.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define BC_GRUPOS 32
#define BC_MAPAS 33
#define BC_HASHES 4096
#define BC_BYTES (4u*1024u*1024u)
typedef struct {BackupCategoriaMapa mapas[BC_MAPAS];int nm,nh,mostrar;uint64_t *hashes;} Grupo;
struct BackupCategorias {Grupo grupos[BC_GRUPOS];int n;};
uint64_t backupcategorias_chave(int tipo,unsigned id,const char *base,const char *nome) {
  char origem[48];snprintf(origem,sizeof origem,"%d:%u:",tipo,id);
  const char *partes[]={origem,tipo?"":base,nome};uint64_t h=1469598103934665603ULL;
  for(int i=0;i<3;i++){for(const unsigned char *p=(const unsigned char *)partes[i];*p;p++)h=(h^*p)*1099511628211ULL;h=h*1099511628211ULL;}
  return h?h:1;
}
void backupcategorias_liberar(BackupCategorias *b){if(b){for(int i=0;i<b->n;i++)free(b->grupos[i].hashes);free(b);}}
static int comparar(const void *a,const void *b){uint64_t x=*(const uint64_t *)a,y=*(const uint64_t *)b;return (x>y)-(x<y);}
static int hashLer(const char *s,uint64_t *out) {
  if(strlen(s)!=16)return 0;
  uint64_t n=0;
  for(int i=0;i<16;i++) {
    unsigned char c=(unsigned char)s[i];int v=c>='0' && c<='9'?c-'0':c>='a' && c<='f'?c-'a'+10:-1;
    if(v<0)return 0;
    n=(n<<4)|(unsigned)v;
  }
  *out=n;return n!=0;
}
static int hashAdicionar(Grupo *g,uint64_t h) {
  for(int i=0;i<g->nh;i++)if(g->hashes[i]==h)return 1;
  if(g->nh>=BC_HASHES)return 0;
  uint64_t *v=realloc(g->hashes,(size_t)(g->nh+1)*sizeof *v);if(!v)return 0;
  g->hashes=v;g->hashes[g->nh++]=h;return 1;
}
static int mapaNumero(const char *s,unsigned max,unsigned *out) {
  unsigned v=0;if(!s || !*s)return 0;
  for(;*s;s++){unsigned d=(unsigned char)*s-'0';if(d>9 || d>max || v>(max-d)/10u)return 0;v=v*10u+d;}
  *out=v;return 1;
}
int backupcategorias_ler(const char *s,BackupCategorias **saida) {
  if(saida)*saida=NULL;
  if(!s || !saida || strnlen(s,BC_BYTES+1u)>BC_BYTES)return 0;
  BackupCategorias *b=calloc(1,sizeof *b);if(!b)return 0;
  if(!*s){*saida=b;return 1;}
  char *copia=strdup(s);if(!copia){free(b);return 0;}
  int ok=1;Grupo *g=NULL;
  for(char *p=copia;*p && ok;) {
    char *e=strchr(p,'\n');if(e)*e++=0;else e=p+strlen(p);
    if(!strcmp(p,"group") || !strcmp(p,"group show")) {
      if(b->n>=BC_GRUPOS){ok=0;break;}g=&b->grupos[b->n++];g->mostrar=!strcmp(p,"group show");
    } else if(!strncmp(p,"map ",4)) {
      int usado=0;unsigned tipo,a,d;char t[12],antes[12],depois[12];
      ok=g && g->nm<BC_MAPAS && sscanf(p,"map %11s %11s %11s%n",t,antes,depois,&usado)==3 && usado && !p[usado] &&
        mapaNumero(t,3,&tipo) && mapaNumero(antes,UINT_MAX-1u,&a) && mapaNumero(depois,UINT_MAX-1u,&d) && (tipo || (!a && !d));
      if(ok)g->mapas[g->nm++]=(BackupCategoriaMapa){(int)tipo,a,d};
    } else if(!strncmp(p,"hash ",5)) {
      uint64_t h;ok=g && hashLer(p+5,&h) && hashAdicionar(g,h);
    } else ok=!*p;
    p=e;
  }
  for(int i=0;i<b->n && ok;i++) {
    Grupo *v=&b->grupos[i];ok=v->nm>0 && v->nh>0;
    if(ok)qsort(v->hashes,(size_t)v->nh,sizeof *v->hashes,comparar);
  }
  free(copia);if(!ok){backupcategorias_liberar(b);return 0;}*saida=b;return 1;
}
int backupcategorias_oculta(const BackupCategorias *b,int tipo,unsigned id,const char *base,const char *nome) {
  if(!b || !base || !nome)return 0;
  for(int i=b->n-1;i>=0;i--) {
    const Grupo *g=&b->grupos[i];
    for(int j=0;j<g->nm;j++)if(g->mapas[j].tipo==tipo && g->mapas[j].depois==id) {
      uint64_t h=backupcategorias_chave(tipo,g->mapas[j].antes,base,nome);
      if(bsearch(&h,g->hashes,(size_t)g->nh,sizeof *g->hashes,comparar))return g->mostrar?-1:1;
    }
  }
  return 0;
}
static int serializar(const BackupCategorias *b,char **saida) {
  char *out=malloc(BC_BYTES+1u);if(!out)return 0;
  size_t n=0;
  for(int i=0;i<b->n;i++) {
    const Grupo *g=&b->grupos[i];if(!g->nm || !g->nh)continue;
    int k=snprintf(out+n,BC_BYTES+1u-n,"%s\n",g->mostrar?"group show":"group");n+=(size_t)k;
    for(int j=0;j<g->nm;j++) {
      k=snprintf(out+n,BC_BYTES+1u-n,"map %d %u %u\n",g->mapas[j].tipo,g->mapas[j].antes,g->mapas[j].depois);
      if(k<0 || (size_t)k>=BC_BYTES+1u-n){free(out);return 0;}n+=(size_t)k;
    }
    for(int j=0;j<g->nh;j++) {
      k=snprintf(out+n,BC_BYTES+1u-n,"hash %016llx\n",(unsigned long long)g->hashes[j]);
      if(k<0 || (size_t)k>=BC_BYTES+1u-n){free(out);return 0;}n+=(size_t)k;
    }
  }
  out[n]=0;*saida=out;return 1;
}
static int grupoAdicionar(BackupCategorias *b,const Grupo *g) {
  if(!g->nm || !g->nh)return 1;
  for(int i=0;i<b->n;i++) {
    const Grupo *d=&b->grupos[i];
    if(d->mostrar==g->mostrar && d->nm==g->nm && d->nh==g->nh && !memcmp(d->mapas,g->mapas,(size_t)g->nm*sizeof *g->mapas) &&
       !memcmp(d->hashes,g->hashes,(size_t)g->nh*sizeof *g->hashes)) {
      free(b->grupos[i].hashes);memmove(&b->grupos[i],&b->grupos[i+1],(size_t)(b->n-i-1)*sizeof b->grupos[0]);b->n--;break;
    }
  }
  if(b->n>=BC_GRUPOS)return 0;
  Grupo *d=&b->grupos[b->n];*d=*g;d->hashes=malloc((size_t)g->nh*sizeof *d->hashes);if(!d->hashes)return 0;
  memcpy(d->hashes,g->hashes,(size_t)g->nh*sizeof *d->hashes);b->n++;return 1;
}
static int adicionarPreferencias(BackupCategorias *d,const char *texto,const BackupCategoriaMapa *mapas,int n) {
  if(!texto || strlen(texto)>BC_HASHES*39u || n<0 || n>32 || (n && !mapas))return 0;
  Grupo g[2];memset(g,0,sizeof g);int ok=1;
  for(int j=0;j<2;j++) {
    g[j].mostrar=j;g[j].mapas[g[j].nm++]=(BackupCategoriaMapa){0,0,0};
    for(int i=0;i<n;i++)g[j].mapas[g[j].nm++]=mapas[i];
  }
  for(const char *p=texto;*p && ok;) {
    const char *e=strchr(p,'\n');if(!e)e=p+strlen(p);
    int mostrar=e-p==21 && !memcmp(p,"show ",5);
    const char *q=p+(mostrar?5:0);char s[17];uint64_t h;
    if(e-q!=16){ok=0;break;}memcpy(s,q,16);s[16]=0;
    ok=hashLer(s,&h) && hashAdicionar(&g[mostrar],h);p=*e?e+1:e;
  }
  for(int i=0;i<2 && ok;i++)if(g[i].nh) {
    qsort(g[i].hashes,(size_t)g[i].nh,sizeof *g[i].hashes,comparar);ok=grupoAdicionar(d,&g[i]);
  }
  free(g[0].hashes);free(g[1].hashes);return ok;
}
int backupcategorias_mesclar(const char *destino,const char *destinoOcultas,const BackupCategoriaMapa *destinoMapas,int nd,
                            const char *origem,const char *ocultas,const BackupCategoriaMapa *mapas,int n,char **saida) {
  if(saida)*saida=NULL;
  if(!saida || n<0 || n>32 || (n && !mapas))return 0;
  BackupCategorias *d=NULL,*o=NULL;int ok=backupcategorias_ler(destino,&d) && backupcategorias_ler(origem,&o);
  if(!ok)goto fim;
  ok=adicionarPreferencias(d,destinoOcultas,destinoMapas,nd);
  for(int i=0;i<o->n && ok;i++) {
    Grupo g=o->grupos[i];g.nm=0;
    for(int j=0;j<o->grupos[i].nm;j++) {
      BackupCategoriaMapa m=o->grupos[i].mapas[j];
      if(!m.tipo){g.mapas[g.nm++]=m;continue;}
      for(int k=0;k<n;k++)if(mapas[k].tipo==m.tipo && mapas[k].antes==m.depois){m.depois=mapas[k].depois;g.mapas[g.nm++]=m;break;}
    }
    ok=grupoAdicionar(d,&g);
  }
  if(ok)ok=adicionarPreferencias(d,ocultas,mapas,n);
  if(ok)ok=serializar(d,saida);
fim:
  backupcategorias_liberar(d);backupcategorias_liberar(o);return ok;
}
