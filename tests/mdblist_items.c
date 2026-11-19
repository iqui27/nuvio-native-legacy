// #762 (itens): a fonte "mdblist" do vertudo. SEM REDE — descoberta.c vem
// incluido aqui (padrao de tests/collections_data.c) e rede_baixar e um
// duble que devolve a resposta de /lists/<id>/items.
//
//   bash tests/mdblist_items.sh
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
int ajustes_ocultar_nao_lancados(void) { return 0; }   // #369: descoberta.c le o ajuste
int ajustes_ocultar_filmes_vistos(void) { return 0; }  // #366: a projecao le o ajuste
unsigned cat_revisao_itens(void) { return 0; }          // #366: a projecao le a revisao
#define NV_DUBLE_SESSAO
#include "../src/descoberta.c"
// Dubles de idioma (mesmo conjunto de tests/collections_data.c).
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
int ajustes_tmdb_ligado(void) { return 1; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
// Chave do MDBList de mentira (descoberta.c so le; nunca escreve).
int extras_mdblist_chave(char *dst, unsigned n) { snprintf(dst, n, "%s", "chave-de-teste"); return 1; }
const char *nuvem_trakt_cliente(void) { return ""; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
// Pass-through: descEnfeitar (prov trakt/mdblist) devolve o que o enfeite
// aceitar; um duble que devolve 0 apagaria a lista inteira (o mesmo efeito
// que um enfeite que falha em producao nao pode ter).
int trakt_enfeitar_lote(CatItem *s, int n) { return n; }
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t session) {
  (void)s; (void)session; return n;
}
Uint32 SDL_GetTicks(void) { return 0; }
void marco(const char *name) { (void)name; }
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
void cat_trocar_continuar(const CatItem *l, int q) { (void)l; (void)q; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int cat_visto(const CatItem *item) { (void)item; return 0; }
int cat_copiar_por_id(const char *id, const char *type, CatItem *item) {
  (void)id; (void)type; (void)item; return 0;
}
int servidores_pagina(const char *key, int start, int limit, CatItem *out, int max, int *total, int *raw) {
  (void)key; (void)start; (void)limit; (void)out; (void)max;
  if (total) *total = 0; if (raw) *raw = 0; return -1;
}
#include <assert.h>
#include <string.h>
#include <unistd.h>

static char fixtureUrl[NV_ADDON_PEDIDO_MAX];
// Duas paginas: a primeira com tres filmes (um sem imdb_id, que nao entra) e
// a segunda curta, que encerra a paginacao pelo mesmo criterio do Trakt
// (pagina menor que o limite = fim). A lista "openapi" responde o exemplo
// HTTP 200 documentado do endpoint: objeto com vetores movies[] e shows[].
char *rede_baixar(const char *url, int timeout) {
  (void)timeout;
  if (strstr(url, "api.mdblist.com/lists/13914/items")) {
    snprintf(fixtureUrl, sizeof fixtureUrl, "%s", url);
    return strdup("["
      "{\"imdb_id\":\"tt0031381\",\"title\":\"Gone with the Wind\",\"year\":1939},"
      "{\"imdb_id\":\"tt0032148\",\"title\":\"The Wizard of Oz\",\"year\":1939},"
      "{\"imdb_id\":\"tt0050083\",\"title\":\"Lawrence of Arabia\",\"year\":1962},"
      "{\"title\":\"Sem imdb, ignorado\",\"year\":2001}]");
  }
  if (strstr(url, "api.mdblist.com/lists/20416/items")) {
    snprintf(fixtureUrl, sizeof fixtureUrl, "%s", url);
    return strdup("[{\"imdb_id\":\"tt0903747\",\"title\":\"Breaking Bad\",\"mediatype\":\"show\",\"year\":2008}]");
  }
  if (strstr(url, "api.mdblist.com/lists/777/items")) {
    snprintf(fixtureUrl, sizeof fixtureUrl, "%s", url);
    // Exemplo HTTP 200 do OpenAPI de /lists/{id}/items (evidencias
    // sol-c2-720/openapi-extrato.json), com um item por vetor.
    return strdup("{"
      "\"movies\":[{\"adult\":0,\"country\":\"us\",\"id\":917496,"
        "\"ids\":{\"imdb\":\"tt2049403\",\"mdblist\":\"m917496\",\"tmdb\":917496,\"tvdb\":null},"
        "\"imdb_id\":\"tt2049403\",\"language\":\"en\",\"mediatype\":\"movie\",\"rank\":1,"
        "\"release_year\":2024,\"spoken_language\":\"en\",\"title\":\"Beetlejuice Beetlejuice\","
        "\"tvdb_id\":null}],"
      "\"shows\":[{\"adult\":0,\"country\":\"us\",\"id\":258902,\"imdb_id\":\"tt20782190\","
        "\"language\":\"en\",\"mediatype\":\"show\",\"rank\":1,\"release_year\":2024,"
        "\"spoken_language\":\"en\",\"title\":\"English Teacher\",\"tvdb_id\":421968}]}");
  }
  if (strstr(url, "api.mdblist.com/lists/778/items")) {
    snprintf(fixtureUrl, sizeof fixtureUrl, "%s", url);
    // Mesmo envelope com um filme sem imdb: conta para raw mas nao entra —
    // o fim vem pela pagina curta, nao pelos validos.
    return strdup("{\"movies\":[{\"title\":\"Sem imdb\",\"release_year\":2001}],\"shows\":[]}");
  }
  return NULL;
}
char *rede_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}
static void esperar(void) {
  for (int i = 0; i < 2000 && desc_vertudo_carregando(); i++) usleep(1000);
  assert(!desc_vertudo_carregando());
}

int main(void) {
  ColSource f;
  DescColecaoResultado r;
  CatItem itens[20];
  NvPerfilSessao sessao = nv_perfil_sessao_ler();

  // 1. desc_colecao_inicial (a vitrine da colecao): so entra o que tem imdb.
  memset(&f, 0, sizeof f);
  snprintf(f.prov, sizeof f.prov, "mdblist");
  snprintf(f.catId, sizeof f.catId, "13914");
  snprintf(f.title, sizeof f.title, "Oscar Winners");
  snprintf(f.midia, sizeof f.midia, "MOVIE");
  assert(desc_colecao_inicial(&f, "", sessao, itens, 20, &r));
  assert(!r.erro);
  printf("mdblist_items: n=%d extra=%d pagina=%d mais=%d\n", r.n, r.extraN, r.pagina, r.mais);
  assert(r.n == 3);
  assert(!strcmp(itens[0].imdb, "tt0031381") && !strcmp(itens[0].titulo, "Gone with the Wind"));
  assert(!strcmp(itens[0].tipo, "movie") && !strcmp(itens[0].meta, "1939"));
  assert(!strcmp(itens[1].imdb, "tt0032148"));
  assert(!strcmp(itens[2].imdb, "tt0050083"));
  // O pedido levou a chave e o id da lista.
  printf("pedido 1: %s\n", fixtureUrl);
  assert(strstr(fixtureUrl, "api.mdblist.com/lists/13914/items"));
  assert(strstr(fixtureUrl, "apikey=chave-de-teste"));

  // 2. desc_vertudo_fonte (o "Ver tudo" da colecao): pagina, tipo e fim.
  desc_vertudo_fonte(&f); esperar();
  assert(!desc_vertudo_erro());
  printf("ver tudo: n=%d fim=%d\n", desc_vertudo_n_bruto(), desc_vertudo_fim());
  assert(desc_vertudo_n_bruto() == 3);   // os tres com imdb; o quarto nao entra
  assert(desc_vertudo_fim());            // pagina menor que o limite encerra
  { CatItem it;
    assert(desc_vertudo_item_bruto(2, &it) && !strcmp(it.imdb, "tt0050083"));
    assert(desc_vertudo_item_bruto(1, &it) && !strcmp(it.imdb, "tt0032148")); }

  // O pedido usa offset/limit (paginacao documentada do endpoint).
  assert(strstr(fixtureUrl, "offset=0&limit=50"));

  // 3. Lista de series (mediatype show) mantem o tipo.
  memset(&f, 0, sizeof f);
  snprintf(f.prov, sizeof f.prov, "mdblist");
  snprintf(f.catId, sizeof f.catId, "20416");
  snprintf(f.title, sizeof f.title, "Top Series");
  snprintf(f.midia, sizeof f.midia, "TV");
  desc_vertudo_fonte(&f); esperar();
  assert(!desc_vertudo_erro());
  { CatItem it;
    assert(desc_vertudo_n_bruto() == 1);
    assert(desc_vertudo_item_bruto(0, &it) && !strcmp(it.imdb, "tt0903747"));
    assert(!strcmp(it.tipo, "series")); }

  // 4. O corpo padrao do OpenAPI ({movies,shows}) alimenta os dois
  // carregadores reais: a Vitrine (desc_colecao_inicial) e o Ver tudo.
  // Lista mista: o filme E a serie entram juntos, na ordem do envelope
  // (a midia da fonte nao recorta mais — o tipo vem do mediatype do item).
  memset(&f, 0, sizeof f);
  snprintf(f.prov, sizeof f.prov, "mdblist");
  snprintf(f.catId, sizeof f.catId, "777");
  snprintf(f.title, sizeof f.title, "Mista");
  snprintf(f.midia, sizeof f.midia, "MOVIE");
  assert(desc_colecao_inicial(&f, "", sessao, itens, 20, &r));
  assert(!r.erro);
  printf("openapi vitrine: n=%d\n", r.n);
  assert(r.n == 2);                      // filme e serie do mesmo envelope
  assert(!strcmp(itens[0].imdb, "tt2049403"));
  assert(!strcmp(itens[0].titulo, "Beetlejuice Beetlejuice"));
  assert(!strcmp(itens[0].meta, "2024"));  // release_year do envelope
  assert(!strcmp(itens[1].imdb, "tt20782190"));
  assert(!strcmp(itens[1].tipo, "series"));

  desc_vertudo_fonte(&f); esperar();
  assert(!desc_vertudo_erro());
  printf("openapi ver tudo (movie): n=%d fim=%d\n", desc_vertudo_n_bruto(), desc_vertudo_fim());
  assert(desc_vertudo_n_bruto() == 2);
  assert(desc_vertudo_fim());
  { CatItem it;
    assert(desc_vertudo_item_bruto(0, &it) && !strcmp(it.imdb, "tt2049403"));
    assert(desc_vertudo_item_bruto(1, &it) && !strcmp(it.imdb, "tt20782190")); }

  // O mesmo envelope com midia TV tambem entrega os dois — o show na frente
  // porque a fonte e de series (a ordem segue a midia da fonte).
  snprintf(f.midia, sizeof f.midia, "TV");
  desc_vertudo_fonte(&f); esperar();
  assert(!desc_vertudo_erro());
  printf("openapi ver tudo (tv): n=%d\n", desc_vertudo_n_bruto());
  assert(desc_vertudo_n_bruto() == 2);
  { CatItem it;
    assert(desc_vertudo_item_bruto(0, &it) && !strcmp(it.imdb, "tt20782190"));
    assert(!strcmp(it.tipo, "series"));
    assert(desc_vertudo_item_bruto(1, &it) && !strcmp(it.imdb, "tt2049403")); }

  // 5. Envelope valido com itens invalidos: fim sem erro, sem crash.
  memset(&f, 0, sizeof f);
  snprintf(f.prov, sizeof f.prov, "mdblist");
  snprintf(f.catId, sizeof f.catId, "778");
  snprintf(f.title, sizeof f.title, "Vazia util");
  snprintf(f.midia, sizeof f.midia, "MOVIE");
  desc_vertudo_fonte(&f); esperar();
  assert(!desc_vertudo_erro());
  assert(desc_vertudo_n_bruto() == 0 && desc_vertudo_fim());

  puts("mdblist_items: ok");
  return 0;
}
