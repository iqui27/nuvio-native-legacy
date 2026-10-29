// Compile the real private state, as in addons_paralelo.c; no network or JS runtime.
#include "../src/plugins.c"
#include "../src/addons.c"
#include <assert.h>

static char *saved;
static int returnHit, slow, animeTitle, crowded, ran;
static const char *manifest = "{\"scrapers\":["
  "{\"id\":\"anime\",\"filename\":\"anime.js\",\"supportedTypes\":[\"anime\"]},"
  "{\"id\":\"general\",\"filename\":\"general.js\",\"supportedTypes\":[\"movie\",\"tv\"]},"
  "{\"id\":\"hit\",\"filename\":\"hit.js\",\"supportedTypes\":[\"movie\",\"series\"]}]}";
unsigned plugrede_capacidades(void) { return PR_CAP_REDE; }
int plugrede_pedir(const RedePedido *q, RedeResposta *r) { (void)q; memset(r, 0, sizeof *r); return 0; }
const char *i18n(const char *s) { return s; }
const char *sessao_usuario(void) { return "test"; }
int perfis_ativo_addons(void) { return 1; }
const char *desc_chave_tmdb_reserva(void) { return ""; }
void debrid_contexto_entrar(const DebridContexto *c) { (void)c; }
void debrid_contexto_sair(void) {}
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *item) {
  (void)id; (void)tipo; memset(item, 0, sizeof *item);
  if (animeTitle) { strcpy(item->genero, "Animation"); strcpy(item->pais, "Japan"); }
  return 1;
}
char *dados_ler(const char *nome) {
  char cache[64], text[2048];
  if (strncmp(nome, "plugins-c-", 10)) return saved ? strdup(saved) : NULL;
  snprintf(cache, sizeof cache, "plugins-c-%016llx.txt", fnv("https://fixture.invalid/manifest.json"));
  snprintf(text, sizeof text, "//nvcache %ld\n%s", (long)time(NULL),
           !strcmp(nome, cache) ? manifest : "// cached test scraper code");
  return strdup(text);
}
int dados_gravar(const char *nome, const char *text) { (void)nome; free(saved); saved = strdup(text); return 1; }
size_t pj_pilha(void) { return 1024 * 1024; }
int pj_executar(const PjPedido *p, PjResultado *r) {
  memset(r, 0, sizeof *r);
  if (crowded) {
    __atomic_add_fetch(&ran, 1, __ATOMIC_RELAXED);
    struct timespec t = {0, p->prazoMs * 1000L};
    nanosleep(&t, NULL); // Scale scraper deadlines by 1000; no network.
  }
  if (slow) {
    while (!__atomic_load_n(p->cancelado, __ATOMIC_ACQUIRE)) {
      struct timespec t = {0, 1000000}; nanosleep(&t, NULL);
    }
    r->cancelado = 1;
  } else r->json = strdup(returnHit && !strcmp(p->idScraper, "hit") ? "hit" : "empty");
  return 0;
}
void pj_resultado_soltar(PjResultado *r) { free(r->json); }
int stream_extrair(const char *json, const char *nome, Stream **out) {
  (void)nome; *out = !strcmp(json, "hit") ? calloc(1, sizeof **out) : NULL;
  return *out != NULL;
}
static void order(void *u, int k, const char *nome, const char *id, int estado, const void *f, int n) {
  (void)f; (void)n; (void)id;
  if (estado == 1) assert(!strcmp(nome, ((const char **)u)[k]));
}

int main(void) {
  char msg[256]; Stream *out = NULL; PlugResumo p;
  // A large pool must query substantially more than the old six scrapers.
  E.ligado = ligadoAt = 1;
  E.nRepos = 1; E.repos[0].r.ativo = 1;
  E.nScr = 130;
  for (int i = 0; i < E.nScr; i++) {
    E.scr[i] = (Scraper){.ativo=1, .filme=1};
    snprintf(E.scr[i].id, sizeof E.scr[i].id, "fixture-%d", i);
    strcpy(E.scr[i].url, "https://fixture.invalid/script.js");
  }
  crowded = 1;
  assert(plugins_consultar("tmdb:603", "movie", NULL, NULL, NULL, NULL, &out) == 0);
  free(out);
  assert(ran >= 64);
  crowded = 0; E.nScr = 0;
  const char *normal[] = {"general", "hit", "anime"};
  const char *hits[] = {"hit", "general", "anime"};
  const char *anime[] = {"hit", "anime", "general"};
  E.ligado = ligadoAt = 1;
  E.nRepos = 1; E.repos[0].r.ativo = 1;
  strcpy(E.repos[0].r.url, "https://fixture.invalid/manifest.json");
  aplicarManifesto(&E, 0, manifest, E.repos[0].r.url);
  resumo = (Resumo){ .valido = 1, .extraIncompleta = 1 };
  assert(addons_busca_incompleta());
  resumo.extraIncompleta = 0;
  assert(!addons_busca_incompleta());
  strcpy(alvoId, "tmdb:603:1:2"); strcpy(alvoTipo, "series");
  assert(addons_motivo_vazio(msg, sizeof msg));
  assert(strstr(msg, "Plugins") && strstr(msg, "0 de 3"));
  returnHit = 1;
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, order, normal, &out) == 1);
  free(out); returnHit = 0;
  assert(saved && strstr(saved, "hit\t"));
  carregar(); // Same persisted profile file and cached manifest used by the app.
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, order, hits, &out) == 0);
  plugins_resumo(alvoId, alvoTipo, &p);
  assert(p.enfileirados == 3 && p.terminados == 3 && !p.cortados);
  assert(addons_motivo_vazio(msg, sizeof msg) && strstr(msg, "3 de 3"));
  animeTitle = 1;
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, order, anime, &out) == 0);
  animeTitle = 0; slow = 1;
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, order, hits, &out) == 0);
  plugins_resumo(alvoId, alvoTipo, &p);
  assert(p.enfileirados == 3 && !p.terminados && p.cortados == 3);
  assert(addons_motivo_vazio(msg, sizeof msg) && strstr(msg, "0 de 3"));
  // Stale title/profile rounds cannot supply old checked counts.
  plugins_resumo("tmdb:999", alvoTipo, &p); assert(!p.terminados);
  gerAvancar(); plugins_resumo(alvoId, alvoTipo, &p); assert(!p.terminados);
  E.repos[0].r.ativo = 0;
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, NULL, NULL, &out) == 0);
  assert(addons_motivo_vazio(msg, sizeof msg) && !strcmp(msg, "Nenhum add-on de fontes instalado"));
  E.repos[0].r.ativo = 1; ligadoAt = 0;
  assert(addons_motivo_vazio(msg, sizeof msg) && !strcmp(msg, "Nenhum add-on de fontes instalado"));
  ligadoAt = 1; slow = 0;
  for (int i = 0; i < E.nScr; i++) E.scr[i].ativo = 0;
  assert(plugins_consultar(alvoId, alvoTipo, NULL, NULL, NULL, NULL, &out) == 0);
  assert(addons_motivo_vazio(msg, sizeof msg) && !strcmp(msg, "Nenhum add-on de fontes instalado"));
  resumo = (Resumo){.valido=1, .instalados=1, .comFonte=1, .ligadosComFonte=1,
                    .consultados=1, .responderam=1};
  strcpy(resumo.vazio, "Fixture");
  assert(addons_motivo_vazio(msg, sizeof msg) && strstr(msg, "Fixture"));
  // Expired hits lose priority; non-anime queries always leave anime-only last.
  Scraper list[3] = {{.id="anime", .anime=1, .ultimoHit=time(NULL)},
                     {.id="general"}, {.id="old", .ultimoHit=time(NULL)-PLUG_HIT_S-1}};
  ordenarScrapers(list, 3, 0);
  assert(!strcmp(list[0].id, "general") && !strcmp(list[1].id, "old") && !strcmp(list[2].id, "anime"));
  free(saved);
  puts("plugins_fila: ok");
  return 0;
}
