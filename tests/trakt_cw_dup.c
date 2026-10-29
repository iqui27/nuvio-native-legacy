// #244 / #243: /sync/playback com cinco registros da mesma obra vira UM card, a
// remocao apaga os cinco registros, o "14" inventado nao existe, e o selo IMDb
// chega pelo Cinemeta mesmo com arte+sinopse vindas do Trakt.
#include "trakt.h"
#include "stub_perfisessao.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

extern int trakt_operacao_estado(int tipo);

static int apagados, cinemetaGets, temporada556 = 1;
static char urlsApagadas[16][96];

static const char *registro(int id, const char *paused, int pct) {
  static char b[8][1400];
  static int k;
  char *o = b[k++ & 7];
  char temporada[40] = "";
  if (temporada556 >= 0) snprintf(temporada, sizeof temporada, "\"season\":%d,", temporada556);
  snprintf(o, 1400,
    "{\"id\":%d,\"progress\":%d,\"paused_at\":\"%s\",\"type\":\"episode\","
    "\"episode\":{%s\"number\":8,\"title\":\"Ep\",\"ids\":{\"trakt\":1}},"
    "\"show\":{\"title\":\"The World of the Married\",\"year\":2020,\"runtime\":82,"
    "\"overview\":\"Sinopse.\",%s\"ids\":{\"trakt\":9,\"imdb\":\"tt12042964\"},"
    "\"images\":{\"poster\":[\"x.test/p.jpg\"],\"fanart\":[\"x.test/f.jpg\"]}}}",
    id, pct, paused, temporada, getenv("COM_CERT") ? "\"certification\":\"TV-MA\"," : "");
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
  if (apagados < 16) snprintf(urlsApagadas[apagados], 96, "%s", url);
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

int main(void) {
  static CatItem v[12];
  int n, i;
  trakt_definir("token", "client");
  n = trakt_continuar(v, 12);
  printf("n=%d\n", n);
  // duas obras: a serie (uma vez) e o filme
  assert(n == 2);
  assert(!strcmp(v[0].imdb, "tt12042964:1:8") || !strcmp(v[1].imdb, "tt12042964:1:8"));
  { int s = !strcmp(v[0].imdb, "tt12042964:1:8") ? 0 : 1;
    // fica o registro mais novo (id 102, 30%)
    assert(v[s].progresso == 30);
    // #243: sem certification no bloco nao ha "14"; com ela, o valor real
    if (getenv("COM_CERT")) assert(!strcmp(v[s].classificacao, "TV-MA"));
    else assert(v[s].classificacao[0] == 0);
    // #243: nota 7.9 do Cinemeta apesar de arte+sinopse ja vindas do Trakt
    if (getenv("SEM_NOTA")) assert(v[s].nota == 0);
    else assert(v[s].nota == 79);
    assert(cinemetaGets >= 1);
    printf("nota=%d classificacao='%s'\n", v[s].nota, v[s].classificacao); }
  for (i = 0; i < n; i++) assert(strcmp(v[i].classificacao, "14"));
  // #244: um pedido de remocao apaga os CINCO registros
  assert(trakt_playback_remover("tt12042964:1:8") == 1);
  assert(apagados == 5);
  for (i = 0; i < 5; i++) assert(strstr(urlsApagadas[i], "/sync/playback/10"));
  // e uma segunda vez nao ha mais o que apagar
  assert(trakt_playback_remover("tt12042964:1:8") == 0 && apagados == 5);
  // Cinemeta sem nota (ou que falha) e tentado UMA vez na sessao, nao a cada refacao.
  if (getenv("SEM_NOTA")) {
    int antes = cinemetaGets;
    static CatItem w[2];
    memset(w, 0, sizeof w);
    n = trakt_continuar(w, 2);
    assert(cinemetaGets == antes);   // ja tentado na chamada de cima
    n = trakt_enfeitar_lote(w, n);
    assert(cinemetaGets == antes);
  }
  // #556: a retomada e a remocao usam o MESMO tt:0:E, nunca tt:1:E.
  temporada556 = 0; apagados = 0;
  trakt_esquecer(); trakt_definir("token-s00", "client");
  n = trakt_continuar(v, 12); assert(n == 2);
  { int s = v[0].episodio > 0 ? 0 : 1;
    assert(v[s].temporada == 0 && v[s].episodio == 8);
    assert(!strcmp(v[s].imdb, "tt12042964:0:8"));
    assert(!strcmp(v[1 - s].imdb, "tt7777777") && !strcmp(v[1 - s].tipo, "movie")); }
  assert(trakt_playback_remover("tt12042964:0:8") == 1 && apagados == 5);
  assert(trakt_playback_remover("tt12042964:0:8") == 0 && apagados == 5);
  temporada556 = -1; // campo ausente conserva o fallback regular, nao vira S00
  trakt_esquecer(); trakt_definir("token-sem-temporada", "client");
  n = trakt_continuar(v, 12); assert(n == 2);
  { int s = v[0].episodio > 0 ? 0 : 1;
    assert(v[s].temporada == 1 && v[s].episodio == 8);
    assert(!strcmp(v[s].imdb, "tt12042964:1:8")); }
  puts("trakt_cw_dup: PASS");
  return 0;
}
