// Actual production subtitle worker; offline providers and measurements only.
#include <time.h>
int teste_clock_gettime(clockid_t c, struct timespec *t);
#define clock_gettime teste_clock_gettime
#include "stub_perfisessao.h"
#include "../src/addons.c"
#undef clock_gettime
#include <assert.h>
static unsigned long long agoraTeste;
static int prazoCenario, tentativasCauda;
static unsigned atrasoInicio;
void prazo_auto_iniciar(void);
void prazo_auto_avancar(unsigned ms);
int prazo_auto_selecionou(void);
int teste_clock_gettime(clockid_t c, struct timespec *t) {
  (void)c; t->tv_sec = (time_t)(agoraTeste / 1000u); t->tv_nsec = (long)(agoraTeste % 1000u) * 1000000L; return 0;
}
static void avancar(unsigned ms) {
  unsigned long long fim = agoraTeste + ms;
  while (agoraTeste < fim) {
    agoraTeste += fim - agoraTeste > 500 ? 500 : fim - agoraTeste;
    if (prazoCenario) prazo_auto_avancar((unsigned)agoraTeste);
  }
}
static const char *responses[3];
static char ultimaUrl[3][2048];
static int desligaIdx = -1;
static int recusaExtras, semRange, trechos;
static _Atomic int requests;
static int status[3] = {200, 200, 200};
const char *rede_ultimo_erro(void) { return ""; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  (void)url; snprintf(dst, tam, "redacted"); return dst;
}
int xtream_url_limitada(const char *url) { (void)url; return 0; }
char *rede_baixar_medido_controle(const char *url, int seconds,
                                  const char *const *headers,
                                  const RedeControle *controle,
                                  RedeMedida *medida) {
  int idx = -1;
  assert(!headers && !controle && seconds == LEG_TETO_S);
  for (int i = 0; i < 3; i++) {
    char base[80]; snprintf(base, sizeof base, "https://fixture.invalid/provider%d/", i);
    if (!strncmp(url, base, strlen(base))) idx = i;
  }
  assert(idx >= 0 && strstr(url, "/subtitles/") && strstr(url, ".json"));
  requests++;
  if (prazoCenario) avancar(15000);
  snprintf(ultimaUrl[idx], sizeof ultimaUrl[idx], "%s", url);
  if (recusaExtras && strstr(url, "videoSize=")) {
    *medida = (RedeMedida){ .status = 400, .bytes = 5, .ms = 7 };
    return strdup("nope!");
  }
  *medida = (RedeMedida){ .status = status[idx], .bytes = responses[idx] ? (long)strlen(responses[idx]) : 0, .ms = 7 };
  return responses[idx] ? strdup(responses[idx]) : NULL;
}
// #201: os dois Range do hash. Conteudo deterministico: byte = posicao % 251.
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *st, int *erro, char *final, unsigned tamFinal) {
  char *b;
  (void)final; (void)tamFinal;
  assert(!strncmp(url, "https://cdn.fixture.invalid/", 28));
  trechos++;
  if (erro) *erro = 0;
  if (prazoCenario && !ini) avancar(atrasoInicio);
  if (prazoCenario && ini > 0) {
    tentativasCauda++; avancar((unsigned)segundos * 1000u);
    if (st) *st = 0;
    if (erro) *erro = 28;
    *tam = 0;
    return NULL;
  }
  if (semRange) { if (st) *st = 200; }
  else if (st) *st = 206;
  b = malloc((size_t)(fim - ini + 2));
  for (long i = ini; i <= fim; i++) b[i - ini] = (char)(i % 251);
  b[fim - ini + 1] = 0;
  *tam = fim - ini + 1;
  return b;
}
const char *i18n(const char *text) { return text; }
static void run(int n, const char *id, const char *tipo) {
  const char *nomes[] = {"OpenSubtitles", "Subs.ro", "Community Subtitles"};
  memset(addon, 0, sizeof addon); nAddon = n;
  for (int i = 0; i < n; i++) {
    addon[i].ativo = addon[i].legenda = 1;
    if (i == desligaIdx) addon[i].ativo = 0;   // desligado na conta
    snprintf(addon[i].base, sizeof addon[i].base, "https://fixture.invalid/provider%d", i);
    snprintf(addon[i].nome, sizeof addon[i].nome, "%s", nomes[i]);
  }
  snprintf(legId, sizeof legId, "%s", id); snprintf(legTipo, sizeof legTipo, "%s", tipo);
  legParar = 0; fioLegVivo = 1; buscarLegendas(NULL); assert(!fioLegVivo);
}
static void dense(char *json, size_t size, int provider) {
  const char *langs[] = {"ron", "spa", "eng", "kor"};
  snprintf(json, size, "{\"subtitles\":[");
  for (int l = 0; l < 4; l++) for (int i = 0; i < LEG_MAX + 2; i++) {
    char item[220];
    snprintf(item, sizeof item, "%s{\"lang\":\"%s\",\"url\":\"https://fixture.invalid/PRIVATE_TOKEN/%d-%d-%d.srt\"}",
             (l || i) ? "," : "", langs[l], provider, l, i);
    strncat(json, item, size - strlen(json) - 1);
  }
  strncat(json, "]}", size - strlen(json) - 1);
}
static char *fixture(const char *name) {
  char path[160];
  snprintf(path, sizeof path, "tests/fixtures/subsync/%s.json", name);
  FILE *f = fopen(path, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n > 0);
  rewind(f); char *body = calloc((size_t)n + 1, 1); assert(body);
  assert(fread(body, 1, (size_t)n, f) == (size_t)n); fclose(f);
  return body;
}

int main(void) {
  // Captured Subtitle Sync 2.1.0 responses; download URLs replaced with markers.
  {
    char *body = fixture("movie"), *manifest = fixture("manifest");
    ling_local_legenda("pt-br"); ling_conta_legenda2("");
    responses[0] = body; run(1, "tt0111161", "movie");
    capacidadesDoManifesto(0, manifest);
    assert(addon[0].legenda && !addon[0].fonte);
    assert(nLegs == 3); // Informational SRT must not enter automatic selection.
    assert(!strcmp(legs[0].idioma, "pob"));
    assert(!strcmp(ling_selo(legs[0].idioma), "PT-BR"));
    assert(strstr(legs[0].rotulo, "Not synced"));
    assert(strcmp(legs[0].rotulo, legs[1].rotulo));
    free(body); body = fixture("extras"); responses[0] = body;
    snprintf(legExt.id, sizeof legExt.id, "tt0111161");
    snprintf(legExt.arquivo, sizeof legExt.arquivo, "The.Shawshank.Redemption.1994.1080p.mkv");
    legExt.tamanho = 1000000;
    run(1, "tt0111161", "movie");
    assert(nLegs == 3 && strstr(ultimaUrl[0], "videoSize=1000000&filename="));
    assert(strstr(legs[0].rotulo, "Weak sync"));
    assert(strstr(legs[1].rotulo, "Syncs when picked"));
    assert(!legs[0].arquivo[0]); // A display label is not a release filename.
    memset(&legExt, 0, sizeof legExt);
    free(body); free(manifest);
  }
  {
    int t, e;
    episodioPedido("tt12409194:0:1", &t, &e); assert(t == 0 && e == 1);
    assert(episodioCorreto("{\"season\":0,\"episode\":1}", NULL, t, e));
    assert(!episodioCorreto("{\"season\":1,\"episode\":1}", NULL, t, e));
    episodioPedido("kitsu:41370:5", &t, &e); assert(t == -1 && e == 5);
    assert(episodioCorreto("{\"season\":1,\"episode\":5}", NULL, t, e));
    episodioPedido("kitsu:41370:0:5", &t, &e); assert(t == 0 && e == 5);
  }

  responses[0] = "{\"subtitles\":[{\"lang\":\"ron\",\"url\":\"https://fixture.invalid/ro.srt\"},{\"lang\":\"spa\",\"url\":\"https://fixture.invalid/es.srt\"},{\"lang\":\"eng\",\"url\":\"https://fixture.invalid/en.srt\"},{\"lang\":\"kor\",\"url\":\"https://fixture.invalid/ko.srt\"}]}";
  ling_conta_legenda("ro"); ling_conta_legenda2(""); ling_local_legenda("*"); run(1, "tt123", "movie");
  assert(nLegs == 4 && !strcmp(legs[0].idioma, "ron") && !strcmp(legs[3].idioma, "kor"));
  assert(!strcmp(legs[0].provedor, "OpenSubtitles"));
  ling_local_legenda(""); ling_conta_legenda(""); run(1, "tt123", "movie"); assert(nLegs == 4);
  ling_conta_legenda("ro"); ling_conta_legenda2("es"); run(1, "tt123", "movie");
  assert(nLegs == 3 && !strcmp(legs[0].idioma, "ron") && !strcmp(legs[1].idioma, "spa") && !strcmp(legs[2].idioma, "eng"));

  char primeiro[32000], segundo[32000]; dense(primeiro, sizeof primeiro, 0); dense(segundo, sizeof segundo, 1);
  responses[0] = primeiro; responses[1] = segundo;
  run(1, "tt123", "movie");
  int counts[3] = {0}; const char *langs[] = {"ron", "spa", "eng"};
  for (int i = 0; i < nLegs; i++) for (int l = 0; l < 3; l++) if (!strcmp(legs[i].idioma, langs[l])) counts[l]++;
  assert(nLegs == LEG_MAX && counts[0] == LEG_MAX / 3 && counts[1] == LEG_MAX / 3 && counts[2] == LEG_MAX / 3);

  // First provider has >12 candidates. Both must be requested and contribute,
  // with principal/secondary/English quotas and real origin attached.
  int antes = requests; run(2, "tt123", "movie"); assert(requests - antes == 2 && nLegs == LEG_MAX);
  int providers[2] = {0}; memset(counts, 0, sizeof counts);
  for (int i = 0; i < nLegs; i++) {
    assert(!strcmp(legs[i].idioma, langs[i / (LEG_MAX / 3)]));
    for (int j = 0; j < 2; j++) if (!strcmp(legs[i].provedor, j ? "Subs.ro" : "OpenSubtitles")) providers[j]++;
    counts[i / (LEG_MAX / 3)]++;
  }
  assert(providers[0] == LEG_MAX / 2 && providers[1] == LEG_MAX / 2 && counts[0] == LEG_MAX / 3 && counts[1] == LEG_MAX / 3 && counts[2] == LEG_MAX / 3);
  ling_local_legenda("*"); antes = requests; run(2, "tt123", "movie");
  assert(requests - antes == 2 && nLegs == LEG_MAX);
  for (int i = 0; i < nLegs; i++) assert(!strcmp(legs[i].provedor, (i % 2) ? "Subs.ro" : "OpenSubtitles"));

  // Empty, missing array and HTTP404 are distinguishable without blocking the
  // later provider, and malformed entries never become menu options.
  responses[0] = "{\"subtitles\":[]}";
  responses[1] = "{\"subtitles\":[{\"lang\":\"ron\",\"url\":\"https://fixture.invalid/PRIVATE_TOKEN/ro.srt\"},{\"lang\":\"spa\"},{\"url\":\"https://fixture.invalid/no-lang.srt\"}]}";
  antes = requests; run(2, "tt123", "movie");
  assert(requests - antes == 2 && nLegs == 1 && !strcmp(legs[0].provedor, "Subs.ro"));
  // Repro da revisao: error.* pode repetir URL/config e injetar linhas no log.
  responses[0] = "{\"subtitles\":[{\"id\":\"error.upstream\",\"lang\":\"Falha ao acessar https://fixture.invalid/PRIVATE_TOKEN/subtitles/tt123.json?api_key=PRIVATE_TOKEN\\nINJECTED_DIAGNOSTIC\"}]}";
  run(1, "tt123", "movie"); assert(nLegs == 0);
  // Sem lang, o id de erro tambem e entrada nao confiavel.
  responses[0] = "{\"subtitles\":[{\"id\":\"error.https://fixture.invalid/PRIVATE_TOKEN?config=PRIVATE_CONFIG\\nINJECTED_DIAGNOSTIC\"}]}";
  run(1, "tt123", "movie"); assert(nLegs == 0);
  responses[0] = "{\"error\":\"PRIVATE_TOKEN\"}"; run(2, "tt123", "movie"); assert(nLegs == 1);
  responses[0] = NULL; status[0] = 404; run(2, "tt123", "movie"); assert(nLegs == 1);

  responses[0] = "{\"subtitles\":[{\"lang\":\"ron\",\"url\":\"https://fixture.invalid/ok.srt\",\"season\":2,\"episode\":4},{\"lang\":\"ron\",\"url\":\"https://fixture.invalid/wrong.srt\",\"season\":2,\"episode\":3},{\"lang\":\"ron\",\"url\":\"https://fixture.invalid/filename-wrong.srt\",\"subtitleFileName\":\"Show.S02E03.srt\"}]}";
  status[0] = 200; run(1, "tt123:2:4", "series");
  assert(nLegs == 1 && strstr(legs[0].url, "/ok.srt"));
  // #201: extras do Stremio. Hash medido por dois Range, valores codificados,
  // ordem hash/tamanho/nome; addon que recusa extras ganha o pedido antigo.
  {
    unsigned long long tam = 1000000;
    unsigned char *ini = malloc(LEGEXTRAS_BLOCO), *fim = malloc(LEGEXTRAS_BLOCO);
    char hash[17], esperado[1024];
    for (long i = 0; i < LEGEXTRAS_BLOCO; i++) {
      ini[i] = (unsigned char)(i % 251);
      fim[i] = (unsigned char)(((long)tam - LEGEXTRAS_BLOCO + i) % 251);
    }
    assert(legextras_hash(ini, LEGEXTRAS_BLOCO, fim, LEGEXTRAS_BLOCO, tam, hash));
    free(ini); free(fim);
    responses[0] = "{\"subtitles\":[{\"lang\":\"ces\",\"url\":\"https://fixture.invalid/cs.srt\"},{\"lang\":\"Hebrew (Auto-Subs)\",\"url\":\"https://fixture.invalid/he.srt\"}]}";
    status[0] = 200; ling_local_legenda(""); ling_conta_legenda("cs"); ling_conta_legenda2("he");
    memset(&legExt, 0, sizeof legExt);
    snprintf(legExt.id, sizeof legExt.id, "tt123");
    snprintf(legExt.arquivo, sizeof legExt.arquivo, "Movie Name \xc3\xa7\xc3\xa3o [1080p].mkv");
    legExt.tamanho = tam;
    snprintf(legExt.video, sizeof legExt.video, "https://cdn.fixture.invalid/x/v.mkv");
    trechos = 0; run(1, "tt123", "movie");
    snprintf(esperado, sizeof esperado,
             "https://fixture.invalid/provider0/subtitles/movie/tt123/videoHash=%s&videoSize=1000000&filename=Movie%%20Name%%20%%C3%%A7%%C3%%A3o%%20%%5B1080p%%5D.mkv.json", hash);
    assert(trechos == 2 && !strcmp(ultimaUrl[0], esperado));
    // ISO 639-2/T e nome por extenso: os dois entram.
    assert(nLegs == 2 && !strcmp(legs[0].idioma, "ces") && !strcmp(legs[1].idioma, "heb"));
    // Mesma fonte de novo: o hash vem do cache, sem Range.
    trechos = 0; run(1, "tt123", "movie"); assert(trechos == 0 && strstr(ultimaUrl[0], hash));
    // Servidor sem Range (200): sem hash, nunca um hash errado.
    hashMedido[0] = 0; semRange = 1; trechos = 0; run(1, "tt123", "movie");
    assert(!strstr(ultimaUrl[0], "videoHash=") && strstr(ultimaUrl[0], "/tt123/videoSize=1000000&filename="));
    semRange = 0;
    // P2P local: nada de Range; nome pela URL quando o addon nao deu.
    legExt.arquivo[0] = 0; legExt.tamanho = 0;
    snprintf(legExt.video, sizeof legExt.video, "http://127.0.0.1:11470/abc/0/Show.S01E02.mkv");
    trechos = 0; run(1, "tt123", "movie");
    assert(trechos == 0 && strstr(ultimaUrl[0], "/tt123/filename=Show.S01E02.mkv.json"));
    // Addon que recusa o caminho com extras: pedido antigo em seguida.
    legExt.tamanho = tam; snprintf(legExt.video, sizeof legExt.video, "https://cdn.fixture.invalid/x/v.mkv");
    recusaExtras = 1; antes = requests; run(1, "tt123", "movie");
    assert(requests - antes == 2 && !strcmp(ultimaUrl[0], "https://fixture.invalid/provider0/subtitles/movie/tt123.json") && nLegs == 2);
    recusaExtras = 0;
    // Extras de OUTRO titulo nao vazam.
    run(1, "tt999", "movie"); assert(!strcmp(ultimaUrl[0], "https://fixture.invalid/provider0/subtitles/movie/tt999.json"));
    // Refazer com extras: a lista antiga fica ate o fim e e trocada inteira.
    legManter = 1; run(1, "tt123", "movie"); assert(nLegs == 2 && !legManter);
    // Link de legenda maior que o campo: descartado, nunca cortado.
    { static char longo[3000]; char *q = longo;
      q += sprintf(q, "{\"subtitles\":[{\"lang\":\"ces\",\"url\":\"https://fixture.invalid/");
      for (int i = 0; i < 1500; i++) *q++ = 'a';
      sprintf(q, ".srt\"},{\"lang\":\"ces\",\"url\":\"https://fixture.invalid/curto.srt\"}]}");
      responses[0] = longo; memset(&legExt, 0, sizeof legExt); run(1, "tt123", "movie");
      assert(nLegs == 1 && strstr(legs[0].url, "curto.srt")); }
  }

  // Desligado na conta: nenhuma requisicao de legenda ao addon (2.0.3).
  { responses[0] = responses[1] = "{\"subtitles\":[{\"lang\":\"eng\",\"url\":\"https://fixture.invalid/x.srt\"}]}";
    memset(ultimaUrl, 0, sizeof ultimaUrl);
    desligaIdx = 1; antes = requests; run(2, "tt123", "movie"); desligaIdx = -1;
    assert(requests - antes == 1 && ultimaUrl[0][0] && !ultimaUrl[1][0]); }

  // Hash + retry nao podem passar do prazo de 30 s da legenda automatica.
  { unsigned long long tam = 1000000;
    responses[0] = "{\"subtitles\":[{\"lang\":\"por\",\"url\":\"https://fixture.invalid/pt.srt\"}]}";
    status[0] = 200; ling_local_legenda("pt"); ling_conta_legenda(""); ling_conta_legenda2("");
    memset(&legExt, 0, sizeof legExt); hashMedido[0] = 0;
    snprintf(legExt.id, sizeof legExt.id, "tt123"); legExt.tamanho = tam;
    snprintf(legExt.video, sizeof legExt.video, "https://cdn.fixture.invalid/x/v.mkv");
    for (atrasoInicio = 0; atrasoInicio <= 6000; atrasoInicio += 6000) {
      agoraTeste = 0; tentativasCauda = 0; nLegs = 0;
      prazoCenario = 1; fioLegVivo = 1; prazo_auto_iniciar();
      avancar(1000); run(1, "tt123", "movie"); avancar(500); prazoCenario = 0;
      assert(prazo_auto_selecionou());
      assert(agoraTeste <= 28500 && tentativasCauda == (atrasoInicio ? 1 : 2));
      assert(!strstr(ultimaUrl[0], "videoHash="));
    } }

  puts("addon subtitles: provider fairness, real origin, ordered languages, empty/missing/HTTP and episode checks ok");
}
