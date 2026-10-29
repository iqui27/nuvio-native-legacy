// Offline #610 regression: real provider readers, fixture pages, deterministic clock.
#if defined(I610_AUTH)
#define main previous_main
#include "simklauth_perfil.c"
#undef main
int main(void) {
  assert(!previous_main());
  dados_gravar("simkl-p1.txt", "fixture-token");
  dados_gravar("simkl-p2.txt", "other-profile");
  assert(simklauth_carregar_perfil(1));
  unsigned long long generation = simklauth_credencial_copiar(NULL, 0);
  simklauth_desconectar();
  assert(!simklauth_token()[0] && simklauth_estado() == SMK_PARADO);
  assert(simklauth_credencial_copiar(NULL, 0) != generation);
  assert(contem("simkl-p1.txt", "!"));
  assert(!simklauth_carregar_perfil(1));
  assert(simklauth_carregar_perfil(2) && !strcmp(simklauth_token(), "other-profile"));
  puts("#610 Simkl disconnect clears token, persists, preserves other profiles: PASS");
  return 0;
}
#elif defined(I610_RESUME)
int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
#include "stub_perfisessao.h"
#define main previous_main
#include "trakt_arranque.c"
#undef main
#include "rede.h"
#include "traktauth.h"
#include "ajustes.h"
#include "js.h"
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r,0,sizeof *r); r->erro=REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); }
static pthread_mutex_t progress_lock = PTHREAD_MUTEX_INITIALIZER;
static int progress_active, progress_peak, progress_requests;
char *rede_baixar_st_retry(const char *u,int s,const char *const *h,int *st,int *retry) {
  *retry=0;
  if (strstr(u,"/sync/watched/")) return rede_baixar_st(u,s,h,st);
  int progress = strstr(u,"/progress/watched") != NULL;
  if (progress) {
    pthread_mutex_lock(&progress_lock);
    progress_requests++;
    if (++progress_active > progress_peak) progress_peak = progress_active;
    pthread_mutex_unlock(&progress_lock);
  }
  char *body=rede_baixar_com(u,s,h); *st=body?200:0;
  if (progress) {
    pthread_mutex_lock(&progress_lock);
    progress_active--;
    pthread_mutex_unlock(&progress_lock);
  }
  return body;
}
int main(void) {
  const char *latencies[]={"L_PLAYBACK","L_HISTORICO","L_ATIV","L_PROX","L_SOCIAL","L_CINEMETA","L_FILMES"};
  for (int i=0;i<7;i++) setenv(latencies[i],"0",1);
  setenv("L_PROX","600",1);
  assert(trakt_definir("fixture","client"));
  CatItem *v=calloc(12,sizeof *v); assert(v);
  double start = agoraMs();
  int n=trakt_continuar(v,12); assert(n>2);
  double elapsed = agoraMs() - start;
  printf("#610 startup: %d progress requests, peak %d workers, %.0f ms\n",
         progress_requests, progress_peak, elapsed);
  if (progress_requests != N_SERIES || progress_peak < 2 || progress_peak > 6 || elapsed >= 6000) {
    fprintf(stderr,"FAIL: 21 requests at 600 ms must overlap within six workers and finish under 6000 ms\n");
    free(v); return 1;
  }
  assert(v[0].retomadoMs > js_ms_iso("2026-01-01T00:00:00Z"));
  for(int i=1;i<n;i++) assert(v[i-1].retomadoMs>=v[i].retomadoMs);
  trakt_esquecer(); assert(!trakt_ativo());
  const char *headers[4]; char aut[200], key[140];
  assert(!trakt_cabecalhos(headers,aut,sizeof aut,key,sizeof key));
  free(v); puts("#610 Trakt resume order and local credential cleanup: PASS"); return 0;
}
#elif defined(I610_ORDER)
#include "cwordem.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  // Input already sorted by last watched: next episode, paused movie, older next episode.
  CwoItem v[] = {{1,CWO_SEM_DATA}, {0,CWO_SEM_DATA}, {1,CWO_SEM_DATA}};
  int order[3];
  assert(cwo_ordenar(v,3,CWO_PADRAO,0,order) == 3);
  assert(order[0] == 0 && order[1] == 1 && order[2] == 2);
  assert(cwo_ordenar(v,3,CWO_STREAMING,0,order) == 3 && order[0] == 1);
  puts("#610 home keeps last-watched order; explicit streaming mode preserved: PASS");
  return 0;
}
#else
#ifdef I610_TRAKT
#include "stub_perfisessao.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static time_t now = 1000;
static time_t fixture_time(time_t *out) { if (out) *out = now; return now; }
#define time fixture_time
#ifdef I610_TRAKT
#include "../src/trakt.c"
unsigned long long cat_historico_geracao(void) { return 1; }
int cat_historico_definir_se_geracao(const char *i, const char *t, int v, unsigned long long g) {
  (void)i; (void)t; (void)v; (void)g; return 1;
}
const char *i18n(const char *s) { return s; }
#else
#include "../src/simkl.c"
#define main previous_main
#define rede_baixar_st previous_get
#define atividades fixture_activities
#define rede_pedir previous_request
#define rede_resposta_limpar previous_response_clear
#define rede_baixar_st_retry previous_retry
#include "simkl_cw.c"
#undef rede_baixar_st_retry
#undef rede_resposta_limpar
#undef rede_pedir
#undef atividades
#undef rede_baixar_st
#undef main
#endif
#undef time
static int requests, limited, empty_retry, bad_page;
static char *fixture(const char *name) {
  char path[160]; snprintf(path, sizeof path, "tests/fixtures/i610/%s", name);
  FILE *f = fopen(path, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); rewind(f);
  char *s = calloc((size_t)n + 1, 1); assert(s);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f); return s;
}
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  memset(r, 0, sizeof *r); requests++;
  assert(p->cabecalhos && p->cabecalhos[0]);
  if (limited) {
    r->status = 429; r->retry_after_s = empty_retry ? 0 : 90;
    r->cabecalhos = fixture("429.headers"); r->corpo = strdup("{}"); return 1;
  }
  r->status = 200;
  int page = 1; const char *q = strstr(p->url, "page="); if (q) page = atoi(q + 5);
  if (page != 1 && page != 3) { bad_page++; r->status = 404; r->corpo = strdup("{}"); return 1; }
#ifdef I610_TRAKT
  int list = strstr(p->url, "/movies/") != NULL;
#else
  if (strstr(p->url, "/sync/playback") || strstr(p->url, "/watching")) {
    r->corpo = previous_get(p->url, 25, p->cabecalhos, &r->status); return 1;
  }
  int list = strstr(p->url, "/movies/plantowatch") != NULL;
  if (strstr(p->url, "/sync/activities")) {
    r->corpo = strdup("{\"movies\":\"changed\"}"); return 1;
  }
#endif
  if (!list) {
#ifdef I610_TRAKT
    r->corpo = strdup("[]");
#else
    r->corpo = strdup(strstr(p->url, "/anime/") ? "{\"anime\":[]}" : "{\"shows\":[]}");
#endif
    return 1;
  }
  char name[64];
#ifdef I610_TRAKT
  snprintf(name, sizeof name, "trakt-%d.json", page);
#else
  snprintf(name, sizeof name, "simkl-%d.json", page);
#endif
  r->corpo = fixture(name);
  r->cabecalhos = fixture(page == 1 ? "next.headers" : "last.headers");
  return 1;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); memset(r, 0, sizeof *r); }
char *rede_baixar_st_retry(const char *u, int s, const char *const *c, int *st, int *ra) {
  (void)s; RedePedido p = {.url=u, .cabecalhos=c}; RedeResposta r;
  rede_pedir(&p, &r); *st=r.status; *ra=r.retry_after_s;
  free(r.cabecalhos); return r.corpo;
}
#ifdef I610_TRAKT
char *rede_baixar_com(const char *u, int s, const char *const *c) {
  int st, ra; return rede_baixar_st_retry(u,s,c,&st,&ra);
}
#else
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  int ra; return rede_baixar_st_retry(u,s,c,st,&ra);
}
#endif
static int load(CatItem *v) {
#ifdef I610_TRAKT
  return trakt_lista("watchlist", v, 100);
#else
  simkl_esquecer(); return simkl_plantowatch(v, 100);
#endif
}
int main(void) {
  CatItem *v = calloc(100, sizeof *v); assert(v);
#ifdef I610_TRAKT
  ligado = 1; strcpy(token, "fixture"); strcpy(cliente, "client");
#else
  tokenAtual = TOKEN;
#endif
  int n = load(v);
  if (n != 17 || bad_page) { fprintf(stderr, "FAIL: pagination loaded %d/17, wrong pages %d\n", n, bad_page); return 1; }
  assert(!strcmp(v[16].imdb, "tt0000017"));
#if defined(I610_TRAKT) && defined(NV_PROVEDORHTTP_H)
  assert(provedorProxima("x-pagination-page-count: 2\r\n",1) == 2);
  assert(provedorProxima("X-Pagination-Page-Count: 2\r\n",2) == 0);
  assert(provedorProxima(NULL,1) == -1);
#elif !defined(I610_TRAKT)
  assert(simkl_na_plantowatch("tt0000017"));
  simkl_esquecer(); assert(!simkl_na_plantowatch("tt0000017"));
#endif
  limited = 1; load(v); int before = requests;
  load(v); assert(requests == before);
  now += 89; load(v); assert(requests == before);
  now++; empty_retry = 1; load(v); assert(requests == before + 1);
  before = requests; now += 9; load(v); assert(requests == before);
  now++; load(v); assert(requests == before + 1);
  before = requests; now += 19; load(v); assert(requests == before);
  now++; limited = 0; assert(load(v) == 17);
#ifndef I610_TRAKT
  // A newly watched next episode must precede an older paused movie.
  simkl_esquecer();
  CatItem ordered[12];
  int count = simkl_continuar(ordered, 12); assert(count == 6);
  assert(!strncmp(ordered[0].imdb, "tt22248376", 10));
  for (int i = 1; i < count; i++) assert(ordered[i-1].retomadoMs >= ordered[i].retomadoMs);
#endif
  free(v); puts("#610 fixture pagination and shared 429 backoff: PASS"); return 0;
}
#endif
