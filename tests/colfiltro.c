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
#define NV_DUBLE_SESSAO
#include "../src/descoberta.c"
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

char *rede_baixar_st(const char *url, int timeout, const char *const *headers, int *status) {
  (void)headers;
  char *body = rede_baixar(url, timeout);
  *status = body ? 200 : 0;
  return body;
}
char *rede_baixar(const char *url,int timeout) {
  (void)timeout;
  if(strstr(url,"skip=3"))return strdup("{\"metas\":[{\"id\":\"tt4\",\"name\":\"Quarto\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2019\"}]}");
  if(strstr(url,"skip=2"))return strdup("{\"metas\":[{\"id\":\"tt3\",\"name\":\"Terceiro\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2098\"}]}");
  return strdup("{\"metas\":[{\"id\":\"tt1\",\"name\":\"Futuro\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2099\"},{\"id\":\"tt2\",\"name\":\"Lancado\",\"poster\":\"p.jpg\",\"releaseInfo\":\"2020\"}]}");
}
static void waitDone(void) {for(int i=0;i<2000&&desc_vertudo_carregando();i++)usleep(1000);assert(!desc_vertudo_carregando());}
static int tem(const char *id){CatItem it;for(int i=0;i<desc_vertudo_n();i++)if(desc_vertudo_item(i,&it)&&!strcmp(it.imdb,id))return 1;return 0;}
int main(void) {
  CatItem lin[8];int resp=0;
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
  puts("colfiltro: PASS (home e ver tudo escondem o de 2099; pagina so de futuros nao encerra)");
  return 0;
}
