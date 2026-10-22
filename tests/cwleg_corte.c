// Revisao: 13 playback; o primeiro sera excluido apos o teto de 12.
#define main original_main
#define rede_baixar_com original_rede_baixar_com
#define rede_baixar original_rede_baixar
#include "trakt_cw_dup.c"
#undef main
#undef rede_baixar_com
#undef rede_baixar
int ajustes_cw_proximo(void) { return getenv("REPRO_ON") != NULL; }
char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) {
    char *b = calloc(1, 10000); size_t w;
    w = snprintf(b, 10000, "[{\"id\":1,\"progress\":62,\"paused_at\":\"2026-10-05T00:00:00.000Z\",\"episode\":{\"season\":1,\"number\":1},\"show\":{\"title\":\"Concluido depois\",\"ids\":{\"imdb\":\"tt8000000\"}}}");
    for (int i=0; i<12; i++) w += snprintf(b+w, 10000-w, ",{\"id\":%d,\"progress\":62,\"paused_at\":\"2026-10-04T00:00:00.000Z\",\"movie\":{\"title\":\"Pausado valido\",\"ids\":{\"imdb\":\"tt%07d\"}}}", i+2, 7000000+i);
    snprintf(b+w, 10000-w, "]"); return b;
  }
  if (strstr(url, "/sync/history?")) return strdup("[{\"watched_at\":\"2026-10-12T00:00:00.000Z\",\"episode\":{\"season\":1,\"number\":1},\"show\":{\"ids\":{\"imdb\":\"tt8000000\"}}}]");
  if (strstr(url, "/progress/watched")) return strdup("{\"next_episode\":{\"season\":1,\"number\":2}}");
  return NULL;
}
char *rede_baixar(const char *u, int s) {
  (void)u; (void)s;
  return strdup("{\"meta\":{\"name\":\"Fixture\",\"imdbRating\":\"7.9\",\"videos\":[{\"id\":\"tt8000000:1:2\",\"season\":1,\"episode\":2,\"released\":\"2020-01-01T00:00:00.000Z\"}]}}");
}
int main(void) {
  static CatItem v[12];
  trakt_definir("offline", "offline");
  int n = trakt_continuar(v, 12), pausados = 0, seguir = 0, ultimo = 0;
  for (int i=0; i<n; i++) {
    pausados += v[i].progresso == 62;
    seguir += v[i].progresso == 0;
    ultimo += !strcmp(v[i].imdb, "tt7000011");
  }
  printf("REPRO corte: ON=%d n=%d pausados=%d sugestoes=%d ultimo_pausado=%d; esperado 12 pausados incluindo tt7000011\n", ajustes_cw_proximo(), n, pausados, seguir, ultimo);
  return !(n == 12 && pausados == 12 && ultimo);
}
