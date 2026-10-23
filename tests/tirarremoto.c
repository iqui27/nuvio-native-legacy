// "Marcar como assistido" nao pode esconder a serie no Trakt (revisao 2.0.3,
// achado 3): so o "Tirar de Continuar assistindo" explicito faz o POST em
// /users/hidden/progress_watched. trakt.c REAL; a rede e interceptada.
#include "trakt.h"
#include "tirarremoto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

static int ocultos, apagados, remocoesSync;
static char ultimaUrlPost[160];

char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback"))
    return strdup("[{\"id\":101,\"progress\":20,\"paused_at\":\"2026-03-01T10:00:00.000Z\",\"type\":\"episode\","
      "\"episode\":{\"season\":1,\"number\":8,\"title\":\"E\",\"ids\":{\"trakt\":1}},"
      "\"show\":{\"title\":\"S\",\"year\":2020,\"runtime\":82,\"ids\":{\"trakt\":9,\"imdb\":\"tt12042964\"}}}]");
  return NULL;
}
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  if (strstr(u, "/sync/playback")) { if (st) *st = 200; return rede_baixar_com(u, 0, NULL); }
  if (st) *st = 0;
  return NULL;
}
char *rede_apagar(const char *url, int s, const char *const *cab, int *st) {
  (void)url; (void)s; (void)cab;
  apagados++;
  if (st) *st = 204;
  return strdup("");
}
char *rede_postar_st(const char *url, int s, const char *const *cab, const char *corpo, int *st) {
  (void)s; (void)cab; (void)corpo;
  if (strstr(url, "/users/hidden/progress_watched")) {
    ocultos++;
    snprintf(ultimaUrlPost, sizeof ultimaUrlPost, "%s", url);
  }
  if (st) *st = 201;
  return strdup("{}");
}
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }
int syncprog_remover(const char *chave) { (void)chave; remocoesSync++; return 1; }
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

static int falhas;
static void confere(const char *d, int ok) {
  printf("  %-72s %s\n", d, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

int main(void) {
  static CatItem v[4];
  trakt_definir("token", "client");
  trakt_continuar(v, 4);   // aprende os ids de playback (o DELETE precisa deles)
  // Marcar como assistido: remove a retomada, NAO esconde a serie.
  tirarremoto_executar("tt12042964:1:8", "tt12042964:1:8", 0);
  confere("marcar como visto: nenhum POST em /users/hidden/progress_watched", ocultos == 0);
  confere("marcar como visto: a pausa do Trakt foi apagada", apagados >= 1);
  confere("marcar como visto: syncprog_remover chamado", remocoesSync == 1);
  // "Tirar de Continuar assistindo": esconde.
  tirarremoto_executar("tt12042964:1:8", "tt12042964:1:8", 1);
  confere("tirar de Continuar: o POST de ocultar existe (1)", ocultos == 1);
  confere("tirar de Continuar: URL e progress_watched (sem /remove)",
          strstr(ultimaUrlPost, "/users/hidden/progress_watched") && !strstr(ultimaUrlPost, "/remove"));
  confere("tirar de Continuar: syncprog_remover chamado de novo", remocoesSync == 2);
  printf("%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
