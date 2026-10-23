#include "trakt.h"
#include "traktscrobble.h"
#include "metaprov.h"
#include "stub_perfisessao.h"
#include <math.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

extern int trakt_operacao_estado(int tipo);
extern int trakt_watchlist_tipo(const char *imdb, const char *tipo, int adicionar);
extern int trakt_assistido_tipo(const char *imdb, const char *tipo, int marcar);

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static char ultimaUrl[160], ultimoCorpo[320];
static int chamadas, proximoStatus, bloquear, liberar;
static unsigned long long geracaoMapa = 1;
static int marcas;

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  (void)segundos; (void)cab;
  pthread_mutex_lock(&lock);
  snprintf(ultimaUrl, sizeof ultimaUrl, "%s", url);
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo);
  chamadas++;
  if (status) *status = proximoStatus;
  pthread_cond_broadcast(&cond);
  while (bloquear && !liberar) pthread_cond_wait(&cond, &lock);
  pthread_mutex_unlock(&lock);
  return strdup("");
}

int cat_indice_por_imdb(const char *imdb) { return !strcmp(imdb, "tt1234567") ? 0 : -1; }
void cat_definir_na_lista(int indice, int naLista) { (void)indice; (void)naLista; }
const char *cat_tipo_por_imdb(const char *imdb) { (void)imdb; return "movie"; }
void cat_historico_definir_id(const char *imdb, const char *tipo, int visto) {
  (void)imdb; (void)tipo; (void)visto;
  marcas++;
}

unsigned long long cat_historico_geracao(void) {
  return __atomic_load_n(&geracaoMapa, __ATOMIC_ACQUIRE);
}
int cat_historico_definir_se_geracao(const char *id, const char *tipo, int visto,
                                     unsigned long long geracao) {
  if (geracao != cat_historico_geracao()) return 0;
  cat_historico_definir_id(id, tipo, visto);
  return 1;
}
void rede_avisar_401(void (*callback)(const char *url)) { (void)callback; }

// These contracts use movies only; episode metadata must never be requested.
const char *metaprov_idioma(void) { return "en-US"; }
char *metaprov_meta(const char *tipo, const char *id, int seg, int *prov) {
  (void)tipo; (void)id; (void)seg; (void)prov;
  assert(!"unexpected episode metadata request");
  return NULL;
}

static void espera(int esperado) {
  pthread_mutex_lock(&lock);
  while (chamadas < esperado) pthread_cond_wait(&cond, &lock);
  pthread_mutex_unlock(&lock);
}

int main(void) {
  trakt_definir("token", "client");

  // Nenhum desses pedidos pode ocupar a vaga pendente ou tocar a rede.
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 129, 129, 0, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 129, 129, 0.5, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 30, 30, 30, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", NAN, 2580, 2580, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 2580, 2580, NAN, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_TOCANDO, "tt1234567", 10, 2580, 0, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 2768, 2843, 0, 0) == SCR_NADA);
  assert(chamadas == 0);

  proximoStatus = 400;
  assert(trakt_watchlist_tipo("tt1234567:2:4", "series", 1));
  espera(1);
  while (trakt_operacao_estado(1) == 1) {}
  assert(trakt_operacao_estado(1) == 3);
  assert(strstr(ultimaUrl, "/sync/watchlist"));
  assert(strstr(ultimoCorpo, "shows") && !strstr(ultimoCorpo, "movies"));

  proximoStatus = 204;
  while (!trakt_watchlist_tipo("tt1234567", "series", 1)) sched_yield();
  espera(2);
  while (trakt_operacao_estado(1) == 1) {}
  assert(trakt_operacao_estado(1) == 2);

  proximoStatus = 201;
  while (!trakt_assistido_tipo("tt7654321", "movie", 1)) sched_yield();
  espera(3);
  while (trakt_operacao_estado(2) == 1) {}
  assert(trakt_operacao_estado(2) == 2);
  assert(strstr(ultimaUrl, "/sync/history"));
  assert(strstr(ultimoCorpo, "movies") && !strstr(ultimoCorpo, "shows"));
  assert(marcas == 1);
  bloquear = 1; liberar = 0;
  while (!trakt_assistido_tipo("tt7654321", "movie", 0)) sched_yield();
  espera(4);
  __atomic_add_fetch(&geracaoMapa, 1, __ATOMIC_RELEASE);
  pthread_mutex_lock(&lock); liberar = 1; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&lock);
  while (trakt_operacao_estado(2) == 1) sched_yield();
  assert(trakt_operacao_estado(2) == 3 && marcas == 1);
  bloquear = 1; liberar = 0;
  while (!trakt_assistido_tipo("tt7654321", "movie", 1)) sched_yield();
  espera(5);
  trakt_esquecer();
  pthread_mutex_lock(&lock); liberar = 1; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&lock);
  while (trakt_operacao_estado(2) == 1) sched_yield();
  assert(trakt_operacao_estado(2) == 3 && marcas == 1);
  bloquear = 1; liberar = 0;
  trakt_definir("token", "client");
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 2580, 2580, 2322, 1) == SCR_STOP);
  espera(6);
  assert(strstr(ultimaUrl, "/scrobble/stop"));
  assert(strstr(ultimoCorpo, "100.00"));
  // Invalid playback remains rejected; a played seek may queue a stop.
  assert(trakt_scrobble(SCR_EV_SAIU, "tt9999999", 180, 180, 0, 0) == SCR_NADA);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt9999999", 2580, 2580, 120, 0) == SCR_STOP);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 200, 2580, 200, 0) == SCR_PAUSE);
  pthread_mutex_lock(&lock); liberar = 1; pthread_cond_broadcast(&cond); pthread_mutex_unlock(&lock);
  espera(7);
  assert(strstr(ultimaUrl, "/scrobble/pause"));
  assert(!strstr(ultimoCorpo, "tt9999999"));
  // A seek completion ends the active start; reopening must send a new start.
  assert(trakt_scrobble(SCR_EV_TOCANDO, "tt7777777", 99, 2843, 99, 0) == SCR_START);
  espera(8);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt7777777", 2824, 2843, 133, 0) == SCR_STOP);
  espera(9);
  assert(strstr(ultimaUrl, "/scrobble/stop"));
  assert(trakt_scrobble(SCR_EV_TOCANDO, "tt7777777", 10, 2843, 10, 0) == SCR_START);
  espera(10);
  assert(strstr(ultimaUrl, "/scrobble/start"));
  // Com a decisao do player (player_regra_visto), o stop sai.
  assert(trakt_scrobble(SCR_EV_SAIU, "tt7777777", 2700, 2700, 2400, 1) == SCR_STOP);
  espera(11);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt8888888", 6000, 6000, 2300, 1) == SCR_STOP);
  espera(12);
  assert(strstr(ultimaUrl, "/scrobble/stop") && strstr(ultimoCorpo, "tt8888888"));
  // Exit accepts one second of playback; completion begins at 80%, even after seeking.
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 129, 129, 69, 0) == SCR_STOP);
  espera(13);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 2768, 2843, 133, 0) == SCR_STOP);
  espera(14);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 85, 1000, 1, 0) == SCR_PAUSE);
  espera(15);
  assert(trakt_scrobble(SCR_EV_SAIU, "tt1234567", 850, 1000, 1, 0) == SCR_STOP);
  espera(16);
  puts("trakt contracts: PASS (intent, scope, failure, retry, seek completion and playback guards)");
  return 0;
}
