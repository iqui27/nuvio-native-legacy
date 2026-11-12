// #369 item 1: "Ocultar nao lancados" vale nas fileiras da Home e na grade de colecao / Ver tudo.
// Rede falsa: uma pagina com um titulo de 2099 e um de 2020, outra so de futuros.
#include <assert.h>
#include <unistd.h>
int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_busca_cinemeta(void) { return 1; }
int ajustes_busca_nuvio(void) { return 0; }   // #311: Primeiro (padrao)
static int ocultar;
int ajustes_ocultar_nao_lancados(void) { return ocultar; }
static int ocultarVistos;
int ajustes_ocultar_filmes_vistos(void) { return ocultarVistos; }
unsigned cat_revisao_itens(void) { return 0; }               // #366: a projecao le a revisao
#include <stdlib.h>
static int vt_alloc_fail, vt_alloc_remaining = -1;
static void *vt_test_alloc(size_t n) {
  if (vt_alloc_fail || vt_alloc_remaining == 0) return NULL;
  if (vt_alloc_remaining > 0) vt_alloc_remaining--;
  return malloc(n);
}
#define NV_VT_STORE_ALLOC vt_test_alloc
#define NV_DUBLE_SESSAO
#include "../src/descoberta.c"
#include "../src/vt_store.h"
// descoberta.c passou a traduzir os rotulos que monta ("Filme", "Serie", a
// data por extenso) e este teste nao linka idioma.c: linkar puxaria
// ajustes_idioma_ingles e, atras dele, ajustes.c e o resto do app — o oposto
// do que um teste do leitor paginado deve carregar. Devolver a entrada e o que
// i18n faz com o idioma em portugues, que e o padrao, entao os rotulos que as
// asserçoes comparam sao exatamente os de producao. Mesmo stub de
// tests/colfileiras.c.
// 0 = portugues, o padrao — e o idioma em que as asserçoes deste arquivo
// escreveram os rotulos esperados.
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
// Integracao TMDB ligada por padrao — ver a nota igual em tests/colfileiras.c.
int ajustes_tmdb_ligado(void) { return 1; }
const char *ajustes_tmdb_idioma(void) { return "pt-BR"; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
// Ramo de fonte nao-addon (issue #44) e refazer da fileira CW (#38): nao sao
// o que este teste mede, mas fioVerTudo/fioContinuar referenciam os simbolos.
const char *nuvem_trakt_cliente(void) { return ""; }
char *rede_baixar_com(const char *u, int t, const char *const *c) {
  (void)c; return rede_baixar(u, t); }
int trakt_enfeitar_lote(CatItem *s, int n) { (void)s; (void)n; return 0; }
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t session) {
  (void)session; return trakt_enfeitar_lote(s, n);
}
// Origem do item (CatItem.origem): sem addons neste teste, a origem fica vazia.
int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
void cat_trocar_continuar(const CatItem *l, int q) { (void)l; (void)q; }
static int vistoId;
int cat_visto(const CatItem *c) { return c && vistoId && !strcmp(c->imdb, "tt2"); }
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *s) { (void)id; (void)tipo; (void)s; return 0; }
static ColFolder pasta;
static ColSource fontePasta;
int col_n(void) { return 1; }
const ColFolder *col_folder(int i) { return i == 0 ? &pasta : NULL; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
#define LST_SO_ITENS
#include "../src/listas.c"
int caca_n_filmes(int i) { (void)i; return 0; }
int caca_filme(int i, int j, CacaFilme *out) { (void)i; (void)j; (void)out; return 0; }
int caca_itens(int i, int falta, void *out, int max) { (void)i; (void)falta; (void)out; (void)max; return 0; }
int caca_por_chave(const char *key) { (void)key; return -1; }
void caca_previa_lista(int tipo, long alvo, const char *lista, const char *nome, const char *foto) { (void)tipo; (void)alvo; (void)lista; (void)nome; (void)foto; }
int mus_listar(MusSalva *out, int max) { (void)out; (void)max; return 0; }
int servidores_pagina(const char *key, int start, int limit, CatItem *out, int max, int *total, int *raw) { (void)key; (void)start; (void)limit; (void)out; (void)max; (void)total; (void)raw; return -1; }

char *rede_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}
static int limitePedidos, limiteVariante;
char *rede_baixar(const char *url,int timeout) {
  (void)timeout;
  if (strstr(url, "budget.invalid")) {
    const char *p = strstr(url, "skip="); int skip = p ? atoi(p + 5) : 0;
    char poster[2048], background[512], logo[512], description[900];
    memset(poster, 'p', sizeof poster - 1); poster[sizeof poster - 1] = 0;
    memset(background, 'b', sizeof background - 1); background[sizeof background - 1] = 0;
    memset(logo, 'l', sizeof logo - 1); logo[sizeof logo - 1] = 0;
    memset(description, 's', sizeof description - 1); description[sizeof description - 1] = 0;
    size_t cap = 600000; char *body = malloc(cap); assert(body);
    size_t used = (size_t)snprintf(body, cap, "{\"metas\":[");
    for (int i = skip; i < skip + 100; i++)
      used += (size_t)snprintf(body + used, cap - used,
        "%s{\"id\":\"tt%d\",\"name\":\"Item\",\"poster\":\"%s\",\"background\":\"%s\",\"logo\":\"%s\",\"description\":\"%s\"}",
        i == skip ? "" : ",", i + 1, poster, background, logo, description);
    snprintf(body + used, cap - used, "]}"); return body;
  }
  if (strstr(url, "limite.invalid")) {
    limitePedidos++;
    const char *p = strstr(url, "skip=");
    int skip = p ? atoi(p + 5) : 0;
    if (skip % 100) return NULL; /* Provider accepts only its advertised skipStep. */
    char *body = malloc  (16000);
    int used = snprintf(body, 16000, "{\"metas\":[");
    int count = skip == 1000 && limiteVariante == 3 ? 20 : 100;
    for (int pos = 0; pos < count && skip + pos < 1100; pos++) {
      int i = skip + pos;
      if (skip == 1000 && limiteVariante == 1) i++;
      if (skip == 1000 && limiteVariante == 2) i--;
      char id[32] = "";
      if (i != 1020) snprintf(id, sizeof id, "tt%d", i + 1);
      used += snprintf(body + used, 16000 - used,
        "%s{\"id\":\"%s\",\"name\":\"Item %d%s\",\"poster\":\"p.jpg\"}",
        pos ? "," : "", id, i + 1, limiteVariante == 4 && i == 1020 ? " changed" : "");
    }
    snprintf(body + used, 16000 - used, "]}");
    return body;
  }
  if(strstr(url,"skip=3"))return strdup("{\"metas\":[{\"id\":\"tt4\",\"name\":\"Quarto\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2019\"}]}");
  if(strstr(url,"skip=2"))return strdup("{\"metas\":[{\"id\":\"tt3\",\"name\":\"Terceiro\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2098\"}]}");
  return strdup("{\"metas\":[{\"id\":\"tt1\",\"name\":\"Futuro\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2099\"},{\"id\":\"tt2\",\"name\":\"Lancado\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2020\"}]}");
}
static void waitDone(void) {for(int i=0;i<2000&&desc_vertudo_carregando();i++)usleep(1000);assert(!desc_vertudo_carregando());}
static int tem(const char *id){CatItem it;for(int i=0;i<desc_vertudo_n();i++)if(desc_vertudo_item(i,&it)&&!strcmp(it.imdb,id))return 1;return 0;}
int main(void) {
  CatItem lin[8];int resp=0;
  { VtStore s = {0}; CatItem in, out;
    for (int i = 0; i < 1100; i++) {
      memset(&in, 0, sizeof in);
      snprintf(in.imdb, sizeof in.imdb, "tt%d", i + 1);
      memset(in.poster, 'p', sizeof in.poster - 1);
      memset(in.sinopse, 's', sizeof in.sinopse - 1);
      in.ranking = i + 1;
      assert(vt_store_append(&s, &in, 8u * 1024u * 1024u));
    }
    for (int i = 1099; i >= 0; i--) {
      assert(vt_store_get(&s, i, &out));
      memset(&in, 0, sizeof in);
      snprintf(in.imdb, sizeof in.imdb, "tt%d", i + 1);
      memset(in.poster, 'p', sizeof in.poster - 1);
      memset(in.sinopse, 's', sizeof in.sinopse - 1);
      in.ranking = i + 1;
      assert(!memcmp(&in, &out, sizeof in));
    }
    size_t used = s.used; int count = s.n;
    printf("vt_store: %zu requested bytes for %d items (allocator overhead excluded)\n", used, count);
    memset(&in, 0xff, sizeof in);
    assert(!vt_store_append(&s, &in, used));
    assert(s.n == count && s.used == used && vt_store_get(&s, 0, &out) && out.ranking == 1);
    vt_alloc_fail = 1;
    assert(!vt_store_append(&s, &in, SIZE_MAX));
    vt_alloc_fail = 0;
    assert(s.n == count && s.used == used && vt_store_get(&s, 1099, &out) && out.ranking == 1100);
    vt_store_free(&s);
    memset(&in, 0xff, sizeof in);
    assert(vt_store_append(&s, &in, sizeof in + 1024));
    assert(vt_store_get(&s, 0, &out) && !memcmp(&in, &out, sizeof in));
    vt_store_free(&s);
    memset(&in, 0, sizeof in);
    for (int i = 0; i < VT_STORE_MAX; i++) { in.ranking = i; assert(vt_store_append(&s, &in, 1024u * 1024u)); }
    used = s.used;
    assert(!vt_store_append(&s, &in, SIZE_MAX) && s.used == used && s.n == VT_STORE_MAX);
    assert(vt_store_get(&s, 1000, &out) && out.ranking == 1000);
    assert(vt_store_get(&s, 1, &out) && out.ranking == 1);
    vt_store_free(&s);
  }
  /* Home: lerCatalogo */
  ocultar=0;{int r=lerCatalogo("https://a.invalid","movie","rank",lin,8,8,&resp);assert(r==2);}
  ocultar=1;assert(lerCatalogo("https://a.invalid","movie","rank",lin,8,8,&resp)==1&&!strcmp(lin[0].imdb,"tt2"));
  /* Colecao / Ver tudo: desligado mostra tudo (3 paginas) */
  ocultar=0;
  desc_vertudo_abrir("https://off.invalid","movie","rank");waitDone();
  desc_vertudo_mais();waitDone();desc_vertudo_mais();waitDone();
  assert(desc_vertudo_n()==4&&tem("tt1")&&tem("tt3"));
  /* ligado: o de 2099 e o de 2098 somem; a pagina so de futuros NAO encerra a paginacao */
  ocultar=1;
  desc_vertudo_abrir("https://on.invalid","movie","rank");waitDone();
  assert(desc_vertudo_n()==1&&tem("tt2")&&!tem("tt1")&&!desc_vertudo_fim());
  desc_vertudo_mais();waitDone();
  assert(desc_vertudo_n()==1&&!desc_vertudo_fim());   /* pagina 2: so futuro */
  desc_vertudo_mais();waitDone();
  assert(desc_vertudo_n()==2&&tem("tt4")&&!tem("tt3"));
  /* Biblioteca: o mesmo buffer, mas a lista mostra o filme visto. */
  ocultarVistos = 1; vistoId = 1;
  memset(&pasta, 0, sizeof pasta); memset(&fontePasta, 0, sizeof fontePasta);
  snprintf(pasta.id, sizeof pasta.id, "pasta"); pasta.nSources = 1; pasta.sources = &fontePasta;
  snprintf(fontePasta.base, sizeof fontePasta.base, "https://bib.invalid");
  snprintf(fontePasta.type, sizeof fontePasta.type, "movie");
  snprintf(fontePasta.catId, sizeof fontePasta.catId, "rank");
  { LstLista l; CatItem it; memset(&l, 0, sizeof l); l.fonte = LST_NUVIO;
    snprintf(l.colId, sizeof l.colId, "pasta");
    lst_abrir(&l, NULL); waitDone();
    assert(lst_itens_n() == desc_vertudo_n_bruto() && lst_itens_n() == 2);
    { int i, achou = 0;
      for (i = 0; i < lst_itens_n(); i++)
        if (lst_item(i, &it) && !strcmp(it.imdb, "tt2")) achou = 1;
      assert(achou && !tem("tt2")); } }
  /* Paginated add-on: ten full pages followed by another 100 titles. */
  ocultar = ocultarVistos = 0;
  desc_vertudo_abrir("https://limite.invalid", "movie", "rank"); waitDone();
  for (int i = 1; i < 10; i++) { desc_vertudo_mais(); waitDone(); }
  assert(desc_vertudo_n() == 1000 && desc_vertudo_pagina() == 1000);
  { CatItem item; assert(desc_vertudo_item(0, &item) && !strcmp(item.imdb, "tt1"));
    assert(desc_vertudo_item(999, &item) && !strcmp(item.imdb, "tt1000")); }
  assert(limitePedidos == 10);
  vt_alloc_fail = 1;
  desc_vertudo_mais(); waitDone();
  vt_alloc_fail = 0;
  assert(desc_vertudo_erro() && !desc_vertudo_fim() && desc_vertudo_n() == 1000 && desc_vertudo_pagina() == 1000);
  vt_alloc_remaining = 50; desc_vertudo_mais(); waitDone(); vt_alloc_remaining = -1;
  assert(desc_vertudo_erro() && !desc_vertudo_fim() && desc_vertudo_n() == 1050 && desc_vertudo_pagina() == 1000);
  for (int v = 1; v <= 4; v++) {
    limiteVariante = v; desc_vertudo_mais(); waitDone();
    assert(desc_vertudo_erro() && !desc_vertudo_fim() && desc_vertudo_n() == 1050 && desc_vertudo_pagina() == 1000);
  }
  limiteVariante = 0; desc_vertudo_mais(); waitDone();
  assert(limitePedidos == 17 && !desc_vertudo_erro() && desc_vertudo_n() == 1100 && tem("tt1001"));
  { CatItem item; assert(desc_vertudo_item(1099, &item) && !strcmp(item.imdb, "tt1100"));
    assert(desc_vertudo_item(1020, &item) && !item.imdb[0] && !strcmp(item.titulo, "Item 1021"));
    assert(desc_vertudo_item(1021, &item) && !strcmp(item.imdb, "tt1022"));
    assert(desc_vertudo_item(0, &item) && !strcmp(item.imdb, "tt1"));
    ocultarVistos = 1; vistoId = 1;
    assert(desc_vertudo_n() == 1099 && desc_vertudo_item(1, &item) && !strcmp(item.imdb, "tt3"));
    assert(desc_vertudo_item(1098, &item) && !strcmp(item.imdb, "tt1100"));
    ocultarVistos = 0; assert(desc_vertudo_n() == 1100); }
  desc_vertudo_abrir("https://limite.invalid", "movie", "top10"); waitDone();
  assert(desc_vertudo_n() == 10 && desc_vertudo_fim());
  desc_vertudo_abrir("https://budget.invalid", "movie", "rank"); waitDone();
  for (int i = 0; i < 20 && !desc_vertudo_erro(); i++) { desc_vertudo_mais(); waitDone(); }
  { int n = desc_vertudo_n(); CatItem item;
    assert(desc_vertudo_erro() && !desc_vertudo_fim() && n > 0 && n < 2000 && n % 100 != 0);
    int pagina = n / 100 * 100;
    assert(desc_vertudo_pagina() == pagina && desc_vertudo_item(n - 1, &item) && item.ranking == n);
    desc_vertudo_mais(); waitDone();
    assert(desc_vertudo_erro() && desc_vertudo_n() == n && desc_vertudo_pagina() == pagina);
  }
  puts("colfiltro: PASS (home e ver tudo escondem o de 2099; pagina so de futuros nao encerra)");
  return 0;
}
