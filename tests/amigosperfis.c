#include "amigosperfis.h"
#include "recomenda.h"
#include "js.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static SDL_mutex *mtx;
static int recPerfil=1, vincEstado, social=1, atual=1;
static unsigned geracao=1;
static char meuId[96]="nuvio:account-a", vincNome[64], fioUrl[128], enviado[64];
static int stResposta=200, leituras, adicoes, recargas, invalidar;
static const char *resposta="{\"perfis\":[{\"indice\":2,\"id\":\"nuvio:account-a:2\",\"nome\":\"Alice\",\"estado\":\"disponivel\"},"
  "{\"indice\":3,\"id\":\"nuvio:account-a:3\",\"nome\":\"Bob\",\"estado\":\"amigo\"},"
  "{\"indice\":1,\"id\":\"nuvio:account-a\",\"nome\":\"Parent\",\"estado\":\"disponivel\"},"
  "{\"indice\":2,\"id\":\"duplicate\",\"nome\":\"Duplicate\",\"estado\":\"disponivel\"}]}";
static void limparPerfisConta(void);
int recomenda_ativo(void) { return social; }
void recomenda_pedir_agora(void) {}
static int recTravarFio(void) { if(!atual)return 0;SDL_LockMutex(mtx);return 1; }
static void recDestravarFio(void) { SDL_UnlockMutex(mtx); }
static int bearerSessaoLocal(char *s,size_t n) { snprintf(s,n,"Authorization: Bearer test-token");return 1; }
static int perfilCab(char *s,size_t n,int nuvio) { assert(nuvio);snprintf(s,n,"X-Nuvio-Perfil: %d",recPerfil);return 1; }
static char *nuvem_rpc_com(const char *f,const char *body,const char *token,int *st) {
  (void)f;(void)body;(void)token;*st=503;return NULL;
}
static void jsonEsc(char *s,size_t n,const char *v) { snprintf(s,n,"%s",v); }
static void url(const char *p) { snprintf(fioUrl,sizeof fioUrl,"https://social.test%s",p); }
static char *response(const char *u,const char **cab,int *st) {
  assert(!strcmp(u,"https://social.test/v1/contatos/perfis"));
  assert(!strcmp(cab[0],"Authorization: Bearer test-token") && !strcmp(cab[1],"X-Nuvio-Auth: nuvio"));
  assert(cab[2] && !cab[3]);*st=stResposta;
  if(invalidar){limparPerfisConta();geracao++;invalidar=0;}
  size_t n=strlen(resposta)+1;char *s=malloc(n);assert(s);memcpy(s,resposta,n);return s;
}
static char *recBaixar(const char *u,int t,const char **cab,int *st) { (void)t;leituras++;return response(u,cab,st); }
static char *recPostar(const char *u,int t,const char **cab,const char *body,int *st) {
  (void)t;adicoes++;snprintf(enviado,sizeof enviado,"%s",body);return response(u,cab,st);
}
static void lerContatos(const char **cab) { (void)cab;recargas++; }
#define REC_TEMPO_REDE 8
#include "../src/recomenda_perfis.inc"
int main(void) {
  mtx=SDL_CreateMutex();assert(mtx);AmigoPerfil a[CONTA_PERFIL_MAX];
  assert(amigosperfis_carregar() && !amigosperfis_carregar());
  assert(amigosperfis_estado()==AP_CARREGANDO && !amigosperfis_adicionar(2));
  tratarPerfisConta(NULL);
  assert(leituras==1 && amigosperfis_estado()==AP_PRONTO && amigosperfis_listar(a,8)==2);
  assert(a[0].perfil.indice==2 && a[0].estado==AP_DISPONIVEL && a[1].estado==AP_AMIGO);
  assert(!amigosperfis_adicionar(1) && !amigosperfis_adicionar(3) && !amigosperfis_adicionar(9));
  assert(amigosperfis_adicionar(2) && !amigosperfis_adicionar(2));
  resposta="{\"ok\":1,\"contato\":{\"id\":\"nuvio:account-a:2\",\"nome\":\"Alice\"}}";
  tratarPerfisConta(NULL);
  assert(adicoes==1 && !strcmp(enviado,"{\"perfil\":2}") && !strstr(enviado,"codigo"));
  assert(vincEstado==REC_VINC_OK && !strcmp(vincNome,"Alice") && recargas==1);
  assert(amigosperfis_listar(a,8)==2 && a[0].estado==AP_AMIGO && !amigosperfis_adicionar(2));
  limparPerfisConta();vincEstado=0;assert(amigosperfis_carregar());stResposta=404;tratarPerfisConta(NULL);
  assert(amigosperfis_estado()==AP_ERRO && !amigosperfis_listar(a,8));
  assert(amigosperfis_carregar());stResposta=503;tratarPerfisConta(NULL);assert(amigosperfis_estado()==AP_ERRO);
  assert(amigosperfis_carregar());stResposta=200;resposta="{}";tratarPerfisConta(NULL);assert(amigosperfis_estado()==AP_ERRO);
  assert(amigosperfis_carregar());
  resposta="{\"perfis\":[{\"indice\":1e300},{\"indice\":-1e300},{\"indice\":2.5},"
    "{\"indice\":3,\"id\":\"nuvio:account-a:3\",\"nome\":\"Çağrı\",\"estado\":\"disponivel\"}]}";
  recPerfil=2;tratarPerfisConta(NULL);
  assert(amigosperfis_listar(a,8)==1 && a[0].perfil.indice==3 && !strcmp(a[0].perfil.nome,"Çağrı"));
  recPerfil=1;
  assert(amigosperfis_carregar());invalidar=1;tratarPerfisConta(NULL);
  assert(amigosperfis_estado()==AP_NADA && !amigosperfis_listar(a,8));
  strcpy(meuId,"trakt:unlinked");assert(amigosperfis_carregar());int antes=leituras;
  tratarPerfisConta(NULL);assert(amigosperfis_estado()==AP_CONTA_NUVIO && leituras==antes);
  social=0;assert(!amigosperfis_carregar() && !amigosperfis_listar(a,8));
  assert(!amigosperfis_listar(NULL,8) && !amigosperfis_listar(a,0));
  SDL_DestroyMutex(mtx);
  puts("PASS: direct account-profile selection, no-code payload, duplicates, self exclusion, account auth, service errors and stale-session discard");
}
