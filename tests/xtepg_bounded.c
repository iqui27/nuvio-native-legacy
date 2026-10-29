#include "stub_perfisessao.h"
#include <time.h>
static time_t agora=1800000000;
static time_t tempoTeste(time_t *p){if(p)*p=agora;return agora;}
#define time tempoTeste
#include "../src/xtepg.c"
#include <assert.h>

static atomic_int tentativas, modo;
int xtream_e_id(const char *s){return s&&!strncmp(s,"xtream:",7);}
int xtream_ultimo_retry_after(void){return 1;}
int xtream_epg_curto(const char *id,XtreamProg *p,int max,int *st){
  (void)max;atomic_fetch_add(&tentativas,1);
  if(atomic_load(&modo)==429){*st=429;return -1;}
  *st=200;p->ini=agora-10;p->fim=agora+3600;snprintf(p->titulo,sizeof p->titulo,"%s",id);return 1;
}
static void pausa(void){struct timespec t={0,10000000};nanosleep(&t,NULL);}
int main(void){
  fioVivo=1; // Deterministic queued requests without a worker.
  for(int i=0;i<64;i++){char id[24];snprintf(id,sizeof id,"xtream:%d",i);xtepg_querer(id);}
  assert(nFila==64);for(int i=0;i<64;i++)assert(ent[i].estado==XE_PEDIDO);
  xtepg_priorizar("xtream:0");assert(!strcmp(fila[nFila-1],"xtream:0"));
  XeEnt *e=achar("xtream:0");assert(e);
  prontos[0]=(XeResp){.n=1,.st=200,.pedido=e->pedido+1};strcpy(prontos[0].id,e->id);nProntos=1;
  xtepg_passo();assert(e->estado==XE_PEDIDO); // Stale token cannot complete a new request.
  agora+=46;xtepg_passo();for(int i=0;i<64;i++)assert(ent[i].estado==XE_FALHOU);
  xtepg_querer("xtream:0");assert(nFila==64); // Failure cooldown, no retry storm.
  agora+=601;xtepg_querer("xtream:0");assert(nFila==64&&e->estado==XE_PEDIDO);
  xtepg_limpar();fioVivo=0;
  // Without UI consumption, the worker must retain all responses rather than
  // dropping the ninth and leaving its cache entry pending forever.
  for(int i=0;i<20;i++){char id[24];snprintf(id,sizeof id,"xtream:%d",100+i);xtepg_querer(id);}
  int cheio=0;
  for(int i=0;i<600&&!cheio;i++){pthread_mutex_lock(&trava);cheio=nProntos==8;pthread_mutex_unlock(&trava);pausa();}
  assert(cheio);int bons=0;
  for(int i=0;i<1400&&bons<20;i++){
    xtepg_passo();bons=0;for(int j=0;j<20;j++){char id[24];snprintf(id,sizeof id,"xtream:%d",100+j);bons+=xtepg_tem(id);}pausa();
  }
  assert(bons==20);
  xtepg_limpar();atomic_store(&tentativas,0);atomic_store(&modo,429);xtepg_querer("xtream:900");
  for(int i=0;i<600&&xtepg_estado("xtream:900")==XTEPG_CARREGANDO;i++){xtepg_passo();pausa();}
  assert(xtepg_estado("xtream:900")==XTEPG_FALHOU&&atomic_load(&tentativas)==3);
  xtepg_limpar();
  puts("xtepg bounded: 64 queued requests, deadlines, stale replies, backpressure and 429 retry limit PASS");
}
