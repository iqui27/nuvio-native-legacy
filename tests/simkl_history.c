#include "socialvis.h"
#include "simkl.h"
#include "dados.h"
#include "ajustes.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static time_t now = 1800000000;
static int calls, activities, delay, changed, failed, expect_incremental, disk_failed, null_dates;
static const char *stamp = "2026-10-01T10:00:00Z";
static const char *movie = "{\"movies\":[{\"last_watched_at\":\"2026-10-01T10:00:00Z\",\"movie\":{\"title\":\"Old \\\"title\\\"\",\"ids\":{\"imdb\":\"tt0000001\"}}}]}";
static const char *delta = "{\"movies\":[{\"last_watched_at\":\"2026-10-02T10:00:00Z\",\"movie\":{\"title\":\"New\",\"ids\":{\"imdb\":\"tt0000002\"}}}]}";

time_t history_time(time_t *t) {
  time_t n = __atomic_load_n(&now, __ATOMIC_ACQUIRE);
  if (t) *t = n;
  return n;
}
const char *history_client(void) { return "offline-client"; }
int history_save(const char *name, const char *body) {
  return disk_failed ? 0 : dados_gravar(name, body);
}
char *history_get(const char *url, int seconds, const char *const *headers, int *status, int *retry) {
  (void)seconds; *retry = 0; *status = 200;
  assert(headers && strstr(headers[0], "offline-token"));
  if (delay) usleep(2000000);
  if (strstr(url, "/sync/activities")) {
    __atomic_add_fetch(&activities, 1, __ATOMIC_RELEASE);
    if (failed == 1) { *status = 503; return NULL; }
    if (null_dates) return strdup("{\"movies\":{\"completed\":null},\"tv_shows\":{\"completed\":null},\"anime\":{\"completed\":null}}");
    char body[512];
    snprintf(body, sizeof body, "{\"movies\":{\"all\":\"%d\",\"completed\":\"%s\"},\"tv_shows\":{\"completed\":\"2026-09-01T00:00:00Z\"},\"anime\":{\"completed\":\"2026-09-01T00:00:00Z\"}}", changed, stamp);
    return strdup(body);
  }
  assert(strstr(url, "/sync/all-items/all/completed"));
  __atomic_add_fetch(&calls, 1, __ATOMIC_RELEASE);
  if (expect_incremental) assert(strstr(url, "date_from=2026-09-01"));
  else assert(!strstr(url, "date_from="));
  if (failed == 2) { *status = 503; return NULL; }
  if (failed == 3) return strdup("{\"error\":\"invalid\"}");
  return strdup(expect_incremental ? delta : movie);
}
static double monotonic(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec / 1e9;
}
static int read_profile(SvPerfil *p) { return socialvis_perfil("perfil:2", p); }
static void settled(void) {
  // Worker completion is observed through the production revision API.
  unsigned before = socialvis_revisao();
  SvPerfil p; assert(read_profile(&p));
  for (int i = 0; i < 6000 && socialvis_revisao() == before; i++) usleep(1000);
  assert(socialvis_revisao() != before);
}
static int count(void) {
  SvPerfil p; assert(read_profile(&p)); return p.a.nTit;
}
int main(int argc, char **argv) {
  assert(argc == 2); setbuf(stdout, NULL);
  dados_iniciar("deploy/app/art"); ajustes_dir(dados_dir());
  assert(dados_gravar("simkl-p2.txt", "offline-token\n"));
  SvEvento e = {0};
  strcpy(e.pessoaId, "perfil:2"); strcpy(e.pessoaNome, "Two");
  strcpy(e.imdb, "tt9999999"); strcpy(e.tipo, "movie");
  e.fonte = SV_FONTE_SIMKL; e.acao = SV_FIM;
  socialvis_definir_feed(&e, 1);
  if (!strcmp(argv[1], "async")) {
    delay = 1;
    unsigned before = socialvis_revisao();
    SvPerfil p; double start = monotonic(); assert(read_profile(&p));
    double ms = (monotonic() - start) * 1000;
    printf("socialvis_perfil with 2 s HTTP: %.3f ms (limit 50 ms)\n", ms);
    assert(ms < 50);
    for (int i = 0; i < 6000 && socialvis_revisao() == before; i++) usleep(1000);
    assert(socialvis_revisao() != before && count() == 2);
    // Cached reads stay fast while a second profile has no linked account.
    SimklVisto v[24]; assert(simkl_vistos_perfil(3, v, 24) == 0);
    usleep(100000);
    assert(count() == 2);
  } else if (!strcmp(argv[1], "gate")) {
    SvPerfil first; assert(read_profile(&first));
    for (int i = 0; i < 2000 && count() != 2; i++) usleep(1000);
    assert(count() == 2);
    for (int i = 0; i < 5; i++) {
      __atomic_add_fetch(&now, 121, __ATOMIC_RELEASE); changed++;
      int before = __atomic_load_n(&activities, __ATOMIC_ACQUIRE);
      SvPerfil p; assert(read_profile(&p));
      for (int j = 0; j < 2000 && __atomic_load_n(&activities, __ATOMIC_ACQUIRE) == before; j++) usleep(1000);
      usleep(20000);
    }
    printf("10 simulated minutes: all-items=%d, activities=%d\n", calls, activities);
    assert(calls == 1 && activities == 6);
    stamp = "2026-10-02T10:00:00Z"; expect_incremental = 1;
    __atomic_add_fetch(&now, 121, __ATOMIC_RELEASE); settled();
    assert(calls == 2 && count() == 3);
    // Network and parse errors must retain both cached records and watermark.
    for (failed = 1; failed <= 3; failed++) {
      stamp = "2026-10-03T10:00:00Z";
      __atomic_add_fetch(&now, 121, __ATOMIC_RELEASE);
      SvPerfil p; assert(read_profile(&p)); usleep(50000); assert(count() == 3);
    }
  } else if (!strcmp(argv[1], "restore")) {
    stamp = "2026-10-02T10:00:00Z";
    settled();
    assert(count() == 3 && calls == 0 && activities == 1);
    SimklVisto v[24]; assert(simkl_vistos_perfil(2, v, 24) == 2);
    assert(!strcmp(v[1].titulo, "Old \"title\""));
    assert(simkl_vistos_perfil(0, v, 24) == 0);
    assert(simkl_vistos_perfil(2, NULL, 24) == 0);
    assert(simkl_vistos_perfil(2, v, -1) == 0);
    simkl_esquecer(); failed = 1;
    settled(); assert(count() == 3 && calls == 0);
  } else if (!strcmp(argv[1], "stale")) {
    delay = 1; SvPerfil p; assert(read_profile(&p));
    usleep(100000); simkl_esquecer();
    unsigned revision = socialvis_revisao();
    usleep(4500000);
    assert(socialvis_revisao() == revision);
    // Old worker cannot publish into the new session.
    SimklVisto v[24]; assert(simkl_vistos_perfil(2, v, 24) == 0);
  } else if (!strcmp(argv[1], "unlinked")) {
    settled(); assert(count() == 2);
    assert(dados_gravar("simkl-p2.txt", "!\n"));
    unsigned revision = socialvis_revisao();
    __atomic_add_fetch(&now, 121, __ATOMIC_RELEASE);
    SvPerfil p; assert(read_profile(&p));
    for (int i = 0; i < 2000 && socialvis_revisao() == revision; i++) usleep(1000);
    assert(count() == 1 && socialvis_revisao() != revision);
  } else if (!strcmp(argv[1], "memory") || !strcmp(argv[1], "null")) {
    disk_failed = !strcmp(argv[1], "memory"); null_dates = !strcmp(argv[1], "null");
    settled(); assert(count() == 2);
    __atomic_add_fetch(&now, 121, __ATOMIC_RELEASE); settled();
    assert(calls == 1 && activities == 2 && count() == 2);
  } else {
    size_t cap = 6 * 1024 * 1024, n = 0;
    char *body = malloc(cap); assert(body);
    n += (size_t)sprintf(body, "{\"movies\":[");
    for (int i = 0; n < 5 * 1024 * 1024; i++)
      n += (size_t)sprintf(body + n, "%s{\"last_watched_at\":\"2026-10-01T10:00:%02dZ\",\"movie\":{\"title\":\"Large history\",\"ids\":{\"imdb\":\"tt%07d\"}}}", i ? "," : "", i % 60, i);
    strcpy(body + n, "]}");
    SimklVisto v[24]; assert(simkl_ler_vistos(body, v, 24) == 24);
    for (int i = 1; i < 24; i++) assert(v[i-1].quando >= v[i].quando);
    printf("parsed %zu bytes under ASan/UBSan: 24 sorted items\n", n + 2); free(body);
  }
  printf("PASS %s\n", argv[1]); return 0;
}
