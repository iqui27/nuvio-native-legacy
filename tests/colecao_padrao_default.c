#include "gfx.h"
#include "nvtmp.h"
#include "text.h"
TxtLinha shot_text(TxtEstilo style, const char *s, int r, int g, int b, int a, float width) {
  return txt_linha_corta(style, s, r, g, b, a, width);
}
static int pedidosArte;
static GLuint fluxo_tex(const char *p, float w) { (void)p; (void)w; pedidosArte++; return 0; }
static void fluxo_cor(GfxRect r, float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)cr; (void)cg; (void)cb; (void)ca;
}
static void fluxo_rect(GfxRect r, GLuint t, GfxModo m, float f, float x, float y,
                       float raio, float cr, float cg, float cb, float ca) {
  (void)r; (void)t; (void)m; (void)f; (void)x; (void)y; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
static void fluxo_recorte(float x, float y, float w, float h) { (void)x; (void)y; (void)w; (void)h; }
static void fluxo_sem_recorte(void) {}
static void fluxo_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float ca) {
  (void)r; (void)raio; (void)esp; (void)cr; (void)cg; (void)cb; (void)ca;
}
#define tex_obter_larg fluxo_tex
#define gfx_cor fluxo_cor
#define gfx_rect fluxo_rect
#define gfx_recorte fluxo_recorte
#define gfx_sem_recorte fluxo_sem_recorte
#define gfx_anel fluxo_anel
#define ajustes_ocultar_filmes_vistos shot_hide_watched
#define desc_filme_visto shot_watched
#define desc_vertudo_carregando shot_loading
#define desc_vertudo_fim shot_end
#define desc_vertudo_erro shot_error
#define desc_vertudo_mais shot_more
#define desc_vertudo_filtro_desde shot_filter_from
#define desc_vertudo_fonte_desde shot_provider_from
#define desc_chave_tmdb shot_tmdb_key
#define nuvem_trakt_cliente shot_trakt_client
#define desc_vertudo_pagina shot_page
#define desc_colecao_inicial shot_collection
#define fundo_desenhar_modo shot_background
#define txt_linha_corta shot_text
#define desc_vertudo_n shot_vt_n
#define desc_vertudo_item shot_vt_item
#define desc_vertudo_n_bruto shot_vt_n
#define desc_vertudo_item_bruto shot_vt_item
#include "../src/vertudo.c"
#undef desc_colecao_inicial
#undef desc_vertudo_n
#undef desc_vertudo_item
#undef desc_vertudo_n_bruto
#undef desc_vertudo_item_bruto
#include <assert.h>

void shot_background(int mode, GfxRect area, float radius, const char *url, float a) {
  (void)mode; (void)area; (void)radius; (void)url; (void)a;
}
int shot_hide_watched(void) { return 0; }
int shot_watched(const CatItem *it) { (void)it; return 0; }
int shot_loading(void) { return 0; }
int shot_end(void) { return 1; }
int shot_error(void) { return 0; }
void shot_more(void) {}
int shot_page(void) { return 300; }
void shot_filter_from(const char *base, const char *type, const char *id,
                      const char *genre, int page) {
  (void)base; (void)type; (void)id; (void)genre; (void)page;
}
void shot_provider_from(const ColSource *s, int page) { (void)s; (void)page; }
int shot_collection(const ColSource *source, const char *base, NvPerfilSessao epoch,
                    CatItem *out, int limit, DescColecaoResultado *result) {
  (void)source; (void)base; (void)epoch; (void)out; (void)limit; (void)result;
  return 0;
}
const char *shot_tmdb_key(void) { return "collection-test-key"; }
const char *shot_trakt_client(void) { return "collection-test-client"; }

static CatItem semente[14];
static int nSemente;
int shot_vt_n(void) { return nSemente; }
int shot_vt_item(int i, CatItem *dst) {
  if (i < 0 || i >= nSemente) return 0;
  if (dst) *dst = semente[i];
  return 1;
}

static ColFolder pasta;
static ColSource fontes[2];

int main(void) {
  char dir[600], ajuste[64];
  snprintf(dir, sizeof dir, "%s/nv-colexib.XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar("deploy/app/art");
  pasta.sources = fontes;
  pasta.nSources = 1;
  snprintf(pasta.id, sizeof pasta.id, "starwars");
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  snprintf(fontes[0].title, sizeof fontes[0].title, "Filmes");
  snprintf(fontes[0].type, sizeof fontes[0].type, "movie");
  collection = &pasta;

  snprintf(ajuste, sizeof ajuste, "colecaoExibicaoPadraoLocal %d\n", VT_LAY_MURAL);
  dados_gravar("ajustes.txt", ajuste);
  ajustes_dir(dir);
  layEscolha = VT_LAY_AUTO;
  resolverLayout();
  assert(lay == VT_LAY_MURAL);
  assert(escolhaGravada(&pasta) == VT_LAY_AUTO);

  gravarEscolha(&pasta, VT_LAY_RANKING);
  layEscolha = escolhaGravada(&pasta);
  resolverLayout();
  assert(lay == VT_LAY_RANKING);
  assert(escolhaGravada(&pasta) == VT_LAY_RANKING);

  snprintf(ajuste, sizeof ajuste, "colecaoExibicaoPadraoLocal %d\n", VT_LAY_SAGA);
  dados_gravar("ajustes.txt", ajuste);
  ajustes_dir(dir);
  escolherExibicao(VT_LAY_AUTO);
  assert(lay == VT_LAY_SAGA);
  assert(layEscolha == VT_LAY_AUTO);
  assert(escolhaGravada(&pasta) == VT_LAY_AUTO);

  puts("colecao_padrao_default: ok");
  return 0;
}
