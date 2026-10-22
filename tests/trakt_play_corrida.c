// Revisao 2.0.3: trakt_playback_remover (tirarremoto.c, fio do menu) le e apaga
// play[]/nPlay sem trava, enquanto trakt_continuar (fio da sincronizacao) zera
// nPlay e reescreve play[] no mesmo instante. Corrida de dados: o remover podia
// ler uma chave pela metade, apagar o id errado ou zerar a linha que o outro
// fio acabou de gravar. Rodado com -fsanitize=thread: qualquer relato de
// corrida falha o teste.
#include "trakt.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

static _Atomic int apagados, cinemetaGets;

static const char *registro(int id, const char *paused, int pct) {
  static char b[8][1400];
  static int k;
  char *o = b[k++ & 7];
  snprintf(o, 1400,
    "{\"id\":%d,\"progress\":%d,\"paused_at\":\"%s\",\"type\":\"episode\","
    "\"episode\":{\"season\":1,\"number\":8,\"title\":\"Ep\",\"ids\":{\"trakt\":1}},"
    "\"show\":{\"title\":\"The World of the Married\",\"year\":2020,\"runtime\":82,"
    "\"overview\":\"Sinopse.\",%s\"ids\":{\"trakt\":9,\"imdb\":\"tt12042964\"},"
    "\"images\":{\"poster\":[\"x.test/p.jpg\"],\"fanart\":[\"x.test/f.jpg\"]}}}",
    id, pct, paused, getenv("COM_CERT") ? "\"certification\":\"TV-MA\"," : "");
  return o;
}

char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) {
    char *o = malloc(9000);
    snprintf(o, 9000, "[%s,%s,%s,%s,%s,{\"id\":77,\"progress\":40,\"paused_at\":\"2026-01-01T00:00:00.000Z\","
      "\"type\":\"movie\",\"movie\":{\"title\":\"Outro\",\"year\":2019,\"runtime\":100,\"overview\":\"S.\","
      "\"ids\":{\"imdb\":\"tt7777777\"},\"images\":{\"poster\":[\"x.test/p2.jpg\"],\"fanart\":[\"x.test/f2.jpg\"]}}}]",
      registro(101, "2026-03-01T10:00:00.000Z", 20), registro(102, "2026-03-05T10:00:00.000Z", 30),
      registro(103, "2026-03-02T10:00:00.000Z", 25), registro(104, "2026-03-04T10:00:00.000Z", 22),
      registro(105, "2026-03-03T10:00:00.000Z", 21));
    return o;
  }
  return NULL;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "cinemeta")) {
    cinemetaGets++;
    return strdup(getenv("SEM_NOTA") ? "{\"name\":\"x\"}" : "{\"imdbRating\":\"7.9\",\"name\":\"x\"}");
  }
  return NULL;
}
char *rede_apagar(const char *url, int s, const char *const *cab, int *st) {
  (void)s; (void)cab;
  (void)url;
  apagados++;
  if (st) *st = 204;
  return strdup("");
}
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }

char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; if (st) *st = 0; return NULL;
}
int cwo_conta_a_seguir(const char *id) { (void)id; return 0; }
void cwo_conta_trocar(const char *a, const char *b) { (void)a; (void)b; }
void cwo_marcar_estreia(const char *id, long long ms) { (void)id; (void)ms; }
int cwo_virada_aceita(long long e, long long a) { (void)e; (void)a; return 1; }
int ajustes_tmdb_cw(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "en"; }
const char *i18n(const char *s) { return s; }
unsigned long long cat_historico_geracao(void) { return 1; }
int cat_historico_definir_se_geracao(const char *a, const char *b, int c, unsigned long long g) {
  (void)a; (void)b; (void)c; (void)g; return 1;
}


#define VOLTAS 200
static void *fioContinuar(void *u) {
  static CatItem v[12];
  (void)u;
  for (int i = 0; i < VOLTAS; i++) { memset(v, 0, sizeof v); trakt_continuar(v, 12); }
  return NULL;
}
static void *fioRemover(void *u) {
  (void)u;
  for (int i = 0; i < VOLTAS * 4; i++) {
    trakt_playback_remover("tt12042964:1:8");
    trakt_playback_remover("tt7777777");
  }
  return NULL;
}

int main(void) {
  static CatItem v[12];
  pthread_t a, b;
  trakt_definir("token", "client");
  trakt_continuar(v, 12);            // aquece os caches de uma vez (Cinemeta etc.)
  pthread_create(&a, NULL, fioContinuar, NULL);
  pthread_create(&b, NULL, fioRemover, NULL);
  pthread_join(a, NULL);
  pthread_join(b, NULL);
  printf("apagados=%d\n", (int)apagados);
  puts("trakt_play_corrida: PASS");
  return 0;
}
