// DETALHE DE ANIME: TIPO INCERTO NAO VIRA FILME (relato do Mizikashi1, v1.4.2).
//
// O log 2043 (LG, v1.4.2) mostrava, ao abrir Mushoku Tensei (tt13293588):
//   [desc] Mushoku Tensei: Jobless Reincarnation: 3 atores, dir='Miklós Jancsó', 0 temporadas
// O item vinha de um catalogo do AIOMetadata com `type: "anime"`; buscarEps
// fazia `ehFilme = tipo != "series"` e pedia /meta/movie/tt13293588 ao
// Cinemeta, que responde com OUTRO titulo ("My Way Home", 1965, dir. Miklos
// Jancso, 3 atores, 0 videos). A chave do cache de /meta era so o id, entao a
// abertura seguinte, ja como serie, lia o corpo do filme ("meta do cache",
// 0 episodios).
//
// O QUE ESTE TESTE PROVA (fixtures reduzidas das respostas reais do Cinemeta):
//   1. item de catalogo "anime" cujo meta diz "series" entra como serie;
//   2. item "anime" sem `type` proprio: buscarEps pergunta /meta/series
//      primeiro, publica episodios e grava tipo "series" no item, sem o
//      diretor do filme homonimo, e zera um id TMDB sem tipo provado;
//   3. o cache de /meta separa filme de serie do mesmo id;
//   4. filme de verdade continua um pedido so, /meta/movie;
//   5. id que nao e do IMDb (kitsu:) nao vai ao Cinemeta.
//
//   bash tests/detalheanime.sh
#include "../src/descoberta.c"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

// --- DUBLES: nenhum participa da regra, so fazem descoberta.c linkar ---------
// (o conjunto e o de tests/colfileiras.c, menos os cat_* — que catalogo.c ja
// traz — e mais os que este teste ja tinha)
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
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
int   ajustes_tmdb_basico(void)            { return 0; }
int   ajustes_tmdb_arte(void)              { return 0; }
int   ajustes_tmdb_elenco(void)            { return 0; }
int   ajustes_tmdb_cw(void)                { return 0; }
const char *ajustes_tmdb_idioma(void)      { return "pt-BR"; }
const char *ajustes_tmdb_chave(void)       { return ""; }
void  fil_gravar_registro(void)            { }
int   fil_podar_catalogos(const char *const *ids, const char *const *bases, int n) {
  (void)ids; (void)bases; (void)n; return 0; }
int   fil_limite(void)                     { return 16; }
int   fil_oculta(const char *c)            { (void)c; return 0; }
// Dubles da escolha da cota (#126): nada escolhido na TV, e o registro dos
// catalogos fora da cota nao interessa a este teste.
int fil_escolhida(const char *c) { (void)c; return -1; }
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
// Simkl (issue #110): sem vinculo nos testes de fileira, como o Trakt acima.
int   simkl_ativo(void)                    { return 0; }
int   simkl_continuar(CatItem *s, int m)   { (void)s; (void)m; return 0; }
int   simkl_e_a_seguir(const char *id)     { (void)id; return 0; }
int   simkl_plantowatch(CatItem *s, int m) { (void)s; (void)m; return 0; }
int   ajustes_salvos_no_simkl(void)        { return 0; }
int   trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int   trakt_lista(const char *q, CatItem *s, int m) { (void)q; (void)s; (void)m; return 0; }
int   trakt_social(CatItem *s, int m)      { (void)s; (void)m; return 0; }
const char *nuvem_trakt_cliente(void)      { return ""; }
int   addons_n(void)                       { return 0; }
const char *addons_base(int i)             { (void)i; return ""; }
const char *addons_id_manifesto(int i)     { (void)i; return ""; }
const char *addons_nome(int i)            { (void)i; return "addon"; }
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
static const char *META_FILME_OK =
  "{\"meta\":{\"id\":\"tt0111161\",\"type\":\"movie\",\"name\":\"The Shawshank Redemption\","
  "\"director\":[\"Frank Darabont\"],\"cast\":[\"Tim Robbins\"],\"videos\":[]}}";

static char pedidos[16][600];
static int nPedidos;

char *rede_baixar(const char *u, int t) {
  (void)t;
  if (nPedidos < 16) snprintf(pedidos[nPedidos], sizeof pedidos[0], "%s", u);
  nPedidos++;
  if (strstr(u, "cinemeta")) {
    if (strstr(u, "/meta/movie/tt13293588.json"))  return strdup(META_FILME_ERRADO);
    if (strstr(u, "/meta/series/tt13293588.json")) return strdup(META_SERIE);
    if (strstr(u, "/meta/movie/tt0111161.json"))   return strdup(META_FILME_OK);
  }
  return NULL;   // TMDB, addons: fora do teste
}
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  (void)url; (void)imdb; (void)poster; return 1; }

static int pediu(const char *trecho) {
  int i;
  for (i = 0; i < nPedidos && i < 16; i++) if (strstr(pedidos[i], trecho)) return 1;
  return 0;
}
int arte_reserva_episodios(const char *imdb, const char *corpo) { (void)imdb; (void)corpo; return 0; }

static void limparCacheMeta(void) {
  int i;
  for (i = 0; i < META_CACHE_N; i++) { free(metaCache[i].corpo); metaCache[i].corpo = NULL; }
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

static void abrir(void) {
  nPedidos = 0;
  epItem = 0;
  fioEpVivo = 1;
  buscarEps(NULL);
}

int main(void) {
  // 1) deMeta: o `type` do item vence o "anime" do catalogo; o "type" de
  //    trailers[] (aninhado, e vem ANTES) nao conta.
  { const char *js =
      "{\"trailers\":[{\"source\":\"x\",\"type\":\"Trailer\"}],"
      "\"id\":\"tt13293588\",\"type\":\"series\",\"name\":\"Mushoku Tensei\","
      "\"poster\":\"https://p/x.jpg\"}";
    CatItem d;
    assert(deMeta(js, js + strlen(js), "anime", &d));
    assert(!strcmp(d.tipo, "series"));
    const char *js2 =
      "{\"id\":\"tt13293588\",\"name\":\"Mushoku Tensei\",\"poster\":\"https://p/x.jpg\"}";
    assert(deMeta(js2, js2 + strlen(js2), "anime", &d));
    assert(!strcmp(d.tipo, "anime"));   // sem prova, nao inventa
    const char *js3 =
      "{\"id\":\"tt1\",\"type\":\"other\",\"name\":\"X\",\"poster\":\"https://p/x.jpg\"}";
    assert(deMeta(js3, js3 + strlen(js3), "movie", &d));
    assert(!strcmp(d.tipo, "movie")); }
  puts("ok  tipo do item (movie/series) vence o do catalogo; o resto fica");

  // 2) Item "anime" sem tipo proprio: pergunta como serie, publica episodios,
  //    grava "series", nao traz o diretor do filme homonimo.
  limparCacheMeta();
  catalogoCom("tt13293588", "anime", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/series/tt13293588.json"));
  assert(!pediu("/meta/movie/tt13293588.json"));
  { const CatItem *ci = cat_item(0);
    assert(ci);
    assert(!strcmp(ci->tipo, "series"));
    assert(ci->direcao[0] == 0);
    assert(ci->nTemporadas == 2);
    assert(ci->tmdb == 0);           // 94664 sem tipo provado nao fica
    assert(cat_n_episodios(0) == 3); }
  puts("ok  anime vira serie pelo /meta: 3 episodios, 2 temporadas, sem Jancso");

  // 3) Cache por tipo: o corpo do filme guardado para o mesmo id NAO serve a
  //    serie. Simula a sequencia do log: aberto como filme, depois como serie.
  limparCacheMeta();
  catalogoCom("tt13293588", "movie", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/movie/tt13293588.json"));
  catalogoCom("tt13293588", "series", "Mushoku Tensei: Jobless Reincarnation");
  abrir();
  assert(pediu("/meta/series/tt13293588.json"));   // nao leu o do filme
  assert(cat_n_episodios(0) == 3);
  assert(cat_item(0)->direcao[0] == 0);
  puts("ok  cache do /meta separa filme e serie do mesmo id");

  // 4) Filme de verdade: um pedido, /meta/movie, tipo intacto.
  limparCacheMeta();
  catalogoCom("tt0111161", "movie", "The Shawshank Redemption");
  abrir();
  assert(nPedidos >= 1 && strstr(pedidos[0], "/meta/movie/tt0111161.json"));
  assert(!pediu("/meta/series/"));
  assert(!strcmp(cat_item(0)->tipo, "movie"));
  assert(!strcmp(cat_item(0)->direcao, "Frank Darabont"));
  puts("ok  filme continua um pedido so a /meta/movie");

  // 5) Id que nao e do IMDb nao vai ao Cinemeta (nem vira a chave "kitsu").
  limparCacheMeta();
  catalogoCom("kitsu:41370", "anime", "Mushoku Tensei");
  abrir();
  assert(!pediu("cinemeta"));
  assert(fioEpVivo == 0);
  puts("ok  id kitsu: nao pede /meta ao Cinemeta");

  // 6) As puras.
  { const char *t[2];
    assert(desc_meta_tipos("series", t) == 1 && !strcmp(t[0], "series"));
    assert(desc_meta_tipos("movie", t) == 1 && !strcmp(t[0], "movie"));
    assert(desc_meta_tipos("anime", t) == 2 && !strcmp(t[0], "series") && !strcmp(t[1], "movie"));
    assert(desc_meta_tipos("", t) == 2);
    assert(desc_meta_tem_temporadas(META_SERIE));
    assert(!desc_meta_tem_temporadas(META_FILME_ERRADO)); }
  puts("detalheanime: tudo ok");
  return 0;
}
