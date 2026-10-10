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
#include "jellyfin_stub.inc"

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int   ajustes_cw_concluido(void)         { return 90; }  // factory "watched" threshold
unsigned recomenda_geracao(void)         { return 0; }   // social generation: no social layer here
int   ajustes_cw_ordem(void)               { return 0; }   // Padrao (issue #127)
int   ajustes_cw_mostrar_nao_exibidos(void) { return 1; }
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
  if (strstr(u, "themoviedb.org/3/tv/555/season/")) return strdup("{\"episodes\":[]}");
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

int main(void) {
  const char *js =
    "{\"videos\":["
    "{\"id\":\"a:1\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"a:1b\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"a:2\",\"season\":1,\"episode\":2,\"name\":\"What It Takes\"},"
    "{\"id\":\"a:1c\",\"season\":1,\"episode\":1,\"name\":\"Izuku Midoriya: Origin\"},"
    "{\"id\":\"b:1\",\"season\":2,\"episode\":1,\"name\":\"Two\"}]}";
  CatEp e[16];
  int n = parsearEpisodios(js, e, 16);
  assert(n == 3);
  assert(e[0].temporada == 1 && e[0].episodio == 1 && !strcmp(e[0].vid, "a:1"));
  assert(e[1].temporada == 1 && e[1].episodio == 2);
  assert(e[2].temporada == 2 && e[2].episodio == 1);
  // Sem "episode" (todos 0): nao sao repeticoes, nenhum some.
  n = parsearEpisodios("{\"videos\":[{\"id\":\"x\",\"season\":1,\"name\":\"X\"},"
                       "{\"id\":\"y\",\"season\":1,\"name\":\"Y\"},"
                       "{\"id\":\"z\",\"season\":1,\"name\":\"Z\"}]}", e, 16);
  assert(n == 3);
  // --- LISTA DE ARQUIVOS DE ADDON DE FONTE NO LUGAR DOS EPISODIOS (2.0.3.1) ---
  // Relato de TV LG na 2.0.2 (Breaking Bad): um addon de FONTES respondia o
  // /meta com um video por ARQUIVO de torrent — 18529 entradas, quase todas
  // repeticoes dos mesmos (temporada, episodio). Ganhava da lista de 62 por
  // contagem BRUTA, o corte de 1200 valia sobre as repeticoes e a pagina
  // mostrava "Episodio 1" em todos os cartoes, "1200 de 1200 assistidos".
  {
    static const int porTemp[5] = { 7, 13, 13, 13, 16 };   // 62 episodios
    size_t cap = 4u << 20, k = 0;
    char *cine = malloc(cap), *arq = malloc(cap), *maior = malloc(cap);
    int t, ep, i, total = 0;
    assert(cine && arq && maior);
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (t = 1; t <= 5; t++)
      for (ep = 1; ep <= porTemp[t - 1]; ep++, total++)
        k += (size_t)snprintf(cine + k, cap - k,
                              "%s{\"id\":\"tt13293588:%d:%d\",\"season\":%d,\"episode\":%d,"
                              "\"name\":\"T%dE%d\"}", total ? "," : "", t, ep, t, ep, t, ep);
    snprintf(cine + k, cap - k, "]}}");
    assert(total == 62);
    // 18000 arquivos, todos repeticoes de T1E1..T1E7.
    k = (size_t)snprintf(arq, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 18000; i++)
      k += (size_t)snprintf(arq + k, cap - k,
                            "%s{\"id\":\"arq%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.1080p.mkv\"}", i ? "," : "",
                            i, i % 7 + 1, i % 7 + 1);
    snprintf(arq + k, cap - k, "]}}");
    assert(k < cap - 8);

    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    fakeMetaExterno = 0; fakeSoCinemeta = 0; fakeIdioma = "en-US";
    cineSerie = cine;
    addonResp = arq;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(pediu("addon.test/SEGREDO/meta/series/tt13293588.json"));
    printf("lista de arquivos: %d episodios, %d abas\n",
           cat_n_episodios(0), cat_item(0)->nTemporadas);
    assert(cat_n_episodios(0) == 62);
    assert(cat_item(0)->nTemporadas == 5);
    assert(cat_episodio(0, 0)->episodio == 1 && cat_episodio(0, 1)->episodio == 2);
    assert(cat_episodio(0, 61)->temporada == 5 && cat_episodio(0, 61)->episodio == 16);
    puts("ok  lista de arquivos de addon de fonte nao troca a lista de episodios");
    assert(desc_meta_n_episodios(cine) == 62);
    assert(desc_meta_n_episodios(arq) == 7);          // distintos, nao 18000

    // O corte de VIDEOS_MAX vale sobre episodios DISTINTOS: 1300 repeticoes de
    // T1E1 na frente nao gastam as vagas dos outros.
    k = (size_t)snprintf(maior, cap, "{\"videos\":[");
    for (i = 0; i < 1300; i++)
      k += (size_t)snprintf(maior + k, cap - k, "%s{\"id\":\"r%d\",\"season\":1,\"episode\":1}",
                            i ? "," : "", i);
    for (ep = 2; ep <= 80; ep++)
      k += (size_t)snprintf(maior + k, cap - k, ",{\"id\":\"e%d\",\"season\":1,\"episode\":%d}", ep, ep);
    snprintf(maior + k, cap - k, "]}");
    {
      CatEp *g = malloc(sizeof(CatEp) * VIDEOS_MAX);
      assert(g);
      n = parsearEpisodios(maior, g, VIDEOS_MAX);
      assert(n == 80);
      assert(!strcmp(g[0].vid, "r0") && g[79].episodio == 80);
      free(g);
    }
    puts("ok  corte de 1200 sobre episodios distintos");

    // Addon LEGITIMO com mais episodios DISTINTOS (e algumas repeticoes, #328)
    // continua ganhando: 62 + a temporada 6 com 10, cada episodio duas vezes.
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    total = 0;
    for (t = 1; t <= 6; t++)
      for (ep = 1; ep <= (t == 6 ? 10 : porTemp[t - 1]); ep++)
        for (i = 0; i < 2; i++, total++)
          k += (size_t)snprintf(maior + k, cap - k,
                                "%s{\"id\":\"m%d\",\"season\":%d,\"episode\":%d,\"name\":\"M\"}",
                                total ? "," : "", total, t, ep);
    snprintf(maior + k, cap - k, "]}}");
    assert(desc_meta_n_episodios(maior) == 72);
    addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 72);
    assert(cat_item(0)->nTemporadas == 6);
    puts("ok  addon com mais episodios distintos continua ganhando");
    // SEM MEMORIA PARA CRESCER O CONJUNTO, a repeticao continua sendo repeticao:
    // 600 episodios, cada um duas vezes. O conjunto de 1024 casas cabe os 600;
    // o crescimento (pedido ao passar de meia carga) falha sempre.
    k = (size_t)snprintf(maior, cap, "{\"videos\":[");
    for (i = 0; i < 1200; i++)
      k += (size_t)snprintf(maior + k, cap - k, "%s{\"id\":\"f%d\",\"season\":1,\"episode\":%d}",
                            i ? "," : "", i, i % 600 + 1);
    snprintf(maior + k, cap - k, "]}");
    callocFalha = 1; callocNegados = 0;
    n = desc_meta_n_episodios(maior);
    printf("sem memoria para crescer: %d distintos (%d callocs negados)\n", n, callocNegados);
    assert(callocNegados > 0);                 // a falha foi mesmo injetada
    assert(n == 600);
    {
      CatEp *g = malloc(sizeof(CatEp) * VIDEOS_MAX);
      assert(g);
      n = parsearEpisodios(maior, g, VIDEOS_MAX);
      assert(n == 600);
      assert(!strcmp(g[0].vid, "f0") && g[599].episodio == 600);
      free(g);
    }
    callocFalha = 0;
    puts("ok  sem memoria para crescer o conjunto, repeticao continua repeticao");

    // CATALOGO PRIMEIRO: a mesma lista de arquivos, agora na ficha do addon que
    // PUBLICOU o item (titulo aberto de um catalogo dele). A base manda no
    // texto, mas a lista de arquivos dela nao pode esconder os 62 do Cinemeta.
    addonMeta = 0; addonResp = NULL;
    nFake = 1;
    fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
    nRotas = 0;
    rota("fx.test/SEGREDO/meta/series/tt13293588.json", arq);
    limparCacheMeta();
    catalogoDe("tt13293588", "series", "Serie", "org.fx");
    abrir();
    assert(pediu("fx.test/SEGREDO/meta/series/tt13293588.json"));
    assert(pediu("cinemeta"));
    printf("catalogo primeiro, lista de arquivos: %d episodios, %d abas\n",
           cat_n_episodios(0), cat_item(0)->nTemporadas);
    assert(cat_n_episodios(0) == 62);
    assert(cat_item(0)->nTemporadas == 5);
    assert(cat_episodio(0, 61)->temporada == 5 && cat_episodio(0, 61)->episodio == 16);
    for (i = 0; i < 62; i++) assert(!strstr(cat_episodio(0, i)->nome, ".mkv"));
    assert(!strcmp(cat_episodio(0, 0)->nome, "T1E1"));
    puts("ok  catalogo primeiro: lista de arquivos da base nao esconde a lista real");
    // LISTA DE ARQUIVOS QUE SABE MAIS EPISODIOS NAO E JOGADA FORA: 20 episodios
    // distintos em 4 variantes cada (80 videos, "cara" de lista de arquivos)
    // contra 12 do Cinemeta. Sem a recusa em bloco os 8 a mais aparecem; nos 12
    // que os dois tem, o nome e o do Cinemeta e nao o nome de arquivo.
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (ep = 1; ep <= 12; ep++)
      k += (size_t)snprintf(cine + k, cap - k,
                            "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
    snprintf(cine + k, cap - k, "]}}");
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 80; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"v%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.v%d.mkv\"}", i ? "," : "",
                            i, i % 20 + 1, i % 20 + 1, i / 20);
    snprintf(maior + k, cap - k, "]}}");
    assert(desc_meta_n_episodios(cine) == 12 && desc_meta_n_episodios(maior) == 20);
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    cineSerie = cine; addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    printf("20x4 contra 12: %d episodios\n", cat_n_episodios(0));
    assert(cat_n_episodios(0) == 20);
    assert(cat_episodio(0, 19)->episodio == 20);
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1") && !strcmp(cat_episodio(0, 11)->nome, "C12"));
    puts("ok  lista de arquivos com mais episodios distintos nao e descartada");
    // O mesmo pelo caminho do catalogo primeiro.
    addonMeta = 0; addonResp = NULL;
    nFake = 1;
    fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
    rota("fx.test/SEGREDO/meta/series/tt13293588.json", maior);
    limparCacheMeta();
    catalogoDe("tt13293588", "series", "Serie", "org.fx");
    abrir();
    assert(cat_n_episodios(0) == 20);
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1") && strstr(cat_episodio(0, 19)->nome, ".mkv"));
    puts("ok  catalogo primeiro: lista de arquivos com mais episodios fica");
    // EMPATE NAO E DA LISTA DE ARQUIVOS, nem com a preferencia pela ficha do
    // addon ligada: 12 distintos x 5 variantes contra os mesmos 12.
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 60; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"w%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%02d.w%d.mkv\"}", i ? "," : "",
                            i, i % 12 + 1, i % 12 + 1, i / 12);
    snprintf(maior + k, cap - k, "]}}");
    nFake = 0; nRotas = 0; addonMeta = 1; addonResp = maior; fakeMetaExterno = 1;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    assert(cat_n_episodios(0) == 12);
    for (i = 0; i < 12; i++) assert(!strstr(cat_episodio(0, i)->nome, ".mkv"));
    assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
    fakeMetaExterno = 0;
    puts("ok  empate: a lista de arquivos nao ganha nem com a ficha do addon preferida");
    // EMPATE ACIMA DO CORTE DE 1200: o Cinemeta conhece 1300 episodios (publica
    // 1200) e a lista de arquivos os MESMOS 1300, de tras para a frente, em 4
    // variantes. 1300 contra os 1200 PUBLICADOS nao e "saber mais": a conta e
    // de distintos contra distintos, e a lista continua a do Cinemeta.
    k = (size_t)snprintf(cine, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
    for (ep = 1; ep <= 1300; ep++)
      k += (size_t)snprintf(cine + k, cap - k,
                            "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
    snprintf(cine + k, cap - k, "]}}");
    k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                         "\"name\":\"Serie\",\"videos\":[");
    for (i = 0; i < 5200; i++)
      k += (size_t)snprintf(maior + k, cap - k,
                            "%s{\"id\":\"z%d\",\"season\":1,\"episode\":%d,"
                            "\"name\":\"Serie.S01E%04d.z%d.mkv\"}", i ? "," : "",
                            i, 1300 - i % 1300, 1300 - i % 1300, i / 1300);
    snprintf(maior + k, cap - k, "]}}");
    assert(k < cap - 8);
    assert(desc_meta_n_episodios(cine) == 1300 && desc_meta_n_episodios(maior) == 1300);
    nFake = 0; nRotas = 0; addonMeta = 1; addonTipo = "/series/";
    cineSerie = cine; addonResp = maior;
    limparCacheMeta();
    catalogoCom("tt13293588", "series", "Serie");
    abrir();
    printf("1300 contra 1300x4: primeiro id %s, %d episodios\n",
           cat_episodio(0, 0)->vid, cat_n_episodios(0));
    assert(cat_n_episodios(0) == VIDEOS_MAX);
    assert(!strcmp(cat_episodio(0, 0)->vid, "tt13293588:1:1"));
    assert(!strcmp(cat_episodio(0, VIDEOS_MAX - 1)->vid, "tt13293588:1:1200"));
    assert(!strcmp(cat_episodio(0, 0)->nome, "C1"));
    puts("ok  empate acima do corte de 1200: a lista de arquivos nao ganha");
    // CATALOGO PRIMEIRO COM TRES FONTES: lista de arquivos (20), addon de
    // verdade (10), Cinemeta (12). A lista de arquivos fica (sabe mais), e o
    // nome do Cinemeta tem de entrar por cima mesmo com outra fonte no meio.
    {
      static char dez[2048], doze[2048];
      size_t kk;
      kk = (size_t)snprintf(dez, sizeof dez, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                            "\"name\":\"Serie\",\"videos\":[");
      for (ep = 1; ep <= 10; ep++)
        kk += (size_t)snprintf(dez + kk, sizeof dez - kk,
                               "%s{\"id\":\"d%d\",\"season\":1,\"episode\":%d,\"name\":\"D%d\"}",
                               ep > 1 ? "," : "", ep, ep, ep);
      snprintf(dez + kk, sizeof dez - kk, "]}}");
      kk = (size_t)snprintf(doze, sizeof doze, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                            "\"name\":\"Serie\",\"description\":\"S.\",\"videos\":[");
      for (ep = 1; ep <= 12; ep++)
        kk += (size_t)snprintf(doze + kk, sizeof doze - kk,
                               "%s{\"id\":\"tt13293588:1:%d\",\"season\":1,\"episode\":%d,"
                               "\"name\":\"C%d\"}", ep > 1 ? "," : "", ep, ep, ep);
      snprintf(doze + kk, sizeof doze - kk, "]}}");
      k = (size_t)snprintf(maior, cap, "{\"meta\":{\"id\":\"tt13293588\",\"type\":\"series\","
                           "\"name\":\"Serie\",\"videos\":[");
      for (i = 0; i < 80; i++)
        k += (size_t)snprintf(maior + k, cap - k,
                              "%s{\"id\":\"v%d\",\"season\":1,\"episode\":%d,"
                              "\"name\":\"Serie.S01E%02d.v%d.mkv\"}", i ? "," : "",
                              i, i % 20 + 1, i % 20 + 1, i / 20);
      snprintf(maior + k, cap - k, "]}}");
      addonMeta = 0; addonResp = NULL; cineSerie = doze;
      nFake = 2;
      fake[0] = (FakeAddon){ "Fontes", "https://fx.test/SEGREDO", "org.fx", "tt", 0 };
      fake[1] = (FakeAddon){ "Meta", "https://mt.test/SEGREDO", "org.mt", "tt", 0 };
      nRotas = 0;
      rota("fx.test/SEGREDO/meta/series/tt13293588.json", maior);
      rota("mt.test/SEGREDO/meta/series/tt13293588.json", dez);
      limparCacheMeta();
      catalogoDe("tt13293588", "series", "Serie", "org.fx");
      abrir();
      assert(pediu("fx.test/SEGREDO") && pediu("mt.test/SEGREDO") && pediu("cinemeta"));
      printf("tres fontes: %d episodios, primeiro nome '%s'\n",
             cat_n_episodios(0), cat_episodio(0, 0)->nome);
      assert(cat_n_episodios(0) == 20);
      for (i = 0; i < 12; i++) {
        char esperado[8];
        snprintf(esperado, sizeof esperado, "C%d", i + 1);
        assert(!strcmp(cat_episodio(0, i)->nome, esperado));
      }
      puts("ok  catalogo primeiro: nome do Cinemeta entra mesmo com outra fonte no meio");
      // LISTA DE ARQUIVOS QUE FICOU NAO E "TEXTO DO ADDON": o retorno liga
      // DESC_EPT_SO_VAZIO em buscarEps e o TMDB deixaria de traduzir os nomes,
      // prendendo os 8 nomes de arquivo que sobraram. Como em episodiosDoAddon,
      // devolve 0. (Chamada direta: o que se mede e o retorno.)
      {
        MetaFontes m3;
        TempsPub tp3 = {0};
        int r;
        memset(&m3, 0, sizeof m3);
        m3.corpo[0] = maior; m3.corpo[1] = dez; m3.corpo[2] = doze;
        m3.cine[2] = 1; m3.n = 3;
        r = episodiosDoCatalogo(0, "Serie", "tt13293588", &m3, &tp3);
        printf("lista de arquivos que ficou: retorno %d\n", r);
        assert(cat_n_episodios(0) == 20);
        assert(r == 0);
        // A base de verdade continua contando como episodios do addon.
        memset(&m3, 0, sizeof m3);
        memset(&tp3, 0, sizeof tp3);
        m3.corpo[0] = dez; m3.corpo[1] = doze; m3.cine[1] = 1; m3.n = 2;
        r = episodiosDoCatalogo(0, "Serie", "tt13293588", &m3, &tp3);
        assert(cat_n_episodios(0) == 10 && r == 1);
        assert(!strcmp(cat_episodio(0, 0)->nome, "D1"));
      }
      puts("ok  catalogo primeiro: lista de arquivos que ficou nao trava a traducao do TMDB");
    }
    // CONJUNTO CHEIO sem poder crescer: 1023 pares entram (sobra a casa vazia
    // que encerra a sondagem), repetido segue repetido, e o 1024o nunca e
    // guardado — responde "novo" toda vez, sem travar a procura.
    {
      EpSet cj = { 0 };
      callocFalha = 1; callocNegados = 0;
      for (i = 1; i <= 1023; i++) assert(epSetNovo(&cj, 1, i) == 1);
      assert(cj.n == 1023 && cj.cap == 1024);
      for (i = 1; i <= 1023; i++) assert(epSetNovo(&cj, 1, i) == 0);
      for (i = 0; i < 3; i++) assert(epSetNovo(&cj, 1, 1024) == 1);
      assert(epSetNovo(&cj, 2, 1) == 1);
      assert(cj.n == 1023 && cj.cap == 1024 && callocNegados > 0);
      assert(epSetNovo(&cj, 1, 1023) == 0);
      free(cj.v);
      callocFalha = 0;
      puts("ok  conjunto cheio sem crescer: 1023 pares, o resto nao entra e a procura termina");
    }
    nFake = 0; nRotas = 0;
    addonMeta = 0; cineSerie = NULL; addonResp = NULL;
    limparCacheMeta();
    free(cine); free(arq); free(maior);
  }
  puts("episodiosdup: ok");
  return 0;
}
