// GENERO MANUAL NA PAGINA DE COLECAO (issue #656): o chip troca o extra
// `genre` do pedido sem depender do filtro da conta.
#include "gfx.h"
#include "nvtmp.h"
#include "tex_cache.h"
#include "text.h"
TxtLinha shot_text(TxtEstilo style, const char *s, int r, int g, int b, int a, float width);
static GLuint fluxo_tex(const char *p, float w) { (void)p; (void)w; return 0; }
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
#define desc_genero_opcoes test_genre_options
#define desc_genero_obrigatorio test_genre_required
#include "../src/vertudo.c"
#undef desc_colecao_inicial
#undef desc_vertudo_n
#undef desc_vertudo_item
#undef desc_vertudo_n_bruto
#undef desc_vertudo_item_bruto
#include <assert.h>
#include <string.h>
#include "perfis.h"

void shot_background(int mode, GfxRect area, float radius, const char *url, float a) {
  (void)mode; (void)area; (void)radius; (void)url; (void)a;
}
int shot_loading(void) { return 0; }
int shot_end(void) { return 1; }
int shot_error(void) { return 0; }
void shot_more(void) {}
int shot_page(void) { return 0; }
int shot_collection(const ColSource *s, const char *base, NvPerfilSessao sessao,
                     CatItem *itens, int limite, DescColecaoResultado *r) {
  (void)s; (void)base; (void)sessao; (void)itens; (void)limite;
  if (r) memset(r, 0, sizeof *r);
  return 1;
}
const char *shot_tmdb_key(void) { return ""; }
const char *shot_trakt_client(void) { return ""; }
static char openedGenre[96];
static int openedPage, requiredGenre, optionsAvailable = 1;
void shot_filter_from(const char *base, const char *type, const char *id,
                      const char *genre, int page) {
  (void)base; (void)type; (void)id;
  openedPage = page;
  snprintf(openedGenre, sizeof openedGenre, "%s", genre ? genre : "");
}
void shot_provider_from(const ColSource *s, int page) { (void)s; (void)page; }
int shot_vt_n(void) { return 0; }
int shot_vt_item(int i, CatItem *dst) { (void)i; (void)dst; return 0; }
#undef txt_linha_corta
TxtLinha shot_text(TxtEstilo style, const char *s, int r, int g, int b, int a, float width) {
  return txt_linha_corta(style, s, r, g, b, a, width);
}

static ColFolder pasta;
static ColSource fontes[2];
int test_genre_required(const char *base, const char *tipo, const char *id) {
  (void)base; (void)tipo; (void)id; return requiredGenre;
}
int test_genre_options(const char *base, const char *tipo, const char *id,
                            char (*out)[128], int max) {
  (void)base; (void)tipo; (void)id;
  if (!optionsAvailable || !out || max < 2) return 0;
  snprintf(out[0], 128, "Action");
  snprintf(out[1], 128, "Comedy");
  return 2;
}

int main(void) {
  memset(&pasta, 0, sizeof pasta);
  memset(fontes, 0, sizeof fontes);
  snprintf(pasta.title, sizeof pasta.title, "Generos");
  snprintf(pasta.group, sizeof pasta.group, "Genres");
  snprintf(fontes[0].base, sizeof fontes[0].base, "https://addon.example");
  snprintf(fontes[0].type, sizeof fontes[0].type, "movie");
  snprintf(fontes[0].catId, sizeof fontes[0].catId, "popular");
  snprintf(fontes[0].genre, sizeof fontes[0].genre, "Action");
  pasta.sources = fontes; pasta.nSources = 1;
  vertudo_colecao(&pasta);
  assert(genTem());
  assert(genSel == 0);
  assert(!strcmp(openedGenre, "Action"));
  genAplicar(-1);
  assert(genSel == -1);
  assert(openedGenre[0] == 0);
  genAplicar(1);
  assert(genSel == 1);
  assert(!strcmp(openedGenre, "Comedy"));
  assert(openedPage == 0);
  assert(!strcmp(fontes[0].genre, "Action"));
  SDL_Event e = {0}; e.type = SDL_KEYDOWN;
  tabFocus = 0; e.key.keysym.sym = SDLK_UP; vertudo_evento(&e);
  assert(tabFocus == 3);
  e.key.keysym.sym = SDLK_RETURN; vertudo_evento(&e);
  assert(genSel == -1 && openedGenre[0] == 0);
  e.key.keysym.sym = SDLK_DOWN; vertudo_evento(&e);
  assert(tabFocus == 0);
  snprintf(fontes[0].genre, sizeof fontes[0].genre, "Custom genre");
  openSource(); assert(!strcmp(openedGenre, "Custom genre"));
  optionsAvailable = 0; openSource();
  assert(!genTem() && !strcmp(openedGenre, "Custom genre"));
  optionsAvailable = 1; requiredGenre = 1; fontes[0].genre[0] = 0;
  openSource(); assert(genSel == 0 && !strcmp(openedGenre, "Action"));
  genAplicar(-1); assert(genSel == 0);
  puts("colecao_genero: PASS");
  return 0;
}
