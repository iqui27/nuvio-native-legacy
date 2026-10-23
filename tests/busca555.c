// EPISODIO DUPLICADO NA LISTA (#328, Samsung 2.0.2): um /meta cujo videos[]
// repete (temporada, episodio) (agregador de anime com varias entradas) virava
// cartoes identicos, porque parsearEpisodios nao deduplicava. Mantem o primeiro.
//
//   bash tests/episodiosdup.sh
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
int ajustes_ocultar_filmes_vistos(void) { return 0; }    // #366: descoberta.c le o ajuste
// FALTA DE MEMORIA NO CRESCIMENTO DO CONJUNTO (EpSet): com `callocFalha` ligado,
// todo calloc de mais de 1024 itens (so o crescimento do conjunto pede isso
// neste teste) responde NULL. O primeiro, de 1024, passa.
#include <stdlib.h>
static int callocFalha, callocNegados;
static void *callocDoTeste(size_t n, size_t tam) {
  if (callocFalha && n > 1024) { callocNegados++; return NULL; }
  return calloc(n, tam);
}
#define calloc(n, tam) callocDoTeste(n, tam)
#include "../src/descoberta.c"
#undef calloc
Uint32 SDL_GetTicks(void) { return 0; }
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#define NV_TEST_SESSAO_PROPRIA
#define NV_TEST_ADDONS_PROPRIOS
#define NV_TEST_SOCIAL_PROPRIO
#include "jellyfin_stub.inc"
// Manifest loading is outside this metadata harness.
void addons_lista_travar(void) {}
void addons_lista_destravar(void) {}
void ws_destino(const char *b, const char *u, int p, char out[20]) {
  (void)b; (void)u; (void)p; out[0] = 0;
}
void addons_manifesto_lido_base(int i, const char *b, const char *d,
                               unsigned v, const char *c) {
  (void)i; (void)b; (void)d; (void)v; (void)c;
}

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_cw_minimo(void) { return 1; }
int   ajustes_cw_concluido(void)         { return 90; }  // factory "watched" threshold
unsigned recomenda_geracao(void)         { return 0; }   // social generation: no social layer here
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
static const char *fakeDados = "";
const char *dados_dir(void)             { return fakeDados; }
char *dados_caminho(char *dst, unsigned size, const char *name) {
  snprintf(dst, size, "%s/%s", fakeDados, name); return dst;
}
int ajustes_nota_episodio(int source) { (void)source; return 1; }
int ajustes_fonte_biblioteca(void) { return 0; }
const char *sessao_usuario(void)        { return ""; }
static int fakePerfil = 1;
int         perfis_ativo(void)          { return fakePerfil; }
int   ajustes_itens_fileira(void)          { return 12; }   // padrao (#163)
unsigned homeestado_geracao(void) { return 1; }
int homeestado_contexto_valido(void) { return 0; }
int homeestado_tem_fileira(const char *chave) { (void)chave; return 0; }
int homeestado_ordem_fileira(const char *chave) { (void)chave; return -1; }
int homeestado_salvar_se_geracao(const CatFileira *f, int n, unsigned g) {
  (void)f; (void)n; return g == 1;
}
int homeestado_identidade_geracao(unsigned g, char *d, unsigned z, int *p) {
  if (g != 1) return 0;
  if (d && z) d[0] = 0;
  if (p) *p = 1;
  return 1;
}
// Contexto em partes (homeestado.h, 1.4.5): constante aqui, entao nada muda
// no meio da montagem e o fim dela segue o caminho de sempre.
void homeestado_contexto(HomeContexto *c) { *c = (HomeContexto){0}; c->perfil = 1; }
int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) { (void)a; (void)b; return 0; }
const char *homeestado_mudancas_texto(int m, char *b, unsigned t) { (void)m; if (b && t) b[0] = 0; return b; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}

void  SDL_Delay(Uint32 ms)                 { usleep(ms * 1000); }
int   ajustes_cw_fonte(void)               { return 0; }
int   ajustes_tmdb_ligado(void)            { return 1; }
static int fakeMetaExterno, fakeTmdbBasico;
int   ajustes_meta_externo(void)           { return fakeMetaExterno; }
static int fakeSoCinemeta;
int   ajustes_meta_so_cinemeta(void)        { return fakeSoCinemeta; }
int   ajustes_fundo_addon(void)            { return 0; }
int   ajustes_logo_addon(void)             { return 0; }
int   ajustes_tmdb_basico(void)            { return fakeTmdbBasico; }
static int fakeTmdbArte;
int   ajustes_tmdb_arte(void)              { return fakeTmdbArte; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
static const char *fakeIdioma = "pt-BR";
const char *ajustes_tmdb_idioma(void)      { return fakeIdioma; }
const char *ajustes_tmdb_chave(void)       { return ""; }
void  fil_gravar_registro(void)            { }
int   fil_lista_e_deste_perfil(int p)       { (void)p; return 1; }
unsigned fil_perfil_geracao(void)          { return 0; }
FilPassada fil_passada_ler(void)          { FilPassada p = {0, 0}; return p; }
int   fil_passada_valida(const FilPassada *p) { (void)p; return 1; }
void  fil_registrar_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k, int n) { (void)p; (void)c; (void)t; (void)a; (void)k; (void)n; }
void  fil_registrar_se_couber_de(const FilPassada *p, const char *c, const char *t, const char *a, const char *k) { (void)p; (void)c; (void)t; (void)a; (void)k; }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                          int perfilDaLista) {
  (void)ids; (void)bases; (void)n; (void)perfilDaLista; return 0; }
int fil_marcar_sem_addon(const char *const *ids, const char *const *bases, const int *ativos, int n, int perfilDaLista) {
  (void)ids; (void)bases; (void)ativos; (void)n; (void)perfilDaLista; return 0;
}
int fil_marcar_declaradas(const char *const *chaves, int n, int perfil) {
  (void)chaves; (void)n; (void)perfil; return 0;
}
int   fil_addon_novo(const char *id, const char *base) { (void)id; (void)base; return 0; }
int   addons_perfil_da_lista(void)         { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
int   addons_ativo(int i)                  { (void)i; return 1; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
int   fil_adicionada_na_tv(const char *c) { (void)c; return 0; }
int   fil_estado_chave(const char *c) { (void)c; return -1; }
const char *fil_hero_fonte(void) { return ""; }
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
int fil_migrar_197(const char *const *c, int n) { (void)c; (void)n; return 0; }
void fil_registrar_se_couber(const char *c, const char *t, const char *a,
                             const char *tp) { (void)c; (void)t; (void)a; (void)tp; }
void  fil_registrar(const char *c, const char *t, const char *a,
                    const char *tp, int itens) {
  (void)c; (void)t; (void)a; (void)tp; (void)itens;
}
int   fil_tem_ordem(void)                  { return 0; }
int   fil_unir(const char *const *c, int n, int *s, int m) {
  int i; (void)c; for (i = 0; i < n && i < m; i++) s[i] = i; return i;
}
void  marco(const char *n)                 { (void)n; }
void  prog_chave(char *d, unsigned n, const char *c, int t, int e) {
  (void)c; (void)t; (void)e; if (n) d[0] = 0;
}
void  prog_content_id(char *d, unsigned n, const char *i, int *t, int *e) {
  (void)i; (void)t; (void)e; if (n) d[0] = 0;
}
int   prog_por_chave(const char *c, ProgRegistro *s) { (void)c; (void)s; return 0; }
// "Tirar de Continuar assistindo" (desc_tirar_continuar, tests/cwremover.sh).
void  prog_remover(const char *c)          { (void)c; }
void  prog_marcar_removido(const char *i)  { (void)i; }
int   prog_removido_vence(const char *i, long long ms) { (void)i; (void)ms; return 0; }
int   trakt_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   trakt_continuar_falhou(void)        { return 0; }
// Simkl (issue #110): sem vinculo nos testes de fileira, como o Trakt acima.
int   simkl_ativo(void)                    { return 0; }

// "A seguir" da conta (#199): so para linkar; sem vistos, nada semeia.
int   trakt_ativo(void)                    { return 1; }
int   ajustes_cw_do_episodio_mais_alto(void) { return 1; }
int   contalib_sementes_a_seguir(ContaSemente *s, int m, int a) { (void)s; (void)m; (void)a; return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_lista_cresc(const char *q, CatItem **s, int m) { (void)q; (void)s; (void)m; return 0; }
// O servico social proprio (recomenda.c) fica fora deste teste: a uniao e so o que o Trakt trouxe.
int   recomenda_social_mesclar(CatItem *i, int nTrakt, int max) { (void)i; (void)max; return nTrakt; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
// Um addon de metadados, ligado so no caso 7 (o resto do teste roda sem addon).
static int addonMeta;
// Varios addons de metadados para o catalogo primeiro (caso 20 em diante). Com
// nFake > 0 mandam eles; com 0, vale o addon unico de antes.
//   pref  : o idPrefix que o "manifesto" declara ("" = nao declara -> -1)
//   nega  : 1 = o manifesto declara que este tipo/prefixo NAO e dele (-> 0)
typedef struct { const char *nome, *base, *id, *pref; int nega; } FakeAddon;
static FakeAddon fake[4];
static int nFake;
int   addons_n(void)                       { return nFake ? nFake : (addonMeta ? 1 : 0); }
int   addons_sondado(int i)                { (void)i; return 1; }
int   addons_fornece(int i, int oque)      { (void)i; return oque == ADD_META; }
const char *addons_base(int i)             { return nFake ? fake[i].base : (addonMeta ? "https://addon.test/SEGREDO" : ""); }
const char *addons_id_manifesto(int i)     { return nFake ? fake[i].id : ""; }
const char *addons_nome(int i)            { return nFake ? fake[i].nome : "addon"; }
int   addons_aceita_id(int i, const char *t, const char *id) {
  (void)t;
  if (!nFake) return -1;
  if (fake[i].nega) return 0;
  if (!fake[i].pref[0]) return -1;
  return !strncmp(id, fake[i].pref, strlen(fake[i].pref)) ? 1 : 0;
}
unsigned addons_versao(void)             { return 1; }   // estatico no teste
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
void  addons_manifesto_lido(int i, const char *corpo) { (void)i; (void)corpo; }

// --- REDE FALSA: respostas do Cinemeta, reduzidas --------------------------
static const char *META_FILME_ERRADO =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"movie\",\"name\":\"My Way Home\","
  "\"director\":[\"Mikl\xc3\xb3s Jancs\xc3\xb3\"],"
  "\"cast\":[\"Andr\xc3\xa1s Koz\xc3\xa1k\",\"Sergey Nikonenko\",\"B\xc3\xa9la Barsi\"],"
  "\"moviedb_id\":94664,\"videos\":[]}}";
static const char *META_SERIE =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
  "\"name\":\"Mushoku Tensei: Jobless Reincarnation\",\"director\":[],"
  "\"cast\":[\"Yumi Uchiyama\",\"Tomokazu Sugita\",\"Ai Kayano\"],"
  "\"moviedb_id\":94664,\"videos\":["
  "{\"id\":\"tt13293588:1:1\",\"season\":1,\"episode\":1,\"name\":\"Jobless Reincarnation\"},"
  "{\"id\":\"tt13293588:1:2\",\"season\":1,\"episode\":2,\"name\":\"Getting Ahead of Myself\"},"
  "{\"id\":\"tt13293588:2:1\",\"season\":2,\"episode\":1,\"name\":\"The Brokenhearted Mage\"}]}}";
// O addon do usuario conhece 5 episodios da mesma serie; o Cinemeta so 3.
static const char *META_SERIE_ADDON =
  "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
  "\"name\":\"Mushoku Tensei\",\"videos\":["
  "{\"season\":1,\"episode\":1,\"name\":\"A\"},{\"season\":1,\"episode\":2,\"name\":\"B\"},"
  "{\"season\":1,\"episode\":3,\"name\":\"C\"},{\"season\":2,\"episode\":1,\"name\":\"D\"},"
  "{\"season\":2,\"episode\":2,\"name\":\"E\"}]}}";
static const char *META_FILME_OK =
  "{\"meta\":{\"id\":\"tt0111161\",\"type\":\"movie\",\"name\":\"The Shawshank Redemption\","
  "\"director\":[\"Frank Darabont\"],\"cast\":[\"Tim Robbins\"],\"videos\":[]}}";

// Meta completo, como o Cinemeta devolve (#176): descricao, fundo, logo, ano.
static const char *META_COMPLETO =
  "{\"meta\":{\"id\":\"tt0000176\",\"type\":\"movie\",\"name\":\"Filme Salvo\","
  "\"poster\":\"https://p.test/poster.jpg\",\"background\":\"https://p.test/fundo.jpg\","
  "\"logo\":\"https://p.test/logo.png\",\"description\":\"Sinopse de verdade.\","
  "\"releaseInfo\":\"2019\",\"runtime\":\"2h 1min\",\"moviedb_id\":4242,"
  "\"imdbRating\":\"8.1\",\"genres\":[\"Drama\"],\"cast\":[\"Ator Um\"],\"videos\":[]}}";

// Filme SEM elenco no Cinemeta, com sinopse em ingles: o caso em que
// fotosDoElenco saia cedo e a sinopse ficava em ingles (fix/detalhe-traducao).
static const char *META_SEM_ELENCO =
  "{\"meta\":{\"id\":\"tt0000178\",\"type\":\"movie\",\"name\":\"Movie Without Cast\","
  "\"poster\":\"https://p.test/sem-elenco.jpg\",\"description\":\"English plot.\",\"genres\":[\"Action\",\"Sci-Fi\"],\"cast\":[],\"videos\":[]}}";

// O que os addons/TMDB falsos respondem; cada caso liga o seu.
static const char *addonResp, *addonTipo = "/series/";
static const char *tmdbFind, *tmdbTemp1, *tmdbFilme, *cineSerie;
static char pedidos[64][600];
static int nPedidos;

// Rotas do catalogo primeiro: o primeiro trecho que casa responde (resp NULL =
// falha de rede). Vazio, tudo cai nas respostas fixas de antes.
static struct { const char *trecho, *resp; } rotas[8];
static int nRotas;
static void rota(const char *trecho, const char *resp) {
  rotas[nRotas].trecho = trecho; rotas[nRotas].resp = resp; nRotas++;
}

char *rede_baixar(const char *u, int t) {
  int r;
  (void)t;
  if (nPedidos < 64) snprintf(pedidos[nPedidos], sizeof pedidos[0], "%s", u);
  nPedidos++;
  for (r = 0; r < nRotas; r++)
    if (strstr(u, rotas[r].trecho)) return rotas[r].resp ? strdup(rotas[r].resp) : NULL;
  if (strstr(u, "cinemeta")) {
    if (strstr(u, "/meta/series/tt0388629.json") && cineSerie) return strdup(cineSerie);
    if (strstr(u, "/meta/movie/tt13293588.json"))  return strdup(META_FILME_ERRADO);
    if (strstr(u, "/meta/series/tt13293588.json")) return strdup(cineSerie ? cineSerie : META_SERIE);
    if (strstr(u, "/meta/movie/tt0111161.json"))   return strdup(META_FILME_OK);
    if (strstr(u, "/meta/movie/tt0000176.json"))   return strdup(META_COMPLETO);
    if (strstr(u, "/meta/movie/tt0000178.json"))   return strdup(META_SEM_ELENCO);
  }
  // O addon de metadados responde o que o teste pos em `addonResp`, no tipo que
  // o teste pos em `addonTipo` (o de serie e o padrao dos casos antigos).
  if (strstr(u, "addon.test/SEGREDO/meta/") && addonResp &&
      strstr(u, addonTipo)) return strdup(addonResp);
  // TMDB (#176): so o que o teste liga em `tmdbResp*`.
  if (strstr(u, "themoviedb.org/3/find/")) return tmdbFind ? strdup(tmdbFind) : NULL;
  if (strstr(u, "themoviedb.org/3/tv/555/season/1?") && tmdbTemp1) return strdup(tmdbTemp1);
  if (strstr(u, "themoviedb.org/3/tv/555/season/")) return strdup("{\"episodes\":[]}");
  if (strstr(u, "themoviedb.org/3/movie/278?") && tmdbFilme) return strdup(tmdbFilme);
  return NULL;   // outros addons: fora do teste
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
char *rede_baixar_st(const char *u, int t, const char *const *c, int *status) {
  char *r = rede_baixar_com(u, t, c);
  if (status) *status = r ? 200 : 0;
  return r;
}
int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

static int pediu(const char *trecho) {
  int i;
  for (i = 0; i < nPedidos && i < 64; i++) if (strstr(pedidos[i], trecho)) return 1;
  return 0;
}
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

static void limparCacheMeta(void) {
  int i;
  for (i = 0; i < META_CACHE_N; i++) { free(metaCache[i].corpo); metaCache[i].corpo = NULL; }
  metaNegLimpar();
}

static void catalogoCom(const char *imdb, const char *tipo, const char *titulo) {
  CatItem it;
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", tipo);
  snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
  // Um tmdb qualquer, como o de um catalogo que o trouxesse com o tipo errado.
  it.tmdb = 94664;
  cat_definir_tudo(&it, 1, NULL, 0);
}

// Um item de catalogo com id proprio de addon e a origem dele.
static void catalogoDe(const char *imdb, const char *tipo, const char *titulo,
                       const char *origem) {
  CatItem it;
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", imdb);
  snprintf(it.tipo, sizeof it.tipo, "%s", tipo);
  snprintf(it.titulo, sizeof it.titulo, "%s", titulo);
  snprintf(it.origem, sizeof it.origem, "%s", origem);
  cat_definir_tudo(&it, 1, NULL, 0);
}

static void abrir(void) {
  nPedidos = 0;
  epItem = 0;
  fioEpVivo = 1;
  buscarEps(NULL);
}

// #555: a busca "bigg boss" no catalogo do Nuvio devolve 14 itens e a serie de 20
// temporadas (tmdb:11436) e a ULTIMA; o corte em BUSCA_POR_ALVO (12) a perdia.
// Fixture = resposta real de catalog.nuvio.tv.
//
//   bash tests/busca555.sh
// Stubs de link (sessao fixa, sem addons): o conjunto que descoberta.c pede hoje.
uint64_t perfis_sessao_geracao(void) { return 1; }
int perfis_sessao_atual(uint64_t g) { return g == 1; }
int perfis_sessao_travar(uint64_t g) { return g == 1; }
void perfis_sessao_destravar(void) {}
int addons_travar_lista(unsigned v, int p) { (void)v; (void)p; return 1; }
void addons_destravar_lista(void) {}
int addons_copiar_identidades(AddonIdentidade *s, int m, unsigned v, int p) {
  (void)s; (void)m; (void)v; (void)p; return 0; }
int fil_podar_catalogos_de(const char *const *i, const char *const *b, int n, int p, unsigned v) {
  (void)i; (void)b; (void)n; (void)p; (void)v; return 0; }
int recomenda_social_mesclar_sessao(CatItem *i, int n, int m, uint64_t s) {
  (void)i; (void)n; (void)m; (void)s; return 0; }
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t sessao) {
  (void)s; (void)n; (void)sessao; return 0; }

static char *lerArquivo(const char *p) {
  FILE *f = fopen(p, "rb");
  char *b; long n;
  assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = malloc((size_t)n + 1);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f);
  return b;
}

int main(void) {
  char *fx = lerArquivo("tests/fixtures/busca555_bigg_boss_nuvio.json");
  AlvoBusca a;
  CatItem *r = malloc(sizeof(CatItem) * BUSCA_POR_ALVO);
  int n, i, achou = 0;
  rota("search=bigg", fx);
  memset(&a, 0, sizeof a);
  snprintf(a.tipo, sizeof a.tipo, "series");
  a.nuvio = 1;
  n = consultarAlvo(&a, "bigg boss", r, BUSCA_POR_ALVO);
  for (i = 0; i < n; i++)
    if (!strcmp(r[i].imdb, "tmdb:11436")) achou = 1;
  printf("itens=%d bigg-boss-2006=%d\n", n, achou);
  assert(achou);
  printf("OK\n");
  return 0;
}
