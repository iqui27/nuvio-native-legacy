#include "amigosperfis.h"
#include "recomenda.h"
#include "js.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_mutex *mtx;
static int recPerfil=1, vincEstado, atual=1, recargas;
static unsigned geracao=1;
static char meuId[96]="nuvio:account-a", vincNome[64], fioUrl[128];
static void limparPerfisConta(void);
int recomenda_ativo(void) { return 1; }
void recomenda_pedir_agora(void) {}
static int recTravarFio(void) { if(!atual)return 0;SDL_LockMutex(mtx);return 1; }
static void recDestravarFio(void) { SDL_UnlockMutex(mtx); }
static int bearerSessaoLocal(char *s,size_t n) { snprintf(s,n,"Authorization: Bearer token-a");return 1; }
static int perfilCab(char *s,size_t n,int nuvio) {
  assert(nuvio);if(recPerfil==1){s[0]=0;return 0;}snprintf(s,n,"X-Nuvio-Perfil: %d",recPerfil);return 1;
}
static void url(const char *p) { snprintf(fioUrl,sizeof fioUrl,"https://social.test%s",p); }
static void jsonEsc(char *s,size_t n,const char *v) {
  size_t k=0;for(;*v && k+2<n;v++){if(*v=='"' || *v=='\\')s[k++]='\\';s[k++]=*v;}s[k]=0;
}
static const char *profiles="[{\"profile_index\":1,\"name\":\"Parent\",\"is_primary\":true},"
  "{\"profile_index\":2,\"name\":\"Çağrı \\\"Junior\\\"\",\"avatar_url\":\"https://avatar.test/a.png\",\"is_primary\":false},"
  "{\"profile_index\":3,\"name\":\"Bob\",\"is_primary\":false}]";
typedef struct { const char *route,*method,*body,*reply;int profile,status,cancel; } Step;
static Step steps[16];static int total, cursor;
static void expect(const char *route,const char *method,int profile,const char *body,int status,const char *reply,int cancel) {
  assert(total<16);steps[total++]=(Step){route,method,body,reply,profile,status,cancel};
}
static char *request(const char *route,const char *method,const char **headers,const char *body,int *st) {
  assert(cursor<total);Step *e=&steps[cursor++];
  assert(!strcmp(route,e->route) && !strcmp(method,e->method));
  if(!strcmp(method,"RPC")) {
    assert(!strcmp(headers[0],"token-a") && !headers[1]);
  } else {
    assert(!strcmp(headers[0],"Authorization: Bearer token-a"));
    assert(!strcmp(headers[1],"X-Nuvio-Auth: nuvio"));
    char p[40];snprintf(p,sizeof p,"X-Nuvio-Perfil: %d",e->profile);
    assert(e->profile ? headers[2] && !strcmp(headers[2],p) && !headers[3] : !headers[2]);
  }
  if(e->body)assert(body && !strcmp(body,e->body));
  *st=e->status;
  if(e->cancel){limparPerfisConta();geracao++;atual=0;}
  return e->reply?strdup(e->reply):NULL;
}
static char *nuvem_rpc_com(const char *f,const char *body,const char *token,int *st) {
  const char *h[]={token,NULL};return request(f,"RPC",h,body,st);
}
static char *recBaixar(const char *u,int t,const char **h,int *st) { (void)t;return request(u+19,"GET",h,NULL,st); }
static char *recPostar(const char *u,int t,const char **h,const char *body,int *st) { (void)t;return request(u+19,"POST",h,body,st); }
static void lerContatos(const char **h) { (void)h;recargas++; }
#define REC_TEMPO_REDE 8
#include "../src/recomenda_perfis.inc"
static void reset(void) {
  assert(cursor==total);total=cursor=0;atual=1;limparPerfisConta();vincEstado=0;recargas=0;recPerfil=1;strcpy(meuId,"nuvio:account-a");
}
static void list(int cancel) {
  expect("/v1/contatos/perfis","GET",recPerfil==1?0:recPerfil,NULL,404,"{}",0);
  expect("sync_pull_profiles","RPC",0,"{}",200,profiles,cancel);
  if(!cancel)expect("/v1/contatos","GET",recPerfil==1?0:recPerfil,NULL,200,"{\"contatos\":[{\"id\":\"nuvio:account-a:3\"}]}",0);
  assert(amigosperfis_carregar());tratarPerfisConta(NULL);assert(cursor==total);
}
static void addSteps(int target,int cancelAt,int failStatus,const char *registration,const char *pairReply) {
  expect("sync_pull_profiles","RPC",0,"{}",200,profiles,cancelAt==1);
  if(cancelAt==1)return;
  expect("/v1/eu","POST",recPerfil==1?0:recPerfil,"{}",200,recPerfil==1?"{\"id\":\"nuvio:account-a\"}":"{\"id\":\"nuvio:account-a:2\"}",cancelAt==2);
  if(cancelAt==2)return;
  const char *body=target==1?"{\"nome\":\"Parent\",\"avatar\":\"\"}":"{\"nome\":\"Çağrı \\\"Junior\\\"\",\"avatar\":\"https://avatar.test/a.png\"}";
  expect("/v1/eu","POST",target==1?0:target,body,failStatus,registration,cancelAt==3);
  if(cancelAt==3 || failStatus!=200 || !pairReply)return;
  expect("/v1/contatos","POST",recPerfil==1?0:recPerfil,"{\"codigo\":\"ABC123\"}",200,pairReply,cancelAt==4);
}
int main(void) {
  mtx=SDL_CreateMutex();assert(mtx);AmigoPerfil a[CONTA_PERFIL_MAX];
  const char *reg="{\"id\":\"nuvio:account-a:2\",\"codigo\":\"ABC123\"}";
  const char *paired="{\"ok\":1,\"contato\":{\"id\":\"nuvio:account-a:2\"}}";
  // Only reads while listing; duplicate attempts and existing friends are excluded.
  list(0);assert(amigosperfis_estado()==AP_PRONTO && amigosperfis_listar(a,8)==2);
  assert(a[0].perfil.indice==2 && !strcmp(a[0].perfil.nome,"Çağrı \"Junior\"") && a[1].estado==AP_AMIGO);
  assert(!amigosperfis_adicionar(1) && !amigosperfis_adicionar(3));
  addSteps(2,0,200,reg,paired);assert(amigosperfis_adicionar(2) && !amigosperfis_adicionar(2));tratarPerfisConta(NULL);
  assert(vincEstado==REC_VINC_OK && recargas==1 && recPerfil==1 && !strcmp(meuId,"nuvio:account-a"));
  assert(!strcmp(vincNome,"Çağrı \"Junior\"") && !amigosperfis_adicionar(2));
  // Reload after using fallback still succeeds (the server still has no new route).
  list(0);assert(amigosperfis_estado()==AP_PRONTO);
  // The primary profile uses no profile header when selected from a secondary.
  reset();recPerfil=2;strcpy(meuId,"nuvio:account-a:2");list(0);
  addSteps(1,0,200,"{\"id\":\"nuvio:account-a\",\"codigo\":\"ABC123\"}","{\"ok\":1,\"contato\":{\"id\":\"nuvio:account-a\"}}");
  assert(amigosperfis_adicionar(1));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_OK && recPerfil==2);
  // In-flight profile switch after every stage prevents further work/publication.
  for(int stage=1;stage<=4;stage++){
    reset();list(0);addSteps(2,stage,200,reg,paired);assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);
    assert(cursor==total && vincEstado!=REC_VINC_OK && !recargas && !amigosperfis_listar(a,8));
  }
  reset();list(1);assert(amigosperfis_estado()==AP_NADA);
  // A deleted target is rejected using a fresh account list before registration.
  reset();list(0);expect("sync_pull_profiles","RPC",0,"{}",200,"[{\"profile_index\":1,\"is_primary\":true}]",0);
  assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA && !recargas);
  // Do not pair a different account, use an invalid code, or ignore registration failures.
  const char *bad[]={"{\"id\":\"nuvio:foreign:2\",\"codigo\":\"ABC123\"}","{\"id\":\"nuvio:account-a:2\",\"codigo\":\"AB\\\"123\"}","{\"id\":\"nuvio:account-a:2\",\"codigo\":\"ABC1234\"}","{}"};
  for(size_t i=0;i<sizeof bad/sizeof *bad;i++){
    reset();list(0);addSteps(2,0,200,bad[i],NULL);assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA);
  }
  for(int st=401;st<=503;st+=102){reset();list(0);addSteps(2,0,st,"{}",NULL);assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA);}
  // Pairing remains server-authoritative for blocked or refused connections.
  reset();list(0);addSteps(2,0,200,reg,paired);steps[total-1].status=404;steps[total-1].reply="{}";
  assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA && !recargas);
  reset();list(0);addSteps(2,0,200,reg,"{\"ok\":1,\"contato\":{\"id\":\"nuvio:other\"}}");
  assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA);
  // Fail closed if the current social identity differs from the Nuvio session.
  reset();list(0);expect("sync_pull_profiles","RPC",0,"{}",200,profiles,0);
  expect("/v1/eu","POST",0,"{}",200,"{\"id\":\"nuvio:foreign\"}",0);
  assert(amigosperfis_adicionar(2));tratarPerfisConta(NULL);assert(vincEstado==REC_VINC_FALHA);
  reset();assert(cursor==total);SDL_DestroyMutex(mtx);
  puts("PASS: legacy-server account-profile listing, selected code handshake, primary/secondary identity, existing friends, Unicode, stale-session cancellation, deletion, blocked/refused links and malformed identities/codes");
}
