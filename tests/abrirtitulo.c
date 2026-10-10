// ABRIR TITULO SEM ESPERA (R2, owner 04/10: "demora muito para abrir um titulo
// que eu clico nas recomendacoes ... nao mostra nada, so demora e vai de uma vez").
//
// CAUSA (provada aqui): buscarEps segurava fioEpVivo ate o FIM da cauda de
// notas por temporada do TMDB (uma viagem por temporada, 20+ em serie longa).
// Abrir outro titulo nesse meio guardava o pedido da ficha em pendItem, e ele
// so saia quando a cauda do anterior acabava.
//
// O QUE ESTE TESTE PROVA:
//   1. com a cauda do titulo A ainda na rede, a ficha do titulo B (pedida em
//      seguida) chega sem esperar por ela, e o fio de A termina o que faz;
//   2. a semente (nome/ano/cartaz do clique): o titulo entra no catalogo e
//      fica pronto para abrir SEM NENHUM pedido de rede (id tt), ou com um so
//      (id do TMDB -> IMDb), e a ficha vem num unico /meta depois.
//
//   bash tests/abrirtitulo.sh
static int cinemetaLig = 1;
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return cinemetaLig; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
#include "../src/descoberta.c"
Uint32 SDL_GetTicks(void) { return 0; }
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "jellyfin_stub.inc"

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
int   ajustes_cw_proximo(void) { return 1; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
static const char *fakeDados = "";
const char *dados_dir(void)             { return fakeDados; }
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
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
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
int ajustes_cw_concluido(void) { return 90; }
unsigned recomenda_geracao(void) { return 1; }
static int atrasoTempMs;
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
  if (strstr(u, "themoviedb.org/3/tv/555/season/")) {
    if (atrasoTempMs) usleep((useconds_t)atrasoTempMs * 1000);
    return strdup("{\"episodes\":[]}");
  }
  if (strstr(u, "themoviedb.org/3/movie/278?") && tmdbFilme) return strdup(tmdbFilme);
  return NULL;   // outros addons: fora do teste
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
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


#include <time.h>
static double agoraMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}
static void *fioA(void *u) { (void)u; buscarEps(NULL); return NULL; }
static int nMeta(void) {
  int i, n = 0;
  for (i = 0; i < nPedidos && i < 64; i++) if (strstr(pedidos[i], "/meta/")) n++;
  return n;
}

int main(void) {
  // "Buscar no Cinemeta" desligado: nada do addon Cinemeta na busca (alvos e fileiras locais).
  cinemetaLig = 1;
  assert(!desc_busca_base_oculta("https://v3-cinemeta.strem.io"));
  cinemetaLig = 0;
  assert(desc_busca_base_oculta("https://v3-cinemeta.strem.io"));
  assert(desc_busca_base_oculta("https://Cinemeta-Live.example.com/x"));
  assert(!desc_busca_base_oculta("https://addon.example.com") && !desc_busca_base_oculta("") && !desc_busca_base_oculta(NULL));
  cinemetaLig = 1;

  pthread_t th;
  double t0, tB;
  nFake = 0; nRotas = 0; addonMeta = 0; fakeSoCinemeta = 0; fakeTmdbBasico = 1;
  fakeIdioma = "pt-BR";
  desc_tmdb_definir("0123456789abcdef0123456789abcdef");
  tmdbFind = "{\"tv_results\":[{\"id\":555}]}";
  tmdbTemp1 = NULL;
  atrasoTempMs = 400;                       // 2 temporadas: a cauda leva ~800 ms
  limparCacheMeta();
  { CatItem v[2];
    memset(v, 0, sizeof v);
    snprintf(v[0].imdb, sizeof v[0].imdb, "tt13293588");
    snprintf(v[0].tipo, sizeof v[0].tipo, "series");
    snprintf(v[0].titulo, sizeof v[0].titulo, "Serie A");
    snprintf(v[1].imdb, sizeof v[1].imdb, "tt0111161");
    snprintf(v[1].tipo, sizeof v[1].tipo, "movie");
    snprintf(v[1].titulo, sizeof v[1].titulo, "Filme B");
    cat_definir_tudo(v, 2, NULL, 0); }

  // 1) A com a cauda na rede; B pedido 100 ms depois, como o clique numa
  //    recomendacao dos detalhes. A ficha de B nao espera a cauda de A.
  nPedidos = 0; epItem = 0; fioEpVivo = 1; pendItem = -1;
  t0 = agoraMs();
  pthread_create(&th, NULL, fioA, NULL);
  usleep(100 * 1000);
  assert(cat_item(0)->nElenco > 0);         // a ficha de A ja esta publicada
  desc_episodios(1, 0);
  while (cat_item(1)->nElenco == 0 && agoraMs() - t0 < 5000) {
    desc_episodios_pendente();              // o que detail_atualizar faz por quadro
    usleep(10 * 1000);
  }
  tB = agoraMs() - t0;
  printf("    ficha de B na tela %.0f ms apos abrir A (cauda de A: ~800 ms)\n", tB);
  assert(cat_item(1)->nElenco > 0);
  assert(tB < 600);                         // antes do fim da cauda de A
  pthread_join(th, NULL);
  puts("ok  a ficha do proximo titulo nao espera a cauda de notas do TMDB");

#ifdef COM_SEMENTE
  // 2) Semente com id tt: pronto para abrir sem rede alguma.
  atrasoTempMs = 0; limparCacheMeta(); nRotas = 0;
  rota("/meta/series/tt0000999.json",
       "{\"meta\":{\"id\":\"tt0000999\",\"type\":\"series\",\"name\":\"Titulo Y\","
       "\"poster\":\"https://p.test/y.jpg\",\"description\":\"Sinopse Y.\",\"cast\":[\"Ator Y\"],\"videos\":[{\"season\":1,\"episode\":1,\"name\":\"Ep\"}]}}");
  nPedidos = 0;
  desc_pedir_titulo_semente("tt0000999", 0, "series", "Titulo Y", "2020", "https://p.test/y.jpg");
  { int k = desc_titulo_pronto();
    assert(k >= 0 && nPedidos == 0);        // t = 0: nenhuma ida a rede
    assert(!strcmp(cat_item(k)->titulo, "Titulo Y"));
    assert(!strcmp(cat_item(k)->poster, "https://p.test/y.jpg"));
    assert(!strcmp(cat_item(k)->meta, "2020"));
    assert(cat_item(k)->sinopse[0] == 0);   // so o que o clique sabia
    // A abertura dispara a ficha (app.c): um unico /meta, preenche o resto.
    nPedidos = 0; epItem = k; fioEpVivo = 1; pendItem = -1;
    buscarEps(NULL);
    assert(nMeta() == 1);
    assert(cat_item(k)->nElenco == 1 && !strcmp(cat_item(k)->sinopse, "Sinopse Y."));
    assert(!strcmp(cat_item(k)->titulo, "Titulo Y")); }
  puts("ok  semente tt: abre sem rede; a ficha chega num /meta so");

  // 3) Semente com id do TMDB: um pedido (external_ids) e pronto; o /meta fica
  //    para o buscarEps (antes eram external_ids + /meta serial antes de abrir).
  limparCacheMeta(); nRotas = 0;
  rota("/tv/777/external_ids", "{\"imdb_id\":\"tt0000998\"}");
  nPedidos = 0;
  desc_pedir_titulo_semente("", 777, "tv", "Titulo Z", "2021", "https://p.test/z.jpg");
  { int k, w = 0;
    while (desc_titulo_buscando() && w++ < 500) usleep(10 * 1000);
    k = desc_titulo_pronto();
    assert(k >= 0 && nMeta() == 0 && pediu("/tv/777/external_ids"));
    assert(!strcmp(cat_item(k)->imdb, "tt0000998") && cat_item(k)->tmdb == 777);
    assert(!strcmp(cat_item(k)->tipo, "series") && !strcmp(cat_item(k)->titulo, "Titulo Z")); }
  puts("ok  semente tmdb: um pedido (external_ids), titulo pronto sem esperar o /meta");

  // 4) Sem cartaz ou sem nome nao ha semente: caminho de sempre.
  limparCacheMeta(); nRotas = 0; nPedidos = 0;
  rota("/meta/movie/tt0000997.json",
       "{\"meta\":{\"id\":\"tt0000997\",\"type\":\"movie\",\"name\":\"W\","
       "\"poster\":\"https://p.test/w.jpg\",\"cast\":[],\"videos\":[]}}");
  desc_pedir_titulo_semente("tt0000997", 0, "", "W", "", "");
  { int w = 0, k;
    while (desc_titulo_buscando() && w++ < 500) usleep(10 * 1000);
    k = desc_titulo_pronto();
    assert(k >= 0 && nMeta() >= 1); }
  puts("ok  sem cartaz: cai no caminho antigo (espera o /meta)");
#endif
  puts("abrirtitulo: ok");
  return 0;
}
