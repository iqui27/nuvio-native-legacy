// livecat: secao sugerida para canal sem categoria. Rede trocada por um poster
// de mentira (Worker); o disco e a pasta NUVIO_DADOS do script.
#include "../src/livecat.h"
#include "../src/dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static time_t clockNow = 1800000000;
time_t time(time_t *out) { if (out) *out = clockNow; return clockNow; }

static long monotonicMs;
static int slowNetwork, largestTimeout;
int clock_gettime(clockid_t clock, struct timespec *out) {
  (void)clock;
  out->tv_sec = monotonicMs / 1000;
  out->tv_nsec = (monotonicMs % 1000) * 1000000;
  return 0;
}
static char *worker(const char *url, const char *corpo, int *st);
char *rede_postar_st(const char *u, int s, const char *const *c, const char *b, int *st) {
  (void)c;
  if (!slowNetwork) { fprintf(stderr, "unexpected real network\n"); exit(2); }
  if (s > largestTimeout) largestTimeout = s;
  monotonicMs += 1200; // Successful but slow; no wall-clock sleep.
  return worker(u, b, st);
}

static int falhas, posts, status = 200, maiorLote, vazou;
static char ultimoCorpo[8192];

#define OK(c, msg) do { if (!(c)) { printf("FALHOU: %s\n", msg); falhas++; } else printf("ok: %s\n", msg); } while (0)

// Worker de mentira: "futebol" -> Esportes, "kids" -> Infantil, "xyzzy" -> null,
// "weird" -> secao que o app nao conhece, "mudo" -> ausente da resposta.
static char *worker(const char *url, const char *corpo, int *st) {
  const char *p = strstr(corpo, "[");
  char *r = malloc(16384);
  size_t u = 0;
  int n = 0;
  posts++;
  snprintf(ultimoCorpo, sizeof ultimoCorpo, "%s", corpo);
  if (strcmp(url, "http://rec.test/livetv/categorias")) vazou = 1;
  if (strstr(corpo, "http") || strstr(corpo, "token")) vazou = 1;
  *st = status;
  if (strstr(corpo, "\\\"")) {
    free(r);
    return strdup("{\"cats\":{\"canal \\\"futebol\\\"\":\"Esportes\"}}");
  }
  u += (size_t)snprintf(r, 16384, "{\"cats\":{");
  while (p && (p = strchr(p, '"'))) {
    const char *q = strchr(p + 1, '"');
    char b[96];
    const char *v;
    size_t m = (size_t)(q - p - 1);
    memcpy(b, p + 1, m); b[m] = 0;
    p = q + 1; n++;
    if (strstr(b, "mudo")) continue;
    v = strstr(b, "futebol") ? "\"Esportes\"" : strstr(b, "kids") ? "\"Infantil\"" :
        strstr(b, "weird") ? "\"Hacker\"" : "null";
    u += (size_t)snprintf(r + u, 16384 - u, "%s\"%s\":%s", u > 12 ? "," : "", b, v);
  }
  if (n > maiorLote) maiorLote = n;
  snprintf(r + u, 16384 - u, "}}");
  return r;
}

static int cancelada;
static int ativo(void) { return !cancelada; }

int main(int argc, char **argv) {
  (void)argv;
  dados_iniciar(NULL);
  if (argc > 1) {
    clockNow += 31 * 86400;
    const char *names[] = { "Canal Futebol HD", "xyzzy tv" }, *out[2];
    unsigned char sem[] = {1, 1};
    livecat_postar_teste(worker);
    const char *section = livecat_secao("Canal Futebol FHD", NULL);
    OK(section && !strcmp(section, "Esportes") && !livecat_secao("xyzzy tv", NULL) &&
       !livecat_secao("uncached name", NULL) && posts == 0,
       "cached-only lookup loads disk answers without requesting missing names");
    OK(livecat_sugerir(names, sem, 2, out, ativo) == 1 && posts == 0,
       "disk cache survives process restart, including null");
    return falhas ? 1 : 0;
  }
  const char *nomes[] = { "Canal Futebol HD", "Canal Futebol FHD", "ESPN HD", "Kids Club", "xyzzy tv", "weird tv", "mudo tv" };
  unsigned char sem[] = { 1, 1, 0, 1, 1, 1, 1 };
  const char *out[7];
  int ganhou, i;
  livecat_postar_teste(worker);

  ganhou = livecat_sugerir(nomes, sem, 7, out, ativo);
  OK(ganhou == 3 && posts == 1, "unclassified names get suggested sections in one post");
  OK(out[0] && !strcmp(out[0], "Esportes") && out[1] && !strcmp(out[1], "Esportes"), "HD and FHD variants share the base-name section");
  OK(out[3] && !strcmp(out[3], "Infantil"), "second section assigned");
  OK(out[2] == NULL, "classified channel (sem=0) untouched");
  OK(!strstr(ultimoCorpo, "espn"), "classified channel name never leaves the device");
  OK(out[4] == NULL, "null from Worker (low confidence) -> no suggestion");
  OK(out[5] == NULL, "section the app does not know is ignored");
  OK(out[6] == NULL, "name missing from the response -> no suggestion");
  OK(!vazou, "only the route URL and base names in the body");

  posts = 0;
  ganhou = livecat_sugerir(nomes, sem, 7, out, ativo);
  OK(posts == 1, "only the uncached name (mudo) is asked again");
  OK(strstr(ultimoCorpo, "mudo") && !strstr(ultimoCorpo, "futebol") && !strstr(ultimoCorpo, "kids"), "cache hit: answered names are not re-sent");
  OK(ganhou == 3 && out[0] && !strcmp(out[0], "Esportes"), "cached answers still returned");

  { unsigned char nada[7] = {0};
    posts = 0;
    ganhou = livecat_sugerir(nomes, nada, 7, out, ativo);
    OK(ganhou == 0 && posts == 0, "nothing unclassified -> no network"); }

  { const char *um[] = { "Rede Bomba Tv" }; unsigned char s1[] = { 1 };
    status = 500; posts = 0;
    ganhou = livecat_sugerir(um, s1, 1, out, ativo);
    OK(ganhou == 0 && out[0] == NULL && posts == 1, "HTTP 500 -> no suggestion");
    status = 200; posts = 0;
    livecat_sugerir(um, s1, 1, out, ativo);
    OK(posts == 1, "failure is not cached: next load asks again"); }

  { const char *unsafe[] = { "https://private/stream?token=secret", "www.private/list" };
    unsigned char both[] = { 1, 1 }; posts = 0;
    ganhou = livecat_sugerir(unsafe, both, 2, out, ativo);
    OK(ganhou == 0 && posts == 0, "URLs never sent as channel names"); }

  { const char *quoted[] = { "Canal \"Futebol\"" }; unsigned char one[] = {1};
    posts = 0;
    ganhou = livecat_sugerir(quoted, one, 1, out, ativo);
    OK(ganhou == 1 && posts == 1 && out[0] && !strcmp(out[0], "Esportes"), "quoted channel names round-trip as JSON"); }

  { static char nm[100][24]; const char *pn[100], *o100[100]; unsigned char s100[100];
    for (i = 0; i < 100; i++) { snprintf(nm[i], sizeof nm[i], "canal kids %03d", i); pn[i] = nm[i]; s100[i] = 1; }
    posts = 0; maiorLote = 0;
    ganhou = livecat_sugerir(pn, s100, 100, o100, ativo);
    OK(posts == 1 && maiorLote == 100 && ganhou == 100, "100 names fit one route request"); }

  { const char *um[] = { "Outro Futebol Tv" }; unsigned char s1[] = { 1 };
    cancelada = 1; posts = 0;
    ganhou = livecat_sugerir(um, s1, 1, out, ativo);
    OK(ganhou == 0 && posts == 0, "cancelled load asks nothing");
    cancelada = 0; }

  { static char nm[1200][32]; const char *pn[1201], *result[1201]; unsigned char eligible[1201];
    for (i = 0; i < 1200; i++) {
      snprintf(nm[i], sizeof nm[i], "slow kids %04d", i); pn[i] = nm[i]; eligible[i] = 1;
    }
    pn[1200] = "Canal Futebol HD"; eligible[1200] = 1;
    posts = 0; monotonicMs = 0; largestTimeout = 0; slowNetwork = 1;
    livecat_postar_teste(NULL);
    ganhou = livecat_sugerir(pn, eligible, 1201, result, ativo);
    OK(posts == 1 && monotonicMs <= 2000 && largestTimeout <= 2,
       "P2: 1200 cold names share a two-second network budget, including request timeout");
    OK(ganhou == 201 && result[0] && !result[200] && result[1200] && !strcmp(result[1200], "Esportes"),
       "budget exhaustion preserves successful and cached suggestions for the usable guide");
    slowNetwork = 0; livecat_postar_teste(worker);
  }

  // Fill all 3000 slots, then classify a new name without waiting for expiry.
  { static char names[1000][32]; const char *pn[1000], *result[1000];
    unsigned char eligible[1000];
    for (int batch = 0; batch < 3; batch++) {
      for (i = 0; i < 1000; i++) {
        snprintf(names[i], sizeof names[i], "cachefill %04d", batch * 1000 + i);
        pn[i] = names[i]; eligible[i] = 1;
      }
      livecat_sugerir(pn, eligible, 1000, result, ativo);
    }
    const char *fresh[] = {"Fresh Kids", "Another Kids"}; unsigned char both[] = {1, 1};
    posts = 0;
    ganhou = livecat_sugerir(fresh, both, 2, out, ativo);
    OK(ganhou == 2 && out[0] && !strcmp(out[0], "Infantil") &&
       out[1] && !strcmp(out[1], "Infantil"),
       "P2: full 3000-name cache returns every fresh suggestion");
    posts = 0;
    ganhou = livecat_sugerir(fresh, both, 2, out, ativo);
    OK(ganhou == 2 && posts == 0, "P2: fresh answers remain cached after eviction");
  }

  { const char *names[] = { "Canal Futebol HD", "xyzzy tv" }; unsigned char both[] = {1, 1};
    clockNow += 31 * 86400; posts = 0;
    OK(!livecat_secao("Canal Futebol HD", NULL) && posts == 0,
       "cached-only lookup rejects expired sections without network");
    ganhou = livecat_sugerir(names, both, 2, out, ativo);
    OK(ganhou == 1 && posts == 1 && strstr(ultimoCorpo, "xyzzy"), "positive and null answers expire after 30 days in a running app");
  }

  printf(falhas ? "livecat: %d falha(s)\n" : "livecat: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}
