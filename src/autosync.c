#include "autosync.h"
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* MOTOR DE ALINHAMENTO. As ideias vem do AutoSync do Nuvio Reshaped
 * (NuvioMedia/NuvioTV PR 3703, GPL-3.0 como este projeto); o codigo e
 * proprio. Em ordem:
 *  1. Falas de dialogo das duas legendas (sem letreiro, SDH puro, musica).
 *  2. Correlacao de atividade em bits de 500 ms para cada escala candidata
 *     (as razoes exatas de quadros por segundo e a razao dos spans), offset
 *     ate +-180 s (a referencia inteira quando a externa cobre menos de 75%
 *     dela: CD1/CD2); depois 100 ms em volta do vencedor.
 *  3. Programacao dinamica em banda (+-28 falas) casando grupos 1:1, 1:2,
 *     2:1, 1:3, 3:1 e 2:2 contra a transformacao semente; minimos quadrados
 *     nos grupos casados refinam escala e offset.
 *  4. Sem cobertura: offsets proprios para as sequencias longas sem par
 *     (corte, cena a mais), Viterbi rotula cada fala com um trecho e cada
 *     trecho e alinhado de novo. Ancora de linha so onde a vizinhanca concorda.
 *  5. Portoes: score, margem, cobertura, falas casadas, custo medio, buraco
 *     maximo e coerencia por terco. Qualquer falha recusa: a legenda fica
 *     como estava. Transformacao que nao e offset puro tem portao mais alto.
 * Puro: sem SDL/GL/rede; cancelavel e com teto de tempo. */
#define AS_MAX_CUES 8000
#define AS_EXCLUIDAS 16
#define AS_MAX_SEG (8 * 3600.0)
#define AS_GROSSO .5
#define AS_FINO .1
#define AS_RAIO_FINO 10      /* bins finos em volta do vencedor grosso */
#define AS_PARCIAL .75
#define AS_UNIDADE .1         /* s: escala cuja deriva no filme todo e menor que isto e 1 */
#define AS_DISTINTO 3.0      /* segundos: dois picos sao transformacoes diferentes */
#define AS_EMPATE .002
#define AS_BANDA 28
#define AS_PULA_REF 1.35
#define AS_PULA_ALVO 1.85
#define AS_GRUPO_EXTRA .10
#define AS_GRUPO_MAX 8.0
#define AS_MIN_ATIVIDADE .55 /* pico grosso; o score final e AS_MIN_SCORE */
#define AS_MIN_SCORE .78
#define AS_MIN_MARGEM .02
#define AS_MIN_COBERTURA .84
#define AS_MIN_COBERTURA_D .90
#define AS_MIN_CASADAS 20
#define AS_MAX_CUSTO 2.35
#define AS_MAX_CUSTO_D 1.10
#define AS_SEG_COBERTURA .72
#define AS_SEG_CUSTO 1.35
#define AS_MAX_TRECHOS 4
#define AS_TROCA 10.0        /* Viterbi: mudar de trecho custa ~5 falas sem par */
#define AS_MIN_TRECHO_SEG 60.0

typedef struct {
  AutoSyncCancelar cancelar; void *usuario;
  struct timespec inicio; int orcamento, terminou;
} Controle;
static int tempoMs(const struct timespec *inicio) {
  struct timespec agora;clock_gettime(CLOCK_MONOTONIC,&agora);
  double ms=(agora.tv_sec-inicio->tv_sec)*1000.0+(agora.tv_nsec-inicio->tv_nsec)/1000000.0;
  return ms>INT_MAX?INT_MAX:(int)ms;
}
static int parou(Controle *c) {
  if(c->terminou)return 1;
  if(c->cancelar&&c->cancelar(c->usuario)){c->terminou=AUTOSYNC_SESSION_CHANGED;return 1;}
  if(tempoMs(&c->inicio)>=c->orcamento){c->terminou=AUTOSYNC_BUDGET;return 1;}
  return 0;
}
AutoSyncConfig autosync_config(AutoSyncModo modo) {
  return (AutoSyncConfig){modo,250,180000,modo==AUTOSYNC_THOROUGH?16000:4000,100};
}
const char *autosync_motivo(AutoSyncMotivo m) {
  static const char *const nomes[]={"accepted","no_reference","incomplete_document",
    "forced_or_signs","sparse_dialogue","repeated_dialogue","low_confidence",
    "ambiguous_peak","region_disagreement","session_changed","analysis_budget",
    "out_of_memory","excluded_reference","invalid_argument"};
  return m>=0&&m<(int)(sizeof nomes/sizeof *nomes)?nomes[m]:"invalid_argument";
}

/* --- falas --------------------------------------------------------------- */
typedef struct { double ini, fim; } Fala;
typedef struct { Fala *v; int n; double ini, fim; } Falas;
static int cmpHash(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return x<y?-1:x>y;}
static int cmpFala(const void *a,const void *b) {
  const Fala *x=a,*y=b;
  if(x->ini!=y->ini)return x->ini<y->ini?-1:1;
  return x->fim<y->fim?-1:x->fim>y->fim;
}
static uint64_t textoHash(const char *s,int *letras) {
  uint64_t h=UINT64_C(14695981039346656037);int n=0;
  for(;*s;s++) {unsigned char c=(unsigned char)*s;
    if(c>=128||isalnum(c)){h^=c<128?(unsigned char)tolower(c):c;h*=UINT64_C(1099511628211);n++;}
  }
  *letras=n;return h;
}
static int sinal(const LegendaCue *c) {
  const char *s=c->texto;while(isspace((unsigned char)*s))s++;
  /* Upper/positioned ASS signage and pure SDH/music are not dialogue. */
  if(c->an>=4||(c->posY>=0&&c->resY>0&&c->posY<c->resY*.65f))return 1;
  if(strstr(s,"\xe2\x99\xaa")||strstr(s,"\xe2\x99\xab"))return 1;
  size_t n=strlen(s);
  return n>1&&((s[0]=='['&&s[n-1]==']')||(s[0]=='('&&s[n-1]==')'));
}
static AutoSyncMotivo preparar(const LegendaDocumento *doc,Falas *f,Controle *c) {
  const LegendaDocumentoInfo *info=legenda_documento_info(doc);
  int n=0,i,usados=0,sinais=0;const LegendaCue *v=legenda_documento_dados(doc,&n);
  uint64_t *hashes=NULL;
  if(!info||!v)return AUTOSYNC_NO_REFERENCE;
  if(!(info->flags&LEGENDA_DOC_COMPLETO)||n>=AS_MAX_CUES)return AUTOSYNC_INCOMPLETE;
  if(info->flags&(LEGENDA_DOC_FORCED|LEGENDA_DOC_SINAIS))return AUTOSYNC_FORCED_SIGNS;
  f->v=malloc((size_t)n*sizeof *f->v);hashes=malloc((size_t)n*sizeof *hashes);
  if(!f->v||!hashes){free(hashes);return AUTOSYNC_MEMORY;}
  for(i=0;i<n;i++) {
    if(!(i%256)&&parou(c)){free(hashes);return (AutoSyncMotivo)c->terminou;}
    int letras=0;double dur=v[i].fim-v[i].inicio;
    if(sinal(&v[i])){sinais++;continue;}
    uint64_t h=textoHash(v[i].texto,&letras);
    if(letras<3||dur<.20||dur>15||v[i].inicio<0||v[i].fim>AS_MAX_SEG)continue;
    hashes[usados++]=h;f->v[f->n++]=(Fala){v[i].inicio,v[i].fim};
  }
  if(sinais*5>n){free(hashes);return AUTOSYNC_FORCED_SIGNS;}
  if(usados<AS_MIN_CASADAS){free(hashes);return AUTOSYNC_SPARSE;}
  qsort(hashes,(size_t)usados,sizeof *hashes,cmpHash);
  int maior=1,iguais=1,distintos=1;
  for(i=1;i<usados;i++) {
    if(hashes[i]==hashes[i-1]){if(++iguais>maior)maior=iguais;}
    else {iguais=1;distintos++;}
  }
  free(hashes);
  if(maior*4>usados||distintos*2<usados)return AUTOSYNC_REPEATED;
  qsort(f->v,(size_t)f->n,sizeof *f->v,cmpFala);
  /* Camadas ASS duplicadas (contorno + texto) sao uma fala so. */
  int k=0;
  for(i=0;i<f->n;i++) {
    if(k&&fabs(f->v[i].ini-f->v[k-1].ini)<.01&&fabs(f->v[i].fim-f->v[k-1].fim)<.01)continue;
    f->v[k++]=f->v[i];
  }
  f->n=k;f->ini=f->v[0].ini;f->fim=0;
  for(i=0;i<f->n;i++)if(f->v[i].fim>f->fim)f->fim=f->v[i].fim;
  if(f->n<AS_MIN_CASADAS||f->fim-f->ini<60)return AUTOSYNC_SPARSE;
  return AUTOSYNC_OK;
}
static int primeiraApos(const Falas *f,double t) {
  int lo=0,hi=f->n;
  while(lo<hi){int m=(lo+hi)/2;if(f->v[m].ini<t)lo=m+1;else hi=m;}
  return lo;
}

/* --- atividade em bits -----------------------------------------------------
 * Bin ligado = alguma fala no ar. Um offset e AND + popcount palavra a
 * palavra: o filme inteiro a 500 ms sao ~230 palavras de 64 bits. */
typedef struct { uint64_t *w; int *pref; int nb, nw, ativos, primeiro, ultimo; } Bits;
static void bitsLiberar(Bits *b){free(b->w);free(b->pref);memset(b,0,sizeof *b);}
static int bitsMontar(Bits *b,const Fala *v,int n,double p,double q,double bin) {
  double fim=0;memset(b,0,sizeof *b);
  for(int i=0;i<n;i++)if(p*v[i].fim+q>fim)fim=p*v[i].fim+q;
  b->nb=(int)ceil(fim/bin)+2;if(b->nb<64)b->nb=64;b->nw=(b->nb+63)/64;
  b->w=calloc((size_t)b->nw,sizeof *b->w);b->pref=malloc(((size_t)b->nb+1)*sizeof *b->pref);
  if(!b->w||!b->pref){bitsLiberar(b);return 0;}
  for(int i=0;i<n;i++) {
    int a=(int)floor((p*v[i].ini+q)/bin),z=(int)ceil((p*v[i].fim+q)/bin);
    if(a<0)a=0;
    if(z>b->nb)z=b->nb;
    for(int k=a;k<z;k++)b->w[k>>6]|=UINT64_C(1)<<(k&63);
  }
  b->primeiro=-1;b->pref[0]=0;
  for(int k=0;k<b->nb;k++) {
    int on=(int)((b->w[k>>6]>>(k&63))&1);
    b->pref[k+1]=b->pref[k]+on;
    if(on){if(b->primeiro<0)b->primeiro=k;b->ultimo=k;}
  }
  b->ativos=b->pref[b->nb];
  return 1;
}
/* Bins i do alvo com a[i] e b[i+k]. */
static long cruzar(const Bits *a,const Bits *b,int k) {
  long n=0;
  if(a->primeiro<0)return 0;
  for(int w=a->primeiro>>6;w<=a->ultimo>>6;w++) {
    uint64_t x=a->w[w];if(!x)continue;
    int d=w*64+k,bw=d>=0?d>>6:-((-d+63)>>6),r=d-bw*64;
    if(bw>=0&&bw<b->nw)n+=__builtin_popcountll((x<<r)&b->w[bw]);
    if(r&&bw+1>=0&&bw+1<b->nw)n+=__builtin_popcountll((x>>(64-r))&b->w[bw+1]);
  }
  return n;
}
/* Precisao pesa mais que revocacao: a referencia pode ter SDH/letreiros que
 * a externa nao tem, nunca o contrario de forma sistematica. */
static double pontuar(const Bits *a,const Bits *b,int k) {
  int lo=-k>0?-k:0,hi=a->nb<b->nb-k?a->nb:b->nb-k;
  if(hi<=lo||a->ativos<=0)return 0;
  int vis=a->pref[hi]-a->pref[lo];if(vis<=0)return 0;
  int rlo=a->primeiro+k,rhi=a->ultimo+k+1;
  if(rlo<0)rlo=0;
  if(rhi>b->nb)rhi=b->nb;
  if(rhi<=rlo)return 0;
  int rw=b->pref[rhi]-b->pref[rlo];if(rw<=0)return 0;
  double I=(double)cruzar(a,b,k),prec=I/vis,rec=I/rw,visto=(double)vis/a->ativos;
  return (prec*.72+rec*.28)*(.85+.15*fmin(1,visto));
}

/* --- busca da transformacao ------------------------------------------------ */
typedef struct { double p, q; } Transf;   /* video = p*legenda + q */
typedef struct { double p, q, score, alternativo; } Pico;
static double aplicar(Transf t,double s){return t.p*s+t.q;}
static int distintos(double p1,double q1,double p2,double q2,double ini,double fim) {
  return fmax(fabs(p1*ini+q1-(p2*ini+q2)),fabs(p1*fim+q1-(p2*fim+q2)))>=AS_DISTINTO;
}
/* Grosso em todas as escalas, margem contra o melhor pico DISTINTO de
 * qualquer escala, e fino em volta do vencedor. `v/n` e o alvo (pode ser
 * so um trecho dele), qc/raio limitam o offset, escalas[] as candidatas. */
static Pico buscar(const Fala *v,int n,const Bits *rg,const Bits *rf,const double *escalas,int ne,
                   double qc,double raio,int parcial,Controle *c) {
  Pico pk={1,0,0,0};
  double ini=v[0].ini,fim=0;for(int i=0;i<n;i++)if(v[i].fim>fim)fim=v[i].fim;
  float *sc[12]={0};int kmin[12],kmax[12],ok=1;
  double melhor=-1;int me=0,mk=0;
  for(int e=0;e<ne&&ok;e++) {
    Bits a;if(!bitsMontar(&a,v,n,escalas[e],0,AS_GROSSO)){c->terminou=AUTOSYNC_MEMORY;ok=0;break;}
    int R=(int)ceil(raio/AS_GROSSO),k0=(int)lround(qc/AS_GROSSO);
    kmin[e]=k0-R;kmax[e]=k0+R;
    if(parcial) {   /* CD1/CD2: qualquer lugar dentro da referencia */
      int a0=(int)floor((rg->primeiro*AS_GROSSO-escalas[e]*ini)/AS_GROSSO)-R;
      int a1=(int)ceil((rg->ultimo*AS_GROSSO-escalas[e]*fim)/AS_GROSSO)+R;
      if(a0<kmin[e])kmin[e]=a0;
      if(a1>kmax[e])kmax[e]=a1;
    }
    sc[e]=malloc((size_t)(kmax[e]-kmin[e]+1)*sizeof *sc[e]);
    if(!sc[e]){c->terminou=AUTOSYNC_MEMORY;bitsLiberar(&a);ok=0;break;}
    for(int k=kmin[e];k<=kmax[e];k++) {
      if(!((k-kmin[e])%64)&&parou(c)){ok=0;break;}
      double s=pontuar(&a,rg,k);sc[e][k-kmin[e]]=(float)s;
      if(s>melhor){melhor=s;me=e;mk=k;}
    }
    bitsLiberar(&a);
  }
  if(ok&&melhor>0) {
    double p=escalas[me],q=mk*AS_GROSSO,alt=0;
    for(int e=0;e<ne;e++)for(int k=kmin[e];k<=kmax[e];k++)
      if(sc[e][k-kmin[e]]>alt&&distintos(p,q,escalas[e],k*AS_GROSSO,ini,fim))alt=sc[e][k-kmin[e]];
    pk=(Pico){p,q,melhor,alt};
    Bits a;
    if(bitsMontar(&a,v,n,p,0,AS_FINO)) {
      int k0=(int)lround(q/AS_FINO),bk=k0;double bs=-1;
      double fs[2*AS_RAIO_FINO+1];
      for(int d=-AS_RAIO_FINO;d<=AS_RAIO_FINO;d++){fs[d+AS_RAIO_FINO]=pontuar(&a,rf,k0+d);if(fs[d+AS_RAIO_FINO]>bs)bs=fs[d+AS_RAIO_FINO];}
      /* Empate e concordancia: vale o mais perto do grosso. */
      for(int d=0;d<=AS_RAIO_FINO;d++) {
        if(fs[AS_RAIO_FINO+d]>=bs-AS_EMPATE){bk=k0+d;break;}
        if(fs[AS_RAIO_FINO-d]>=bs-AS_EMPATE){bk=k0-d;break;}
      }
      pk.q=bk*AS_FINO;bitsLiberar(&a);
    } else c->terminou=AUTOSYNC_MEMORY;
  }
  for(int e=0;e<ne;e++)free(sc[e]);
  return pk;
}

/* --- programacao dinamica em banda ------------------------------------------ */
typedef struct { int i, na, j, nb; float custo; } Grupo;
static const signed char formas[6][2]={{1,1},{1,2},{2,1},{1,3},{3,1},{2,2}};
static double custoGrupo(const double *ts,const double *te,int i,int na,const Falas *b,int j,int nb) {
  double rs=b->v[j].ini,re=b->v[j].fim,as=ts[i],ae=te[i];
  for(int k=1;k<nb;k++)if(b->v[j+k].fim>re)re=b->v[j+k].fim;
  for(int k=1;k<na;k++)if(te[i+k]>ae)ae=te[i+k];
  double rd=fmax(.001,re-rs),ad=fmax(.001,ae-as);
  return .34*fabs(rs-as)/1.25+.34*fabs(re-ae)/1.25+.18*fabs((rs+re)/2-(as+ae)/2)/1.5+.14*fabs(rd-ad)/2.0;
}
/* Alvo [i0,i1) ja transformado em ts/te (indices absolutos) contra a
 * referencia inteira. Grupos em ordem, *ng deles. 0 = parou/sem memoria. */
static int alinharBanda(const double *ts,const double *te,int i0,int i1,const Falas *b,
                        Grupo *saida,int *ng,Controle *c) {
  int N=i1-i0,W=2*AS_BANDA+1,m=b->n;*ng=0;
  if(N<=0)return 1;
  int *cen=malloc(((size_t)N+1)*sizeof *cen);
  float *custo=malloc(((size_t)N+1)*W*sizeof *custo);
  unsigned char *passo=calloc(((size_t)N+1)*W,1);
  if(!cen||!custo||!passo){free(cen);free(custo);free(passo);c->terminou=AUTOSYNC_MEMORY;return 0;}
  for(int r=0;r<N;r++)cen[r]=primeiraApos(b,ts[i0+r]);
  cen[N]=primeiraApos(b,te[i1-1]);
  for(int x=0;x<(N+1)*W;x++)custo[x]=1e30f;
#define CEL(r,j) ((r)*W+(j)-cen[r]+AS_BANDA)
#define NABANDA(r,j) ((j)>=0&&(j)<=m&&abs((j)-cen[r])<=AS_BANDA)
#define RELAXAR(r2,j2,v,s) do{if(NABANDA(r2,j2)){int x_=CEL(r2,j2);if((v)<custo[x_]-1e-6f){custo[x_]=(float)(v);passo[x_]=(unsigned char)(s);}}}while(0)
  for(int j=cen[0]-AS_BANDA;j<=cen[0]+AS_BANDA;j++)if(NABANDA(0,j)){custo[CEL(0,j)]=0;passo[CEL(0,j)]=1;}
  for(int r=0;r<=N;r++) {
    if(!(r%64)&&parou(c)){free(cen);free(custo);free(passo);return 0;}
    for(int j=cen[r]-AS_BANDA;j<=cen[r]+AS_BANDA;j++) {
      if(!NABANDA(r,j))continue;
      int x=CEL(r,j);if(!passo[x])continue;
      double v=custo[x];
      if(j<m)RELAXAR(r,j+1,v+AS_PULA_REF,2);
      if(r<N)RELAXAR(r+1,j,v+AS_PULA_ALVO,3);
      for(int s=0;s<6;s++) {
        int na=formas[s][0],nb=formas[s][1];
        if(r+na>N||j+nb>m||!NABANDA(r+na,j+nb))continue;
        double g=custoGrupo(ts,te,i0+r,na,b,j,nb);
        if(g>AS_GRUPO_MAX)continue;
        RELAXAR(r+na,j+nb,v+g+AS_GRUPO_EXTRA*(na+nb-2),4+s);
      }
    }
  }
  int bj=-1;double bv=1e30;
  for(int j=cen[N]-AS_BANDA;j<=cen[N]+AS_BANDA;j++)
    if(NABANDA(N,j)&&passo[CEL(N,j)]&&custo[CEL(N,j)]<bv){bv=custo[CEL(N,j)];bj=j;}
  int r=N,j=bj,n=0;
  while(bj>=0&&r>=0) {
    int s=passo[CEL(r,j)];
    if(s<=1)break;
    if(s==2)j--;
    else if(s==3)r--;
    else {
      int na=formas[s-4][0],nb=formas[s-4][1];
      saida[n++]=(Grupo){i0+r-na,na,j-nb,nb,(float)custoGrupo(ts,te,i0+r-na,na,b,j-nb,nb)};
      r-=na;j-=nb;
    }
  }
#undef CEL
#undef NABANDA
#undef RELAXAR
  for(int k=0;k<n/2;k++){Grupo t=saida[k];saida[k]=saida[n-1-k];saida[n-1-k]=t;}
  *ng=n;free(cen);free(custo);free(passo);return 1;
}
static int cmpD(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
/* Minimos quadrados nos inicios dos grupos, descartando quem foge da
 * mediana. livre = escala tambem; senao so o offset. */
static void ajustar(const Falas *a,const Falas *b,const Grupo *g,int ng,int livre,Transf *t) {
  if(ng<10)return;
  double *x=malloc((size_t)ng*sizeof *x),*y=malloc((size_t)ng*sizeof *y),*e=malloc((size_t)ng*sizeof *e);
  if(!x||!y||!e){free(x);free(y);free(e);return;}
  for(int it=0;it<3;it++) {
    for(int k=0;k<ng;k++){x[k]=a->v[g[k].i].ini;y[k]=b->v[g[k].j].ini;e[k]=y[k]-aplicar(*t,x[k]);}
    qsort(e,(size_t)ng,sizeof *e,cmpD);
    double med=e[ng/2];
    for(int k=0;k<ng;k++)e[k]=fabs(e[k]-med);
    qsort(e,(size_t)ng,sizeof *e,cmpD);
    double lim=fmax(.3,3*1.4826*e[ng/2]);
    double sx=0,sy=0,sxx=0,sxy=0;int m=0;
    for(int k=0;k<ng;k++) {
      double r=y[k]-aplicar(*t,x[k]);
      if(fabs(r-med)>lim)continue;
      sx+=x[k];sy+=y[k];m++;
    }
    if(m<10)break;
    double xm=sx/m,ym=sy/m;
    for(int k=0;k<ng;k++) {
      double r=y[k]-aplicar(*t,x[k]);
      if(fabs(r-med)>lim)continue;
      sxx+=(x[k]-xm)*(x[k]-xm);sxy+=(x[k]-xm)*(y[k]-ym);
    }
    if(livre&&sxx>1) {
      double p=sxy/sxx;
      if(p>=.94&&p<=1.06){t->p=p;t->q=ym-p*xm;}
    } else t->q=ym-t->p*xm;
  }
  free(x);free(y);free(e);
}

/* --- trechos, avaliacao, mapa ------------------------------------------------ */
typedef struct { Transf t; int i0, i1; } Peca;
typedef struct { double t, r; } No;
struct AutoSyncMapa {
  int nt, nn;
  struct { double t0, s0, s1, p, q; int n0, nn; } t[AS_MAX_TRECHOS];
  No *nos;
};
double autosync_mapa_tempo(const AutoSyncMapa *m,double t) {
  if(!m||m->nt<=0)return t;
  int k=0;
  while(k+1<m->nt&&t>=m->t[k+1].t0)k++;
  double r=0;const No *v=m->nos+m->t[k].n0;int n=m->t[k].nn;
  if(n&&t>=v[0].t&&t<=v[n-1].t) {
    int lo=0,hi=n-1;
    while(hi-lo>1){int mid=(lo+hi)/2;if(v[mid].t<=t)lo=mid;else hi=mid;}
    double d=v[hi].t-v[lo].t;
    r=d>0?v[lo].r+(v[hi].r-v[lo].r)*(t-v[lo].t)/d:v[lo].r;
  }
  double s=(t-r-m->t[k].q)/m->t[k].p;
  /* Cada trecho so mostra a sua parte da legenda (s0 e a emenda NA
   * LEGENDA, s1 o fim dela, t0 a emenda NO VIDEO): cena que so o video
   * tem fica sem legenda; cena que so a legenda tem nunca aparece. */
  if(s<m->t[k].s0||s>m->t[k].s1)return AUTOSYNC_SEM_LEGENDA;
  return s;
}
void autosync_mapa_liberar(AutoSyncMapa *m){if(m){free(m->nos);free(m);}}

typedef struct {
  double cobertura, custo, erro;
  int casadas, tercos;
} Medida;
/* Cada fala do alvo com a transformacao do seu trecho. */
static void transformar(const Falas *a,const Peca *pc,int np,double *ts,double *te) {
  for(int k=0;k<np;k++)for(int i=pc[k].i0;i<pc[k].i1;i++){ts[i]=aplicar(pc[k].t,a->v[i].ini);te[i]=aplicar(pc[k].t,a->v[i].fim);}
}
/* Portoes estruturais sobre os grupos de todos os trechos. 0 = passou;
 * senao o motivo. nseg = 3 (rapida) ou 6 (completa). */
static AutoSyncMotivo medir(const Falas *a,const Falas *b,const Peca *pc,int np,const Grupo *g,int ng,
                            int nseg,int tolMs,int descoberta,Medida *md) {
  int n=a->n;char *ok=calloc((size_t)n,1);
  if(!ok)return AUTOSYNC_MEMORY;
  memset(md,0,sizeof *md);
  double soma=0;
  for(int k=0;k<ng;k++){for(int i=g[k].i;i<g[k].i+g[k].na;i++)ok[i]=1;soma+=g[k].custo;}
  for(int i=0;i<n;i++)md->casadas+=ok[i];
  md->cobertura=(double)md->casadas/n;md->custo=ng?soma/ng:99;
  /* Uma sequencia longa sem par sozinha nao recusa (letra de musica,
   * placa traduzida): os outros portoes e os segmentos decidem. */
  AutoSyncMotivo r=AUTOSYNC_OK;
  if(md->casadas<AS_MIN_CASADAS)r=AUTOSYNC_SPARSE;
  else if(md->cobertura<(descoberta?AS_MIN_COBERTURA_D:AS_MIN_COBERTURA))r=AUTOSYNC_LOW_CONFIDENCE;
  else if(md->custo>(descoberta?AS_MAX_CUSTO_D:AS_MAX_CUSTO))r=AUTOSYNC_LOW_CONFIDENCE;
  /* Coerencia por segmento (fatias iguais do alvo): cobertura, custo e o
   * residuo mediano contra a transformacao do trecho. */
  double *res=malloc((size_t)(ng+1)*sizeof *res);
  if(!res){free(ok);return AUTOSYNC_MEMORY;}
  for(int s=0;s<nseg;s++) {
    int x=s*n/nseg,y=(s+1)*n/nseg,c=0,nr=0;double cs=0;
    for(int i=x;i<y;i++)c+=ok[i];
    for(int k=0;k<ng;k++) {
      if(g[k].i<x||g[k].i>=y)continue;
      int p=0;while(p+1<np&&g[k].i>=pc[p+1].i0)p++;
      cs+=g[k].custo;res[nr++]=fabs(b->v[g[k].j].ini-aplicar(pc[p].t,a->v[g[k].i].ini));
    }
    /* Mediana do erro ABSOLUTO: casamentos por acaso (offset errado num
     * dialogo denso) ficam a ~0,5 s; os de verdade, no ruido do autor. */
    double med=0;
    if(nr){qsort(res,(size_t)nr,sizeof *res,cmpD);med=res[nr/2];}
    if(med*1000>md->erro)md->erro=med*1000;
    int largura=y-x;
    if((double)c/(largura>0?largura:1)>=AS_SEG_COBERTURA&&nr>=(largura<4?largura:4)&&cs/(nr?nr:1)<=AS_SEG_CUSTO&&
       med*1000<=tolMs)md->tercos++;
  }
  if(r==AUTOSYNC_OK&&md->tercos<nseg)r=AUTOSYNC_REGION_DISAGREEMENT;
  free(res);free(ok);return r;
}
/* Sequencias longas sem par sob a transformacao base: cada uma pode ter o
 * seu offset (corte, cena a mais, rolo trocado). Devolve candidatas novas. */
static int descobrir(const Falas *a,const Falas *b,const Bits *rg,const Bits *rf,const Grupo *g,int ng,Transf base,
                     double raio,Transf *cand,int nc,Controle *c) {
  int n=a->n;char *ok=calloc((size_t)n,1);if(!ok)return nc;
  /* So conta como casada a fala que caiu perto (0,5 s): longe e acaso. */
  for(int k=0;k<ng;k++)if(fabs(b->v[g[k].j].ini-aplicar(base,a->v[g[k].i].ini))<=.5)
    for(int i=g[k].i;i<g[k].i+g[k].na;i++)ok[i]=1;
  for(int volta=0;volta<3&&nc<AS_MAX_TRECHOS;volta++) {
    /* a maior sequencia sem par, tolerando ate 3 casamentos por acaso no meio */
    int bi=-1,bj=-1;
    for(int i=0;i<n;) {
      if(ok[i]){i++;continue;}
      int j=i,acaso=0;
      while(j<n){if(!ok[j]){j++;acaso=0;continue;}if(++acaso>3)break;j++;}
      j-=acaso;
      if(j-i>bj-bi){bi=i;bj=j;}
      i=j+acaso;
    }
    if(bi<0||bj-bi<12||a->v[bj-1].fim-a->v[bi].ini<30)break;
    double esc=base.p;
    Pico pk=buscar(a->v+bi,bj-bi,rg,rf,&esc,1,base.q,raio,0,c);
    if(c->terminou)break;
    for(int i=bi;i<bj;i++)ok[i]=1;   /* nao tentar a mesma de novo */
    if(pk.score<.6||pk.score-pk.alternativo<AS_MIN_MARGEM)continue;
    int novo=1;
    for(int k=0;k<nc;k++)if(fabs(cand[k].q-pk.q)<1.0)novo=0;
    if(novo)cand[nc++]=(Transf){base.p,pk.q};
  }
  free(ok);return nc;
}
/* Viterbi: rotulo de trecho por fala. Custo da fala = o melhor 1:1 perto da
 * posicao prevista (teto: deixar sem par); trocar de rotulo custa AS_TROCA. */
static int rotular(const Falas *a,const Falas *b,const Transf *cand,int nc,Peca *pc,Controle *c) {
  int n=a->n;double *d=malloc((size_t)n*nc*sizeof *d);unsigned char *de=malloc((size_t)n*nc);
  int np=0;
  if(!d||!de){free(d);free(de);c->terminou=AUTOSYNC_MEMORY;return 0;}
  for(int i=0;i<n;i++) {
    if(!(i%256)&&parou(c)){free(d);free(de);return 0;}
    double melhorAnt=1e30;int ka=0;
    if(i)for(int k=0;k<nc;k++)if(d[(i-1)*nc+k]<melhorAnt){melhorAnt=d[(i-1)*nc+k];ka=k;}
    for(int k=0;k<nc;k++) {
      double ts=aplicar(cand[k],a->v[i].ini),te=aplicar(cand[k],a->v[i].fim),cm=AS_PULA_ALVO;
      int j=primeiraApos(b,ts-1);
      /* Mais apertado que o custo de grupo: aqui so importa qual trecho
       * poe a fala em cima de uma da referencia. */
      for(;j<b->n&&b->v[j].ini<=ts+1;j++)cm=fmin(cm,(fabs(b->v[j].ini-ts)+fabs(b->v[j].fim-te))/.5);
      double fica=i?d[(i-1)*nc+k]:0,troca=i?melhorAnt+AS_TROCA:1e30;
      d[i*nc+k]=cm+(fica<=troca?fica:troca);de[i*nc+k]=(unsigned char)(fica<=troca?k:ka);
    }
  }
  int k=0;for(int x=1;x<nc;x++)if(d[(n-1)*nc+x]<d[(n-1)*nc+k])k=x;
  int *rot=malloc((size_t)n*sizeof *rot);
  if(!rot){free(d);free(de);c->terminou=AUTOSYNC_MEMORY;return 0;}
  for(int i=n-1;i>=0;i--){rot[i]=k;k=de[i*nc+k];}
  for(int i=0;i<n;) {
    int j=i;while(j<n&&rot[j]==rot[i])j++;
    if(np&&(j-i<8||cand[rot[i]].q==pc[np-1].t.q))pc[np-1].i1=j;   /* curto demais: fica com o anterior */
    else if(np==AS_MAX_TRECHOS){np=-1;break;}
    else pc[np++]=(Peca){cand[rot[i]],i,j};
    i=j;
  }
  free(rot);free(d);free(de);return np;
}
/* Alinha cada trecho com a sua semente e refina o offset dele (escala comum). */
static int alinharPecas(const Falas *a,const Falas *b,Peca *pc,int np,int livre,double *ts,double *te,
                        Grupo *g,int *ng,Controle *c) {
  for(int volta=0;volta<2;volta++) {
    *ng=0;transformar(a,pc,np,ts,te);
    for(int k=0;k<np;k++) {
      int n=0;
      if(!alinharBanda(ts,te,pc[k].i0,pc[k].i1,b,g+*ng,&n,c))return 0;
      if(volta==0) {
        ajustar(a,b,g+*ng,n,livre&&np==1,&pc[k].t);
        if(livre&&np==1&&fabs(pc[k].t.p-1)*(a->fim-a->ini)<AS_UNIDADE)pc[k].t.p=1;
      }
      *ng+=n;
    }
  }
  return 1;
}
/* Indice da fala que fecha (fim=0: a ultima; 1: a primeira) a sequencia
 * firme mais perto da ponta do trecho: 4 grupos firmes sem fala solta
 * entre eles. -1 = nenhuma. */
/* Grupo firme: inicio E fim a ate 0,35 s da transformacao. Por acaso, num
 * dialogo denso, so um em ~30 passa nos dois. */
static int grupoFirme(const Falas *a,const Falas *b,Transf t,const Grupo *g) {
  double fa=a->v[g->i].fim,fb=b->v[g->j].fim;
  for(int k=1;k<g->na;k++)fa=fmax(fa,a->v[g->i+k].fim);
  for(int k=1;k<g->nb;k++)fb=fmax(fb,b->v[g->j+k].fim);
  return fabs(b->v[g->j].ini-aplicar(t,a->v[g->i].ini))<=.35&&fabs(fb-aplicar(t,fa))<=.35;
}
static int firme(const Falas *a,const Falas *b,const Peca *pc,const Grupo *g,int ng,int ini) {
  int x0=ini?0:ng-1,dx=ini?1:-1;
  for(int x=x0;x>=0&&x<ng;x+=dx) {
    int ok=1;
    for(int y=0;y<4&&ok;y++) {
      int z=x+y*dx;
      if(z<0||z>=ng||g[z].i<pc->i0||g[z].i>=pc->i1){ok=0;break;}
      if(g[z].na!=1||g[z].nb!=1||!grupoFirme(a,b,pc->t,&g[z]))ok=0;   /* agrupado casa por acaso mais facil */
      if(y){int w=z-dx,lo=ini?w:z,hi=ini?z:w;if(g[hi].i!=g[lo].i+g[lo].na)ok=0;}
    }
    if(ok)return ini?g[x].i:g[x].i+g[x].na-1;
  }
  return -1;
}
/* Emendas entre trechos: um casamento isolado junto da emenda e acaso
 * (a cena cortada caiu perto de outra fala), e nenhum grupo de um trecho
 * pode ficar no video antes do fim do trecho anterior. Tira esses grupos. */
static void limparEmendas(const Falas *a,const Falas *b,const Peca *pc,int np,Grupo *g,int *ng) {
  int n=a->n;char *ok=calloc((size_t)n,1),*fora=calloc((size_t)*ng+1,1);
  if(!ok||!fora){free(ok);free(fora);return;}
  for(int k=0;k<*ng;k++)for(int i=g[k].i;i<g[k].i+g[k].na;i++)ok[i]=1;
  for(int e=1;e<np;e++) {
    for(int lado=0;lado<2;lado++) {   /* 0: fim do trecho e-1; 1: comeco do e */
      int x0=lado?0:*ng-1,dx=lado?1:-1;
      for(int x=x0;x>=0&&x<*ng;x+=dx) {
        int i=g[x].i;
        if(lado?(i<pc[e].i0||i>=pc[e].i1):(i<pc[e-1].i0||i>=pc[e-1].i1))continue;
        if(fora[x])continue;
        int cas=0,tot=0;
        for(int y=1;y<=6;y++) {
          int z=lado?i+g[x].na-1+y:i-y;
          if(z<0||z>=n||(lado?z>=pc[e].i1:z<pc[e-1].i0))break;
          tot++;cas+=ok[z];
        }
        if(tot<6||cas>=4)break;
        fora[x]=1;for(int z=i;z<i+g[x].na;z++)ok[z]=0;
      }
    }
    /* Os dois lados nao podem se sobrepor no video: ha um instante V do
     * corte, antes dele so o trecho anterior, depois so este. V e o que
     * deixa mais grupos do lado certo com o inicio a <= 0,35 s; empate entre posicoes (a
     * mesma fala da referencia serve aos dois lados) tira os grupos do
     * intervalo empatado dos dois: melhor faltar que mostrar a errada. */
    double fimAnt=-1e30,iniPos=1e30;
    for(int x=0;x<*ng;x++)if(!fora[x]) {
      if(g[x].i>=pc[e-1].i0&&g[x].i<pc[e-1].i1)fimAnt=fmax(fimAnt,b->v[g[x].j].ini);
      if(g[x].i>=pc[e].i0&&g[x].i<pc[e].i1)iniPos=fmin(iniPos,b->v[g[x].j].ini);
    }
    if(iniPos<=fimAnt) {
      int melhor=-1;double vMin=0,vMax=0;
      for(int y=-1;y<*ng;y++) {
        double V;
        if(y<0)V=iniPos;   /* tudo do trecho anterior cai */
        else {
          if(fora[y]||b->v[g[y].j].ini<iniPos||b->v[g[y].j].ini>fimAnt)continue;
          V=b->v[g[y].j].ini+.001;   /* logo depois deste inicio */
        }
        int pts=0;
        for(int x=0;x<*ng;x++) {
          if(fora[x])continue;
          double rs=b->v[g[x].j].ini;
          if(rs<iniPos||rs>fimAnt)continue;
          /* aqui so o inicio: um grupo 2:1 com uma fala real tambem disputa */
          if(g[x].i>=pc[e-1].i0&&g[x].i<pc[e-1].i1&&rs<V&&fabs(rs-aplicar(pc[e-1].t,a->v[g[x].i].ini))<=.35)pts++;
          if(g[x].i>=pc[e].i0&&g[x].i<pc[e].i1&&rs>=V&&fabs(rs-aplicar(pc[e].t,a->v[g[x].i].ini))<=.35)pts++;
        }
        if(pts>melhor){melhor=pts;vMin=vMax=V;}
        else if(pts==melhor){vMin=fmin(vMin,V);vMax=fmax(vMax,V);}
      }
      for(int x=0;x<*ng;x++) {
        if(fora[x])continue;
        double rs=b->v[g[x].j].ini;
        if(g[x].i>=pc[e-1].i0&&g[x].i<pc[e-1].i1&&rs>=vMin-.001)fora[x]=1;
        if(g[x].i>=pc[e].i0&&g[x].i<pc[e].i1&&rs<vMax)fora[x]=1;
      }
    }
  }
  int m=0;for(int x=0;x<*ng;x++)if(!fora[x])g[m++]=g[x];
  *ng=m;free(ok);free(fora);
}
/* Ancoras de linha: o grupo vai para o inicio da referencia quando ele e os
 * 7 vizinhos concordam num desvio local. So entra no mapa o que passa da
 * tolerancia: o resto ja estava bom. */
static int ancorar(const Falas *a,const Falas *b,const Peca *pc,int np,const Grupo *g,int ng,int tolMs,
                   AutoSyncMapa *m) {
  double *r=malloc((size_t)(ng+1)*sizeof *r),*j=malloc(7*sizeof *j);
  m->nos=malloc((size_t)(ng+1)*sizeof *m->nos);m->nn=0;
  if(!r||!j||!m->nos){free(r);free(j);return 0;}
  int total=0;
  for(int k=0;k<np;k++) {
    int g0=0;while(g0<ng&&g[g0].i<pc[k].i0)g0++;
    int g1=g0;while(g1<ng&&g[g1].i<pc[k].i1)g1++;
    m->t[k].n0=m->nn;
    for(int x=g0;x<g1;x++)r[x]=b->v[g[x].j].ini-aplicar(pc[k].t,a->v[g[x].i].ini);
    int algum=0;
    for(int x=g0;x<g1;x++) {
      int lo=x-3<g0?g0:x-3,hi=x+3>=g1?g1-1:x+3,q=0;
      for(int y=lo;y<=hi;y++)j[q++]=r[y];
      qsort(j,(size_t)q,sizeof *j,cmpD);
      double med=j[q/2],v=r[x];
      int juntos=0;for(int y=0;y<q;y++)juntos+=fabs(j[y]-med)<=.15;
      /* So move a fala que concorda com a vizinhanca inteira (6 de 7 a
       * 0,15 s da mediana). Nunca a troca pela mediana: com casamentos
       * errados por perto, a mediana e que esta errada. Na ponta do trecho
       * nao ha 7 vizinhos: nao ancora. */
      if(q<7||juntos<6||fabs(r[x]-med)>.25||fabs(v)*1000<=tolMs)v=0;
      else algum=1;
#ifdef AS_DEPURAR
      if(v)fprintf(stderr,"[as]   ancora i=%d j=%d r=%.3f med=%.3f viz=%.2f %.2f %.2f %.2f %.2f %.2f %.2f\n",g[x].i,g[x].j,r[x],med,j[0],j[1],j[2],j[3],j[4],j[5],j[6]);
#endif
      double t=aplicar(pc[k].t,a->v[g[x].i].ini)+v;
      if(m->nn>m->t[k].n0&&t<=m->nos[m->nn-1].t)continue;
      m->nos[m->nn++]=(No){t,v};
      if(v)total++;
    }
    if(!algum)m->nn=m->t[k].n0;
    m->t[k].nn=m->nn-m->t[k].n0;
  }
  free(r);free(j);return total;
}

AutoSyncResultado autosync_alinhar(const LegendaDocumento *doc,const LegendaDocumento *ref,
                                  const AutoSyncConfig *config,AutoSyncCancelar cancelar,void *usuario,
                                  AutoSyncMapa **saida) {
  AutoSyncResultado r={.estado=AUTOSYNC_REJECTED,.motivo=AUTOSYNC_INVALID_ARGUMENT,.escala=1};
  AutoSyncConfig cfg=config?*config:autosync_config(AUTOSYNC_QUICK);
  Controle c={.cancelar=cancelar,.usuario=usuario,.orcamento=cfg.orcamentoMs};
  Falas a={0},b={0};Bits rg={0},rf={0};Grupo *g=NULL;double *ts=NULL,*te=NULL;
  AutoSyncMapa *mapa=NULL;int ng=0;
  clock_gettime(CLOCK_MONOTONIC,&c.inicio);
  if(saida)*saida=NULL;
  const LegendaDocumentoInfo *di=legenda_documento_info(doc),*ri=legenda_documento_info(ref);
  if(!di||!ri){r.estado=AUTOSYNC_UNAVAILABLE;r.motivo=AUTOSYNC_NO_REFERENCE;goto fim;}
  r.documento=legenda_documento_hash(doc);r.referencia=legenda_documento_hash(ref);r.sessao=di->sessao;
  if(di->sessao!=ri->sessao){r.estado=AUTOSYNC_CANCELLED;r.motivo=AUTOSYNC_SESSION_CHANGED;goto fim;}
  if((cfg.modo!=AUTOSYNC_QUICK&&cfg.modo!=AUTOSYNC_THOROUGH)||
     cfg.toleranciaMs<50||cfg.toleranciaMs>1000||cfg.raioBuscaMs<1000||
     cfg.raioBuscaMs>180000||cfg.orcamentoMs<1||cfg.orcamentoMs>20000||
     cfg.manterMs<0||cfg.manterMs>500||doc==ref)goto fim;
  if(parou(&c)){r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
  r.motivo=preparar(doc,&a,&c);if(r.motivo!=AUTOSYNC_OK)goto fim;
  r.motivo=preparar(ref,&b,&c);
  if(r.motivo!=AUTOSYNC_OK){r.referenciaInvalida=1;goto fim;}
  if(di->duracaoSeg>0&&ri->duracaoSeg>0&&fabs(di->duracaoSeg-ri->duracaoSeg)>cfg.toleranciaMs/1000.0) {
    r.motivo=AUTOSYNC_REGION_DISAGREEMENT;goto fim;
  }
  if(!bitsMontar(&rg,b.v,b.n,1,0,AS_GROSSO)||!bitsMontar(&rf,b.v,b.n,1,0,AS_FINO)){r.motivo=AUTOSYNC_MEMORY;goto fim;}
  /* 1. A transformacao global. */
  double escalas[12]={1,25/23.976,23.976/25,25/24.0,24/25.0,24/23.976,23.976/24};int ne=7;
  {
    double ra=b.fim-b.ini,rd=a.fim-a.ini,e=rd>0?ra/rd:1;
    int novo=e>=.94&&e<=1.06;
    for(int k=0;k<ne;k++)if(fabs(escalas[k]-e)<.00035)novo=0;
    if(novo)escalas[ne++]=e;
  }
  int parcial=(a.fim-a.ini)<AS_PARCIAL*(b.fim-b.ini);
  Pico pk=buscar(a.v,a.n,&rg,&rf,escalas,ne,0,cfg.raioBuscaMs/1000.0,parcial,&c);
  if(c.terminou){r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
  r.confianca=pk.score;r.alternativa=pk.alternativo;
  if(pk.score<AS_MIN_ATIVIDADE){r.motivo=AUTOSYNC_LOW_CONFIDENCE;goto fim;}
  /* Margem curta: dois picos. Pode ser corte (cada pico explica uma parte
   * da legenda, vira trechos com margem propria) ou estrutura repetida (os
   * dois explicam as mesmas falas: recusa). */
  int ambiguo=pk.score-pk.alternativo<AS_MIN_MARGEM;
  g=malloc(((size_t)a.n+1)*sizeof *g);ts=malloc((size_t)a.n*sizeof *ts);te=malloc((size_t)a.n*sizeof *te);
  if(!g||!ts||!te){r.motivo=AUTOSYNC_MEMORY;goto fim;}
  int nseg=cfg.modo==AUTOSYNC_THOROUGH?6:3,livre=pk.p!=1;
  Peca pc[AS_MAX_TRECHOS]={{{pk.p,pk.q},0,a.n}};int np=1;
  /* 2. Programacao dinamica e refino com um trecho so. */
  if(!alinharPecas(&a,&b,pc,1,livre,ts,te,g,&ng,&c)){r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
  Medida md;
  int descoberta=pc[0].t.p!=1;
  AutoSyncMotivo m=medir(&a,&b,pc,1,g,ng,nseg,cfg.toleranciaMs,descoberta,&md);
  if(m==AUTOSYNC_MEMORY){r.motivo=m;goto fim;}
  if(m==AUTOSYNC_OK&&ambiguo)m=AUTOSYNC_AMBIGUOUS;
#ifdef AS_DEPURAR
  fprintf(stderr,"[as] pico p=%.5f q=%.2f s=%.3f alt=%.3f -> p=%.5f q=%.3f m=%s cob=%.3f custo=%.2f seg=%d erro=%.0f\n",
    pk.p,pk.q,pk.score,pk.alternativo,pc[0].t.p,pc[0].t.q,autosync_motivo(m),md.cobertura,md.custo,md.tercos,md.erro);
#endif
  /* 3. Sem cobertura: trechos com offset proprio. */
  if(m!=AUTOSYNC_OK) {
    Transf cand[AS_MAX_TRECHOS]={pc[0].t};
    int nc=descobrir(&a,&b,&rg,&rf,g,ng,pc[0].t,cfg.raioBuscaMs/1000.0,cand,1,&c);
    if(c.terminou){r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
#ifdef AS_DEPURAR
    for(int k=0;k<nc;k++)fprintf(stderr,"[as]  cand %d q=%.3f\n",k,cand[k].q);
#endif
    if(nc>1) {
      Peca npc[AS_MAX_TRECHOS];int nn=rotular(&a,&b,cand,nc,npc,&c);
      if(c.terminou){r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
      if(nn>1) {
        Grupo *g2=malloc(((size_t)a.n+1)*sizeof *g2);int ng2=0;Medida m2;
        if(!g2){r.motivo=AUTOSYNC_MEMORY;goto fim;}
        if(!alinharPecas(&a,&b,npc,nn,0,ts,te,g2,&ng2,&c)){free(g2);r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
        limparEmendas(&a,&b,npc,nn,g2,&ng2);
        AutoSyncMotivo m3=medir(&a,&b,npc,nn,g2,ng2,nseg,cfg.toleranciaMs,1,&m2);
#ifdef AS_DEPURAR
        for(int k=0;k<nn;k++)fprintf(stderr,"[as]  trecho %d [%d,%d) q=%.3f\n",k,npc[k].i0,npc[k].i1,npc[k].t.q);
        fprintf(stderr,"[as]  trechos m=%s cob=%.3f custo=%.2f seg=%d erro=%.0f\n",autosync_motivo(m3),m2.cobertura,m2.custo,m2.tercos,m2.erro);
#endif
        /* Cada trecho tem de se sustentar sozinho, com margem propria e em
         * ordem no video. */
        for(int k=0;k<nn&&m3==AUTOSYNC_OK;k++) {
          int cas=0;
          for(int x=0;x<ng2;x++)if(g2[x].i>=npc[k].i0&&g2[x].i<npc[k].i1)cas+=g2[x].na;
          if(cas<AS_MIN_CASADAS||a.v[npc[k].i1-1].fim-a.v[npc[k].i0].ini<AS_MIN_TRECHO_SEG)m3=AUTOSYNC_REGION_DISAGREEMENT;
          if(k&&aplicar(npc[k].t,a.v[npc[k].i0].ini)<=aplicar(npc[k-1].t,a.v[npc[k-1].i0].ini))m3=AUTOSYNC_REGION_DISAGREEMENT;
          if(m3==AUTOSYNC_OK) {
            double esc=npc[k].t.p;
            Pico lp=buscar(a.v+npc[k].i0,npc[k].i1-npc[k].i0,&rg,&rf,&esc,1,npc[k].t.q,cfg.raioBuscaMs/1000.0,0,&c);
            if(c.terminou){free(g2);r.motivo=(AutoSyncMotivo)c.terminou;goto fim;}
            if(lp.score-lp.alternativo<AS_MIN_MARGEM||fabs(lp.q-npc[k].t.q)>1)m3=AUTOSYNC_AMBIGUOUS;
          }
        }
        if(m3==AUTOSYNC_OK){free(g);g=g2;ng=ng2;np=nn;memcpy(pc,npc,sizeof npc);md=m2;m=m3;}
        else free(g2);
      }
    }
  }
  r.casadas=md.casadas;r.cobertura=md.cobertura;r.regioes=md.tercos;r.erroMs=(int)lround(md.erro);
  if(m!=AUTOSYNC_OK){r.motivo=m;goto fim;}
  /* 4. Mapa, ancoras e o score final sobre a legenda JA corrigida. */
  mapa=calloc(1,sizeof *mapa);
  if(!mapa){r.motivo=AUTOSYNC_MEMORY;goto fim;}
  mapa->nt=np;
  for(int k=0;k<np;k++) {
    mapa->t[k].p=pc[k].t.p;mapa->t[k].q=pc[k].t.q;
    mapa->t[k].s1=1e30;
    if(k) {
      /* Emenda. Perto dela ha casamento por acaso (a cena cortada cai em
       * cima de outras falas), entao cada lado so vale ate a sua ultima
       * sequencia FIRME (4 grupos seguidos, inicio e fim a <= 0,35 s). O que fica entre
       * as duas nao aparece: melhor faltar uma fala que mostrar uma fora. */
      int ua=firme(&a,&b,&pc[k-1],g,ng,0),pri=firme(&a,&b,&pc[k],g,ng,1);
      if(ua<0||pri<0){r.motivo=AUTOSYNC_REGION_DISAGREEMENT;goto fim;}
      mapa->t[k-1].s1=a.v[ua].fim+.05;
      mapa->t[k].s0=a.v[pri].ini-.05;
      double g0=aplicar(pc[k-1].t,mapa->t[k-1].s1),g1=aplicar(pc[k].t,mapa->t[k].s0);
      mapa->t[k].t0=g1>g0?g1:g0;
      if(mapa->t[k].t0<=mapa->t[k-1].t0){r.motivo=AUTOSYNC_REGION_DISAGREEMENT;goto fim;}
#ifdef AS_DEPURAR
      fprintf(stderr,"[as]  emenda %d: ultima %d (%.2f) primeira %d (%.2f) t0=%.2f\n",k,ua,a.v[ua].fim,pri,a.v[pri].ini,mapa->t[k].t0);
#endif
    } else mapa->t[k].t0=mapa->t[k].s0=-1e30;
  }
  r.ancoras=ancorar(&a,&b,pc,np,g,ng,cfg.toleranciaMs,mapa);
  if(!mapa->nos){r.motivo=AUTOSYNC_MEMORY;goto fim;}
  transformar(&a,pc,np,ts,te);   /* a tentativa de trechos pode ter sobrescrito */
  {
    Fala *v=malloc((size_t)a.n*sizeof *v);Bits x;
    if(!v){r.motivo=AUTOSYNC_MEMORY;goto fim;}
    /* Inverte o mapa fala a fala (o mapa vai do video para a legenda). */
    for(int i=0;i<a.n;i++)v[i]=(Fala){ts[i],te[i]};
    int ok=bitsMontar(&x,v,a.n,1,0,AS_FINO);free(v);
    if(!ok){r.motivo=AUTOSYNC_MEMORY;goto fim;}
    r.confianca=pontuar(&x,&rf,0);bitsLiberar(&x);
  }
  if(r.confianca<AS_MIN_SCORE){r.motivo=AUTOSYNC_LOW_CONFIDENCE;goto fim;}
  r.trechos=np;r.escala=pc[0].t.p;
  r.tipo=np>1||r.ancoras?AUTOSYNC_T_TRECHOS:pc[0].t.p!=1?AUTOSYNC_T_ESCALA:AUTOSYNC_T_OFFSET;
  {
    double meio=(a.ini+a.fim)/2;int k=0;
    while(k+1<np&&meio>=a.v[pc[k+1].i0].ini)k++;
    r.offsetMs=(int)lround((meio-aplicar(pc[k].t,meio))*1000);
  }
  if(r.tipo==AUTOSYNC_T_OFFSET&&abs(r.offsetMs)<=cfg.manterMs) {
    /* Ja estava boa: a correcao seria menor que a tolerancia da pessoa. */
    r.offsetMs=0;r.mantida=1;mapa->t[0].q=0;
  }
  r.estado=AUTOSYNC_ACCEPTED;r.motivo=AUTOSYNC_OK;
fim:
  if(r.motivo==AUTOSYNC_SESSION_CHANGED)r.estado=AUTOSYNC_CANCELLED;
  if(r.estado==AUTOSYNC_ACCEPTED&&saida){*saida=mapa;mapa=NULL;}
  else if(r.estado!=AUTOSYNC_ACCEPTED){r.offsetMs=0;r.tipo=AUTOSYNC_T_OFFSET;r.escala=1;r.trechos=0;r.ancoras=0;}
  autosync_mapa_liberar(mapa);
  r.tempoMs=tempoMs(&c.inicio);free(a.v);free(b.v);bitsLiberar(&rg);bitsLiberar(&rf);
  free(g);free(ts);free(te);return r;
}
AutoSyncResultado autosync_comparar(const LegendaDocumento *doc,const LegendaDocumento *ref,
                                   const AutoSyncConfig *config,AutoSyncCancelar cancelar,void *usuario) {
  return autosync_alinhar(doc,ref,config,cancelar,usuario,NULL);
}

typedef struct {
  LegendaDocumento *doc,*ref;AutoSyncConfig cfg;AutoSyncResultado resultado;
  uint64_t epoch,excluidas[AS_EXCLUIDAS];int nExcluidas,manual,automatico,pendente;
  AutoSyncMapa *mapa; /* aceito ESCALA/TRECHOS em vigor (offset puro: automatico) */
} Slot;
struct AutoSync {
  pthread_mutex_t trava;pthread_cond_t acordar;pthread_t fio;
  uint64_t sessao;int parar,proximo;Slot slots[2];
};
typedef struct {AutoSync *s;int slot;uint64_t sessao,epoch;} Cancelamento;
static int vigente(void *u) {
  Cancelamento *c=u;AutoSync *s=c->s;int cancelado;
  pthread_mutex_lock(&s->trava);
  cancelado=s->parar||s->sessao!=c->sessao||s->slots[c->slot].epoch!=c->epoch;
  pthread_mutex_unlock(&s->trava);return cancelado;
}
static void limparSlot(Slot *s) {
  legenda_documento_liberar(s->doc);legenda_documento_liberar(s->ref);autosync_mapa_liberar(s->mapa);
  uint64_t epoch=s->epoch+1;memset(s,0,sizeof *s);s->epoch=epoch;
  s->resultado.estado=AUTOSYNC_UNAVAILABLE;s->resultado.motivo=AUTOSYNC_NO_REFERENCE;
}
static void *trabalhar(void *u) {
  AutoSync *s=u;
  for(;;) {
    pthread_mutex_lock(&s->trava);
    while(!s->parar&&!s->slots[0].pendente&&!s->slots[1].pendente)pthread_cond_wait(&s->acordar,&s->trava);
    if(s->parar){pthread_mutex_unlock(&s->trava);return NULL;}
    int k=s->slots[s->proximo].pendente?s->proximo:1-s->proximo;s->proximo=1-k;
    Slot *slot=&s->slots[k];slot->pendente=0;
    LegendaDocumento *doc=legenda_documento_reter(slot->doc),*ref=legenda_documento_reter(slot->ref);
    AutoSyncConfig cfg=slot->cfg;Cancelamento token={s,k,s->sessao,slot->epoch};
    pthread_mutex_unlock(&s->trava);
    AutoSyncMapa *mapa=NULL;
    AutoSyncResultado resultado=autosync_alinhar(doc,ref,&cfg,vigente,&token,&mapa);
    pthread_mutex_lock(&s->trava);
    int publicado=!s->parar&&s->sessao==token.sessao&&slot->epoch==token.epoch;
    if(publicado) {
      slot->resultado=resultado;
      if(resultado.estado==AUTOSYNC_ACCEPTED) {
        /* Offset puro segue pelo caminho de sempre (manual + automatico);
         * escala/trechos pelo mapa, sem offset somado. */
        int puro=resultado.tipo==AUTOSYNC_T_OFFSET;
        slot->automatico=puro?resultado.offsetMs:0;
        AutoSyncMapa *velho=slot->mapa;slot->mapa=puro?NULL:mapa;
        if(!puro)mapa=velho;else autosync_mapa_liberar(velho);
      }
    }
    pthread_mutex_unlock(&s->trava);
    autosync_mapa_liberar(mapa);
    /* stderr may block when an external log consumer stalls. Never hold the
     * state lock while writing logs or freeing retained worker documents. */
    if(publicado) {
      static const char *const tipos[]={"offset","scale","pieces"};
      fprintf(stderr,"[autosync] subtitle_sync %s reason=%s slot=%d kind=%s offset_ms=%d scale=%.5f pieces=%d anchors=%d"
        " matched=%d coverage=%.3f confidence=%.3f alternative=%.3f kept=%d elapsed_ms=%d\n",
        resultado.estado==AUTOSYNC_ACCEPTED?"accepted":"rejected",autosync_motivo(resultado.motivo),k,
        tipos[resultado.tipo],resultado.offsetMs,resultado.escala,resultado.trechos,resultado.ancoras,
        resultado.casadas,resultado.cobertura,resultado.confianca,resultado.alternativa,resultado.mantida,resultado.tempoMs);
    }
    legenda_documento_liberar(doc);legenda_documento_liberar(ref);
  }
}
AutoSync *autosync_criar(void) {
  AutoSync *s=calloc(1,sizeof *s);if(!s)return NULL;
  for(int i=0;i<2;i++)s->slots[i].resultado=(AutoSyncResultado){
    .estado=AUTOSYNC_UNAVAILABLE,.motivo=AUTOSYNC_NO_REFERENCE};
  if(pthread_mutex_init(&s->trava,NULL)){free(s);return NULL;}
  if(pthread_cond_init(&s->acordar,NULL)){pthread_mutex_destroy(&s->trava);free(s);return NULL;}
  if(pthread_create(&s->fio,NULL,trabalhar,s)) {
    pthread_cond_destroy(&s->acordar);pthread_mutex_destroy(&s->trava);free(s);return NULL;
  }
  return s;
}
void autosync_destruir(AutoSync *s) {
  if(!s)return;
  pthread_mutex_lock(&s->trava);s->parar=1;pthread_cond_signal(&s->acordar);
  pthread_mutex_unlock(&s->trava);pthread_join(s->fio,NULL);
  limparSlot(&s->slots[0]);limparSlot(&s->slots[1]);
  pthread_cond_destroy(&s->acordar);pthread_mutex_destroy(&s->trava);free(s);
}
void autosync_iniciar(AutoSync *s,uint64_t sessao) {
  if(!s)return;
  pthread_mutex_lock(&s->trava);s->sessao=sessao;
  limparSlot(&s->slots[0]);limparSlot(&s->slots[1]);pthread_mutex_unlock(&s->trava);
}
static int valido(AutoSync *s,int slot){return s&&slot>=0&&slot<2;}
int autosync_selecionar(AutoSync *s,int k,LegendaDocumento *doc) {
  if(!valido(s,k))return 0;
  pthread_mutex_lock(&s->trava);const LegendaDocumentoInfo *info=legenda_documento_info(doc);
  if(doc&&(!info||info->sessao!=s->sessao)){pthread_mutex_unlock(&s->trava);return 0;}
  Slot *slot=&s->slots[k];
  if(doc==slot->doc){pthread_mutex_unlock(&s->trava);return 1;}
  legenda_documento_reter(doc);limparSlot(slot);slot->doc=doc;
  pthread_mutex_unlock(&s->trava);return 1;
}
static int permitido(const Slot *slot,const LegendaDocumento *ref) {
  uint64_t h=legenda_documento_hash(ref);
  if(!h||slot->nExcluidas>=AS_EXCLUIDAS)return 0;
  for(int i=0;i<slot->nExcluidas;i++)if(slot->excluidas[i]==h)return 0;
  return 1;
}
int autosync_referencia_permitida(AutoSync *s,int k,const LegendaDocumento *ref) {
  if(!valido(s,k))return 0;
  pthread_mutex_lock(&s->trava);const LegendaDocumentoInfo *info=legenda_documento_info(ref);
  int ok=info&&info->sessao==s->sessao&&permitido(&s->slots[k],ref);
  pthread_mutex_unlock(&s->trava);return ok;
}
int autosync_solicitar(AutoSync *s,int k,LegendaDocumento *ref,const AutoSyncConfig *cfg) {
  if(!valido(s,k))return 0;
  pthread_mutex_lock(&s->trava);Slot *slot=&s->slots[k];const LegendaDocumentoInfo *info=legenda_documento_info(ref);
  if(!slot->doc||!info||info->sessao!=s->sessao||!permitido(slot,ref)) {
    pthread_mutex_unlock(&s->trava);return 0;
  }
  legenda_documento_reter(ref);legenda_documento_liberar(slot->ref);slot->ref=ref;
  slot->cfg=cfg?*cfg:autosync_config(AUTOSYNC_QUICK);slot->epoch++;slot->pendente=1;
  slot->automatico=0;autosync_mapa_liberar(slot->mapa);slot->mapa=NULL;
  slot->resultado=(AutoSyncResultado){.estado=AUTOSYNC_ANALYSING,.sessao=s->sessao,
    .documento=legenda_documento_hash(slot->doc),.referencia=legenda_documento_hash(ref)};
  pthread_cond_signal(&s->acordar);pthread_mutex_unlock(&s->trava);return 1;
}
void autosync_cancelar(AutoSync *s,int k) {
  if(!valido(s,k))return;
  pthread_mutex_lock(&s->trava);Slot *slot=&s->slots[k];
  slot->epoch++;slot->pendente=0;slot->resultado.estado=AUTOSYNC_CANCELLED;
  slot->resultado.motivo=AUTOSYNC_SESSION_CHANGED;pthread_mutex_unlock(&s->trava);
}
int autosync_manual(AutoSync *s,int k,int atrasoMs) {
  if(!valido(s,k)||atrasoMs < -120000||atrasoMs>120000)return 0;
  pthread_mutex_lock(&s->trava);s->slots[k].manual=atrasoMs;pthread_mutex_unlock(&s->trava);return 1;
}
int autosync_offset_ms(AutoSync *s,int k) {
  if(!valido(s,k))return 0;
  pthread_mutex_lock(&s->trava);int ms=s->slots[k].manual+s->slots[k].automatico;
  pthread_mutex_unlock(&s->trava);return ms;
}
double autosync_posicao(AutoSync *s,int k,double pos) {
  if(!valido(s,k))return pos;
  pthread_mutex_lock(&s->trava);
  /* Busca binaria pequena sob a trava: o mapa e imutavel e so troca aqui. */
  double t=s->slots[k].mapa?autosync_mapa_tempo(s->slots[k].mapa,pos):pos;
  pthread_mutex_unlock(&s->trava);return t;
}
void autosync_desfazer(AutoSync *s,int k) {
  if(!valido(s,k))return;
  pthread_mutex_lock(&s->trava);Slot *slot=&s->slots[k];
  slot->epoch++;slot->pendente=0;slot->automatico=0;autosync_mapa_liberar(slot->mapa);slot->mapa=NULL;
  slot->resultado.estado=AUTOSYNC_CANCELLED;slot->resultado.motivo=AUTOSYNC_SESSION_CHANGED;
  pthread_mutex_unlock(&s->trava);
}
int autosync_tentar_outra(AutoSync *s,int k) {
  if(!valido(s,k))return 0;
  pthread_mutex_lock(&s->trava);Slot *slot=&s->slots[k];uint64_t h=legenda_documento_hash(slot->ref);
  int ok=h&&permitido(slot,slot->ref)&&slot->nExcluidas<AS_EXCLUIDAS;
  if(ok)slot->excluidas[slot->nExcluidas++]=h;
  slot->epoch++;slot->pendente=0;slot->automatico=0;autosync_mapa_liberar(slot->mapa);slot->mapa=NULL;
  slot->resultado.estado=AUTOSYNC_UNAVAILABLE;slot->resultado.motivo=AUTOSYNC_EXCLUDED_REFERENCE;
  pthread_mutex_unlock(&s->trava);return ok;
}
AutoSyncResultado autosync_estado(AutoSync *s,int k) {
  AutoSyncResultado r={.estado=AUTOSYNC_UNAVAILABLE,.motivo=AUTOSYNC_INVALID_ARGUMENT};
  if(!valido(s,k))return r;
  pthread_mutex_lock(&s->trava);r=s->slots[k].resultado;pthread_mutex_unlock(&s->trava);return r;
}
