// O SERVIDOR DE MENTIRA da enquete: imita servidor/recomendacoes/src/enquete.js
// (mesma forma de JSON, 409/410, opt-out, contagem so depois do voto). Incluido
// por tests/enquete.c e tests/enquete_shot.c DEPOIS de src/enquete.c.
#ifndef ENQUETE_ARTE
#define ENQUETE_ARTE "https://x.test/l.jpg"
#endif
// --- o servidor de mentira ---------------------------------------------------------
static struct {
  long long fim; int voto, optout, ativa; int base[3]; int delayMs, vote500;
  int nGet, nVoto, nOptout; char id[48];
} S;

static int semIdentidade, getStatus;

int teste_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv, char *pf, size_t np) {
  (void)pf; (void)np;
  if (semIdentidade) return 0;
  assert(na >= 3022); /* A full Nuvio bearer must fit without truncation. */
  snprintf(aut, na, "Authorization: Bearer x"); snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  cab[0] = aut; cab[1] = via; cab[2] = NULL; cab[3] = NULL;
  return 1;
}

static char *corpoEnquete(void) {
  char *b = malloc(2048), *w;
  int i, total = 0;
  w = b;
  w += sprintf(w, "{\"optout\":%d,\"enquete\":", S.optout);
  if (S.optout || !S.ativa || S.fim <= (long long)time(NULL)) { strcpy(w, "null}"); return b; }
  w += sprintf(w, "{\"id\":\"%s\",\"pergunta\":\"Qual logo voc\\u00ea prefere?\",\"arte\":\"" ENQUETE_ARTE "\",\"fim\":%lld,"
               "\"opcoes\":[{\"idx\":1,\"texto\":\"Novo\"},{\"idx\":2,\"texto\":\"Cl\\u00e1ssico renovado\"},{\"idx\":3,\"texto\":\"Cl\\u00e1ssico\"}],\"voto\":%d",
               S.id, S.fim, S.voto);
  if (S.voto) {
    w += sprintf(w, ",\"resultado\":[");
    for (i = 0; i < 3; i++) {
      int v = S.base[i] + (S.voto == i + 1);
      total += v;
      w += sprintf(w, "%s{\"idx\":%d,\"votos\":%d}", i ? "," : "", i + 1, v);
    }
    w += sprintf(w, "],\"total\":%d", total);
  }
  strcpy(w, "}}");
  return b;
}

char *teste_get(const char *url, int s, const char *const *cab, int *st) {
  (void)s; (void)cab;
  assert(strstr(url, "/v1/enquete"));
  S.nGet++;
  if (S.delayMs) usleep((useconds_t)S.delayMs * 1000);
  *st = getStatus ? getStatus : 200;
  return corpoEnquete();
}
char *teste_post(const char *url, int s, const char *const *cab, const char *corpo, int *st) {
  (void)s; (void)cab;
  if (strstr(url, "/v1/enquete/voto")) {
    int op = 0;
    S.nVoto++;
    if (S.vote500) { *st = 500; return strdup("{\"erro\":\"x\"}"); }
    sscanf(strstr(corpo, "\"opcao\":") + 8, "%d", &op);
    if (S.fim <= (long long)time(NULL)) { *st = 410; return strdup("{\"erro\":\"encerrada\"}"); }
    if (S.voto && S.voto != op) { *st = 409; return strdup("{\"erro\":\"ja votou\"}"); }
    S.voto = op; *st = 200;
    return corpoEnquete();
  }
  if (strstr(url, "/v1/enquete/optout")) {
    S.nOptout++;
    S.optout = strstr(corpo, "\"optout\":1") ? 1 : 0;
    *st = 200;
    { char *b = malloc(32); sprintf(b, "{\"optout\":%d}", S.optout); return b; }
  }
  *st = 404; return strdup("{}");
}

