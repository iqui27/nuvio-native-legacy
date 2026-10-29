#include "m3u.h"
#include "iptvid.h"
#include "guiacorte.h"
#include "dados.h"
#include "perfis.h"
#include "perfisessao.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <sys/stat.h>

typedef struct { char id[80]; char *url, *cabecalhos; } M3uLigacao;
static void liberarLigacoes(M3uLigacao *v,int n) {
  for(int i=0;i<n;i++){free(v[i].url);free(v[i].cabecalhos);}free(v);
}
typedef struct FonteContexto {
  unsigned sid; struct FonteContexto *proximo;
  pthread_mutex_t trava_;
  pthread_mutex_t cargaTrava_;
  char lista_[1100];
  char epg_[1100];
  char epgDetectada_[1100];
  M3uLigacao *canais_;
  int nCanais_;
  int total_;
  int perfil_;
  int lido_;
  int renovarCache_;
  uint64_t sessaoLida_;
  uint64_t revisao_;
  time_t carregada_;
  volatile int cancelada_;
} FonteContexto;
#define IPTV_CTX_INICIALIZAR(c) do { (c)->perfil_ = -1; pthread_mutex_init(&(c)->trava_, NULL); pthread_mutex_init(&(c)->cargaTrava_, NULL); } while (0)
#define IPTV_CTX_EMERGENCIA { .sid = UINT_MAX, .perfil_ = -1, .trava_ = PTHREAD_MUTEX_INITIALIZER, .cargaTrava_ = PTHREAD_MUTEX_INITIALIZER }
// NV_TPK40: rebuild this unit with pthread storage (tools/tpk.sh).
#include "iptvcontexto.inc"
#define trava (contexto()->trava_ )
#define cargaTrava (contexto()->cargaTrava_ )
#define lista (contexto()->lista_ )
#define epg (contexto()->epg_ )
#define epgDetectada (contexto()->epgDetectada_ )
#define canais (contexto()->canais_ )
#define nCanais (contexto()->nCanais_ )
#define total (contexto()->total_ )
#define perfil (contexto()->perfil_ )
#define lido (contexto()->lido_ )
#define renovarCache (contexto()->renovarCache_ )
#define sessaoLida (contexto()->sessaoLida_ )
#define revisao (contexto()->revisao_ )
#define carregada (contexto()->carregada_ )
#define cancelada (contexto()->cancelada_ )

static uint64_t hash(const char *s) {
  uint64_t h=UINT64_C(14695981039346656037);
  for(;*s;s++){h^=(unsigned char)*s;h*=UINT64_C(1099511628211);}return h;
}
static void nomeConfig(char *s,unsigned n,int p) { if(contexto()->sid)snprintf(s,n,"m3u-p%d-s%u.txt",p,contexto()->sid);else snprintf(s,n,"m3u-p%d.txt",p); }
static void nomeCache(char *s,unsigned n,int p,const char *url) {
  if(contexto()->sid)snprintf(s,n,"m3u-cache-p%d-s%u-%016" PRIx64 ".txt",p,contexto()->sid,hash(url));
  else snprintf(s,n,"m3u-cache-p%d-%016" PRIx64 ".txt",p,hash(url));
}
static void limparEpg(int p) {
  static const char *ext[]={"bin","ts","id"};char nome[96];
  for(int i=0;i<3;i++) {
    snprintf(nome,sizeof nome,"epg-conta-2-%08x-p%d.%s",contexto()->sid,p,ext[i]);dados_apagar(nome);
    if(!contexto()->sid){snprintf(nome,sizeof nome,"epg-extra-m3u-p%d.%s",p,ext[i]);dados_apagar(nome);}
  }
}
static void invalidar(void) {
  cancelada=1;
  liberarLigacoes(canais,nCanais); canais=NULL; nCanais=total=0; carregada=0; epgDetectada[0]=0; renovarCache=0; revisao++;
}
static void carregar(void) {
  int p=perfis_ativo(); uint64_t sessao=nv_perfil_sessao_ler();
  if(lido&&perfil==p&&sessaoLida==sessao)return;
  lido=1;perfil=p;sessaoLida=sessao;invalidar();lista[0]=epg[0]=0;
  char nome[64];nomeConfig(nome,sizeof nome,p);
  char *body=dados_ler(nome);if(!body)return;
  for(char *line=body;*line;){char *next=strchr(line,'\n');if(next)*next++=0;else next=line+strlen(line);
    if(!strncmp(line,"lista\t",6)&&strlen(line+6)<sizeof lista)snprintf(lista,sizeof lista,"%s",line+6);
    if(!strncmp(line,"epg\t",4)&&strlen(line+4)<sizeof epg)snprintf(epg,sizeof epg,"%s",line+4);
    line=next;
  }free(body);
}
int m3u_url_valida(const char *s) {
  if(!s)return 0;
  if(!*s)return 1;
  if(strlen(s)>=sizeof lista||strpbrk(s,"\r\n\t "))return 0;
  return !strncmp(s,"http://",7)||!strncmp(s,"https://",8);
}
static int definir(const char *url,const char *grade) {
  if((url && !m3u_url_valida(url)) || (grade && !m3u_url_valida(grade)))return 0;
  pthread_mutex_lock(&trava);carregar();
  char body[2300],nome[64];nomeConfig(nome,sizeof nome,perfil);
  snprintf(body,sizeof body,"lista\t%s\nepg\t%s\n",url?url:lista,grade?grade:epg);
  int atual=nv_perfil_sessao_travar(sessaoLida);
  int ok=atual && dados_gravar(nome,body);
  if(ok){
    limparEpg(perfil);
    if (url && lista[0] && strcmp(lista,url)) {
      char cache[96];nomeCache(cache,sizeof cache,perfil,lista);
      dados_apagar(cache);
    }
    if(url)snprintf(lista,sizeof lista,"%s",url);
    if(grade)snprintf(epg,sizeof epg,"%s",grade);
    invalidar();
  }
  if(atual)nv_perfil_sessao_destravar();
  pthread_mutex_unlock(&trava);return ok;
}
int m3u_definir_lista(const char *s){return s?definir(s,NULL):0;}
int m3u_definir_epg(const char *s){return s?definir(NULL,s):0;}
int m3u_conta_definir(const char *s,const char *e){return s && *s && e?definir(s,e):0;}
void m3u_esquecer(void) {
  pthread_mutex_lock(&trava);carregar();
  if (nv_perfil_sessao_travar(sessaoLida)) {
    char nome[96];limparEpg(perfil);
    nomeCache(nome,sizeof nome,perfil,lista);
    dados_apagar(nome);nomeConfig(nome,sizeof nome,perfil);dados_apagar(nome);
    lista[0]=epg[0]=0;invalidar();
    nv_perfil_sessao_destravar();
  }
  pthread_mutex_unlock(&trava);
}
void m3u_esquecer_todos(void) {
  pthread_mutex_lock(&trava);
  invalidar();lista[0]=epg[0]=0;lido=0;
  // Matches the account cleanup's supported local profile slots.
  for (int p=0;p<=32;p++) {
    limparEpg(p);
    char nome[96];nomeConfig(nome,sizeof nome,p);
    char *body=dados_ler(nome);
    if(body) {
      char *u=!strncmp(body,"lista\t",6)?body+6:NULL;
      if(u) { char *fim=strchr(u,'\n');if(fim)*fim=0;
        nomeCache(nome,sizeof nome,p,u);dados_apagar(nome);
      }
      free(body);
    }
    nomeConfig(nome,sizeof nome,p);dados_apagar(nome);
  }
  pthread_mutex_unlock(&trava);
}
int m3u_configurado(void){pthread_mutex_lock(&trava);carregar();int r=lista[0]!=0;pthread_mutex_unlock(&trava);return r;}
const char *m3u_lista(void){static char s[1100];pthread_mutex_lock(&trava);carregar();snprintf(s,sizeof s,"%s",lista);pthread_mutex_unlock(&trava);return s;}
const char *m3u_epg(void){static char s[1100];pthread_mutex_lock(&trava);carregar();snprintf(s,sizeof s,"%s",epg);pthread_mutex_unlock(&trava);return s;}
int m3u_url_epg(char *s,unsigned n){pthread_mutex_lock(&trava);carregar();const char *u=epg[0]?epg:epgDetectada;int k=snprintf(s,n,"%s",u);pthread_mutex_unlock(&trava);return k>0&&(unsigned)k<n;}
int m3u_e_id(const char *id){unsigned f;char original[80];if(!iptvid_ler("nvm3u:",id,&f,original,sizeof original)||strlen(original)!=22)return 0;for(const char *p=original+6;*p;p++)if(!isxdigit((unsigned char)*p))return 0;return 1;}

// Fetch/parse on the guide/resolver worker, never in a frame or Settings getter.
static int atualizar(uint64_t sessao,int renovar,M3uCanal *saida,int max) {
  char url[1100],nome[96],descoberta[1100];uint64_t rev;int p;
  pthread_mutex_lock(&cargaTrava);
  pthread_mutex_lock(&trava);carregar();
  if(!nv_perfil_sessao_atual(sessao)||!lista[0]){pthread_mutex_unlock(&trava);pthread_mutex_unlock(&cargaTrava);return 0;}
  renovar = renovar || renovarCache;
  if(!saida&&!renovar&&canais&&time(NULL)-carregada<3600){int n=nCanais;pthread_mutex_unlock(&trava);pthread_mutex_unlock(&cargaTrava);return n;}
  snprintf(url,sizeof url,"%s",lista);p=perfil;rev=revisao;cancelada=0;
  nomeCache(nome,sizeof nome,p,url);
  pthread_mutex_unlock(&trava);
  char *body=NULL;
  if(!renovar){
    char caminho[1024];struct stat st;
    if(dados_caminho(caminho,sizeof caminho,nome)&&!stat(caminho,&st)&&time(NULL)-st.st_mtime<3600)body=dados_ler(nome);
  }
  int daRede=0;
  if(!body){
    RedeControle ctl={0};RedeMedida med={0};long bytes=0;
    ctl.max_bytes=16*1024*1024;ctl.cancelado=&cancelada;
    body=rede_baixar_bin_medido_controle(url,45,NULL,&ctl,&bytes,&med);
    if(!body||med.status<200||med.status>=300||med.limitado||bytes<=0){free(body);body=NULL;}
    else if(memchr(body,0,(size_t)bytes)){free(body);body=NULL;}
    else {char *b=realloc(body,(size_t)bytes+1);if(b){body=b;body[bytes]=0;daRede=1;}else{free(body);body=NULL;}}
  }
  if(!body)body=dados_ler(nome);
  M3uCanal *meta=body?calloc(GUIACORTE_MAX_CANAL,sizeof *meta):NULL;
  char *cache=body&&daRede?strdup(body):NULL;
  int totalNovo=0,n=meta?m3u_processar(body,url,meta,GUIACORTE_MAX_CANAL,descoberta,sizeof descoberta,&totalNovo):0;
  free(body);
  // A malformed HTTP 200 must preserve the previous playlist, too.
  if(!n&&daRede&&meta){
    free(cache);cache=NULL;daRede=0;body=dados_ler(nome);
    if(body)n=m3u_processar(body,url,meta,GUIACORTE_MAX_CANAL,descoberta,sizeof descoberta,&totalNovo);
    free(body);
  }
  M3uLigacao *novo=n?calloc((size_t)n,sizeof *novo):NULL;
  int alocadas=0;
  if(novo)for(int i=0;i<n;i++){
    snprintf(novo[i].id,sizeof novo[i].id,"%s",meta[i].id);
    novo[i].url=strdup(meta[i].url);novo[i].cabecalhos=strdup(meta[i].cabecalhos);alocadas=i+1;
    if(!novo[i].url||!novo[i].cabecalhos){n=0;break;}
  }
  if(!novo)n=0;
  pthread_mutex_lock(&trava);
  if(n>0&&rev==revisao&&p==perfil&&nv_perfil_sessao_travar(sessao)){
    liberarLigacoes(canais,nCanais);canais=novo;novo=NULL;nCanais=n;total=totalNovo;
    carregada=daRede?time(NULL):(renovar?0:time(NULL));renovarCache=0;
    snprintf(epgDetectada,sizeof epgDetectada,"%s",descoberta);
    if(saida){if(n>max)n=max;memcpy(saida,meta,sizeof *saida*(size_t)n);}
    if(cache)dados_gravar_leve(nome,cache);
    nv_perfil_sessao_destravar();
    printf("[m3u] %d channel(s) loaded, %d declared\n",nCanais,totalNovo);fflush(stdout);
  } else n=0;
  pthread_mutex_unlock(&trava);
  liberarLigacoes(novo,novo?alocadas:0);free(meta);free(cache);pthread_mutex_unlock(&cargaTrava);return n;
}
int m3u_canais_sessao(M3uCanal *out,int max,uint64_t sessao) {
  return !out||max<1?0:atualizar(sessao,0,out,max);
}
int m3u_ultimo_total(void){pthread_mutex_lock(&trava);int n=total;pthread_mutex_unlock(&trava);return n;}
static int resolverSessao(const char *id,char *out,unsigned n,uint64_t sessao,int renovar) {
  if(n)out[0]=0;
  if(!m3u_e_id(id)||!nv_perfil_sessao_atual(sessao))return 0;
  int pronto=0;
  pthread_mutex_lock(&trava);carregar();
  if(!renovar && nv_perfil_sessao_atual(sessao)) {
    for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)) {
      int k=snprintf(out,n,"%s",canais[i].url);pronto=k>0&&(unsigned)k<n;break;
    }
  }
  pthread_mutex_unlock(&trava);
  if(pronto)return 1;
  if(!atualizar(sessao,renovar,NULL,0))return 0;
  pthread_mutex_lock(&trava);int ok=0;
  if(nv_perfil_sessao_atual(sessao)) {
    for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)){
      int k=snprintf(out,n,"%s",canais[i].url);ok=k>0&&(unsigned)k<n;break;
    }
  }
  pthread_mutex_unlock(&trava);return ok;
}
static int cabecalhos(const char *id,char *out,unsigned n) {
  if(n)out[0]=0;
  pthread_mutex_lock(&trava);carregar();int ok=0;
  for(int i=0;i<nCanais;i++)if(!strcmp(id,canais[i].id)){int k=snprintf(out,n,"%s",canais[i].cabecalhos);ok=k>=0&&(unsigned)k<n;break;}
  pthread_mutex_unlock(&trava);return ok;
}
void m3u_recarregar(void){pthread_mutex_lock(&trava);carregar();carregada=0;renovarCache=1;pthread_mutex_unlock(&trava);}

void m3u_fonte_selecionar(unsigned id){atomic_store(&contaEditada,id);}
int m3u_fonte_configurado(unsigned id){unsigned a=usarFonte(id);int r=m3u_configurado();usarFonte(a);return r;}
int m3u_fonte_canais(unsigned id,M3uCanal *out,int max,uint64_t sessao){
  unsigned a=usarFonte(id);int n=m3u_canais_sessao(out,max,sessao);
  for(int i=0;i<n;i++){char original[80];snprintf(original,sizeof original,"%s",out[i].id);
    if(!iptvid_montar("nvm3u:",id,original,out[i].id,sizeof out[i].id)){n=i;break;}}
  usarFonte(a);return n;
}
int m3u_fonte_urls(unsigned id,char *url,unsigned nUrl,char *grade,unsigned nGrade) {
  if (!url || !nUrl || !grade || !nGrade) return 0;
  unsigned a=usarFonte(id);
  pthread_mutex_lock(&trava);carregar();
  int l=snprintf(url,nUrl,"%s",lista),e=snprintf(grade,nGrade,"%s",epg);
  int ok=l>0&&(unsigned)l<nUrl&&e>=0&&(unsigned)e<nGrade;
  if(!ok)url[0]=grade[0]=0;
  pthread_mutex_unlock(&trava);usarFonte(a);return ok;
}
int m3u_fonte_epg(unsigned id,char *out,unsigned n){unsigned a=usarFonte(id);int r=m3u_url_epg(out,n);usarFonte(a);return r;}
int m3u_fonte_total(unsigned id){unsigned a=usarFonte(id);int r=m3u_ultimo_total();usarFonte(a);return r;}
void m3u_fonte_recarregar(unsigned id){unsigned a=usarFonte(id);m3u_recarregar();usarFonte(a);}
void m3u_fonte_esquecer(unsigned id){unsigned a=usarFonte(id);m3u_esquecer();usarFonte(a);}
int m3u_resolver_sessao(const char *id,char *out,unsigned n,uint64_t sessao,int renovar){
  unsigned f;char original[80];if(!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,original,sizeof original))return 0;
  unsigned a=usarFonte(f);int r=resolverSessao(original,out,n,sessao,renovar);usarFonte(a);return r;
}
int m3u_cabecalhos(const char *id,char *out,unsigned n){
  unsigned f;char original[80];if(n)out[0]=0;
  if(!m3u_e_id(id)||!iptvid_ler("nvm3u:",id,&f,original,sizeof original))return 0;
  unsigned a=usarFonte(f);int r=cabecalhos(original,out,n);usarFonte(a);return r;
}

void m3u_fontes_invalidar(void) {
  pthread_mutex_lock(&fontesTrava);
  for(FonteContexto *c=fontesContextos;c;c=c->proximo){ pthread_mutex_lock(&c->trava_);c->cancelada_=1;c->revisao_++;c->lido_=0;
    liberarLigacoes(c->canais_,c->nCanais_);c->canais_=NULL;c->nCanais_=c->total_=0;c->carregada_=0;
    c->lista_[0]=c->epg_[0]=c->epgDetectada_[0]=0;
    pthread_mutex_unlock(&c->trava_); }
  pthread_mutex_unlock(&fontesTrava);
}
