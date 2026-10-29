// Regressao #205: pausados preservados com proximo OFF/ON. Rede falsa.
// bash tests/cw205_diagnostico.sh
#ifdef CW205_MONTAGEM
#define main cwordem_main_original
#include "cwordem_desc.c"
#undef main
int ajustes_cw_proximo(void) { return getenv("CW205_PROXIMO") != NULL; }

int main(void) {
  static Falso recentes[12];
  static char ids[12][32];
  static CatItem lote[12];
  int i, nc, pausados = 0, seguir = 0;
  prog_definir_relogio(relogioProg);
  agoraMs = (long long)time(NULL) * 1000LL;
  // Mesma ordem do log: 12 sugestoes recentes contra 12 pausados mais velhos.
  // Ted Lasso T4E3 tem 62%; nao e futuro nem terminado.
  for (i = 0; i < 12; i++) {
    ProgRegistro r = {0};
    snprintf(ids[i], sizeof ids[i], "tt%07d:1:2", 8000000 + i);
    recentes[i] = (Falso){ {0}, 1, 0, 1791813960000LL - i * 60000LL, -1 };
    snprintf(recentes[i].id, sizeof recentes[i].id, "%s", ids[i]);
    snprintf(r.contentId, sizeof r.contentId, i ? "tt%07d" :
             getenv("CW205_MESMA_OBRA") ? "tt8000000" : "tt10986410", 7000000 + i);
    r.temporada = i ? 1 : 4; r.episodio = i ? 1 : 3;
    r.posSeg = i ? 1200 : 1860; r.durSeg = 3000;
    r.lastWatchedMs = 1791216728000LL - i * 60000LL;
    assert(prog_aplicar_remoto(&r));
  }
  tabela = recentes; nTabela = 12; modoTeste = CWO_SEPARAR;
  naoExibidosTeste = 0;
  nc = montarContinuar(lote, 12);
  for (i = 0; i < nc; i++) {
    pausados += lote[i].progresso > 0;
    seguir += lote[i].progresso == 0;
  }
  printf("CW205 montagem: proximo=%d n=%d pausados=%d a_seguir=%d\n",
         ajustes_cw_proximo(), nc, pausados, seguir);
  if (!ajustes_cw_proximo()) {
    if (nc != 12 || pausados != 12 || seguir != 0) {
      fputs("FAIL: a seguir remoto excluiu pausado da mesma obra antes do filtro OFF\n", stderr);
      return 1;
    }
    assert(!strcmp(lote[0].imdb, getenv("CW205_MESMA_OBRA") ? "tt8000000:4:3" :
                                                            "tt10986410:4:3") && lote[0].progresso == 62);
    puts("PASS: filtro final desligado respeitado; nao reproduz 12 Up next com ajuste OFF");
    return 0;
  }
  // Regra aprovada: sugestoes nao expulsam pausados de outras obras.
  if (pausados != 12) {
    fputs("FAIL: corte final expulsou os 12 pausados, inclusive Ted Lasso a 62%\n", stderr);
    return 1;
  }
  return 0;
}
#else
#define main dup_main_original
#define rede_baixar_com dup_baixar_com_original
#define rede_baixar dup_baixar_original
#include "trakt_cw_dup.c"
#undef main
#undef rede_baixar_com
#undef rede_baixar

// Definido mesmo OFF: src/trakt.c atualmente nem consulta esta preferencia.
int ajustes_cw_proximo(void) { return getenv("CW205_PROXIMO") != NULL; }
char *rede_baixar_com(const char *url, int s, const char *const *cab) {
  (void)s; (void)cab;
  if (strstr(url, "/sync/playback")) {
    char *b = calloc(1, 20000); size_t w = 0;
    w += snprintf(b + w, 20000 - w, "[");
    for (int i = 0; i < 12; i++)
      w += snprintf(b + w, 20000 - w,
        "%s{\"id\":%d,\"progress\":62,\"paused_at\":\"2026-10-05T00:00:00.000Z\","
        "\"movie\":{\"title\":\"Pausado\",\"ids\":{\"imdb\":\"tt%07d\"}}}",
        i ? "," : "", i + 1, 7000000 + i);
    snprintf(b + w, 20000 - w, "]");
    if (getenv("CW205_CONCLUSAO")) {
      free(b);
      return strdup("[{\"id\":1,\"progress\":62,\"paused_at\":\"2026-10-05T00:00:00.000Z\","
                    "\"episode\":{\"season\":1,\"number\":1},\"show\":{\"title\":\"Pausado\",\"ids\":{\"imdb\":\"tt8000000\"}}}]");
    }
    if (getenv("CW205_PAUSADO_MAIS_NOVO")) {
      char *p = b;
      while ((p = strstr(p, "2026-10-05"))) { memcpy(p, "2026-10-13", 10); p += 10; }
    }
    return b;
  }
  if (strstr(url, "/sync/history?")) {
    if (getenv("CW205_SEM_HISTORICO")) return strdup("[]");
    if (getenv("CW205_CONCLUSAO"))
      return strdup(getenv("CW205_CONCLUSAO_ANTIGA") ?
        "[{\"watched_at\":\"2026-10-01T00:00:00.000Z\",\"episode\":{\"season\":1,\"number\":1},\"show\":{\"ids\":{\"imdb\":\"tt8000000\"}}}]" :
        "[{\"watched_at\":\"2026-10-12T00:00:00.000Z\",\"episode\":{\"season\":1,\"number\":1},\"show\":{\"ids\":{\"imdb\":\"tt8000000\"}}}]");
    char *b = calloc(1, 20000); size_t w = 0;
    w += snprintf(b + w, 20000 - w, "[");
    for (int i = 0; i < 12; i++)
      w += snprintf(b + w, 20000 - w,
        "%s{\"watched_at\":\"2026-10-12T14:06:00.000Z\","
        "\"episode\":{\"season\":1,\"number\":1},"
        "\"show\":{\"ids\":{\"imdb\":\"tt%07d\"}}}", i ? "," : "", 8000000 + i);
    snprintf(b + w, 20000 - w, "]"); return b;
  }
  if (strstr(url, "/progress/watched"))
    return strdup("{\"next_episode\":{\"season\":1,\"number\":2}}");
  return NULL;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  char id[24], *b = malloc(1000);
  const char *p = strstr(url, "/tt");
  if (!p || sscanf(p + 1, "%23[^/.?]", id) != 1) { free(b); return NULL; }
  snprintf(b, 1000, "{\"meta\":{\"name\":\"Fixture\",\"imdbRating\":\"7.9\","
           "\"videos\":[{\"id\":\"%s:1:2\",\"season\":1,\"episode\":2,"
           "\"released\":\"2020-01-01T00:00:00.000Z\"}]}}", id);
  return b;
}
int main(void) {
  static CatItem v[12];
  int n, i, pausados = 0, seguir = 0;
  trakt_definir("token-falso-offline", "cliente-falso");
  n = trakt_continuar(v, 12);
  for (i = 0; i < n; i++) {
    pausados += v[i].progresso == 62;
    seguir += trakt_e_a_seguir(v[i].imdb);
  }
  printf("CW205 trakt: proximo=%d n=%d pausados=%d a_seguir=%d\n",
         ajustes_cw_proximo(), n, pausados, seguir);
  if (getenv("CW205_CONCLUSAO")) {
    if (getenv("CW205_CONCLUSAO_ANTIGA"))
      assert(n == 1 && pausados == 1 && !strcmp(v[0].imdb, "tt8000000:1:1"));
    else if (ajustes_cw_proximo())
      assert(n == 1 && seguir == 1 && !strcmp(v[0].imdb, "tt8000000:1:2"));
    else assert(n == 0);
    puts("PASS: avancar exige conclusao posterior; OFF nao semeia"); return 0;
  }
  if (n != 12 || pausados != 12) {
    fputs("FAIL: trakt_continuar descartou filmes pausados a 62% por sugestoes\n", stderr);
    return 1;
  }
  puts("PASS: 12 pausados preservados"); return 0;
}
#endif
