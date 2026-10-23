// CAPTURA DA PAGINA DO TITULO NO GLASS UI (ilha), para comparar LADO A LADO
// com os mockups aprovados — design/glass-ilha/glass-ilha.html, quadro
// "Detalhe" (filme, pagina inteira) e o quadro "detalhe-retomar" do mockup do
// player (serie: Retomar, temporadas no segmentado, episodios 400x225).
//
// Arte REAL do pacote (deploy/app/art): o filme e "The Devil Wears Prada 2"
// (07.jpg, o mesmo detalhe.jpg do mockup) e a serie e "Fallout" (00.jpg, com
// as stills ep/00_1_0N.jpg), que e a serie do mockup do player.
//
// Sem rede: extras.h e interceptado por #define antes do include de detail.c,
// a mesma receita de tests/detail_secoes_shot.c.
//
//   bash tests/detalhe_glass_shot.sh <saida> [id...]
//   NUVIO_SHOT_VIDRO=1 -> material vidro; sem ele, solido.
//
// Ids: filme-topo filme-explorar ("..." em foco) filme-mais serie-mais
//      filme-trailers filme-elenco filme-notas filme-comentarios
//      filme-relacionados filme-colecao filme-fim serie-topo serie-ep
//      serie-temp serie-baixo filme-comentarios-foco serie-comentarios
//      serie-comentarios-pilula
#include <SDL2/SDL.h>
#include "nvtmp.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define extras_pedir            fx_pedir
#define extras_carregando       fx_carregando
#define extras_n_temporadas     fx_n_temporadas
#define extras_temporada_numero fx_temporada_numero
#define extras_n_eps            fx_n_eps
#define extras_ep_numero        fx_ep_numero
#define extras_ep_nota          fx_ep_nota
#define extras_n_comentarios    fx_n_comentarios
#define extras_n_comentarios_ep fx_n_comentarios_ep
#define extras_comentario_usuario  fx_com_usuario
#define extras_comentario_texto    fx_com_texto
#define extras_comentario_curtidas fx_com_curtidas
#define extras_comentario_nota     fx_com_nota
#define extras_comentario_lingua   fx_com_lingua
#define extras_n_relacionados   fx_n_relacionados
#define extras_relacionado_titulo fx_relacionado_titulo
#define extras_relacionado_ano    fx_relacionado_ano
#define extras_relacionado_poster fx_relacionado_poster
#define extras_n_colecao        fx_n_colecao
#define extras_colecao_nome     fx_colecao_nome
#define extras_colecao_titulo   fx_colecao_titulo
#define extras_colecao_ano      fx_colecao_ano
#define extras_colecao_tmdb     fx_colecao_tmdb
#define extras_colecao_capa     fx_colecao_capa
#define extras_colecao_fundo    fx_colecao_fundo
#define extras_colecao_sinopse  fx_colecao_sinopse
#define extras_colecao_poster   fx_colecao_poster
#define extras_colecao_sinopse_parte fx_colecao_sinopse_parte
#define extras_colecao_nota     fx_colecao_nota
#define extras_n_estudios       fx_n_estudios
#define extras_estudio_nome     fx_estudio_nome
#define extras_estudio_logo     fx_estudio_logo
#define extras_n_trailers       fx_n_trailers
#define extras_trailer_yt       fx_trailer_yt
#define extras_trailer_nome     fx_trailer_nome
#define extras_trailer_miniatura fx_trailer_miniatura
#define extras_nota_trakt       fx_nota_trakt
#define extras_nota             fx_nota
#define extras_ep_visto         fx_ep_visto
#define extras_progresso_pronto fx_progresso_pronto
#define extras_progresso_serie  fx_progresso_serie
#define extras_proximo_episodio fx_proximo_episodio
#define extras_agenda_temporada fx_agenda_temporada
#define extras_agenda_episodio  fx_agenda_episodio
#define extras_agenda_data      fx_agenda_data
#define extras_agenda_status    fx_agenda_status
#define extras_ficha_status       fx_ficha_status
#define extras_ficha_lancamento   fx_ficha_lancamento
#define extras_ficha_digital      fx_ficha_digital
#define extras_ficha_duracao      fx_ficha_duracao
#define extras_ficha_paises       fx_ficha_paises
#define extras_ficha_classificacao fx_ficha_classificacao

#include "text.h"
static int traktLayoutCalls;
float trakt_bloco(TxtEstilo, const char *, int, int, int, float, float, float, float, float, int);
int trakt_largura(TxtEstilo, const char *);
size_t trakt_token_tam(const char *);
#define txt_bloco trakt_bloco
#define txt_largura trakt_largura
#define txt_token_tam trakt_token_tam
#include "gfx.h"
static int cardMaterial, cardFrost, captionSolid, commentMaterial;
static void card_material(GfxRect r, float radius, float density, float alpha) {
  int before = gfx_vidro_com_fonte;
  gfx_vidro_material(r, radius, density, alpha);
  cardMaterial++;
  if (r.w == 600.0f && r.h == 340.0f) commentMaterial++;
  cardFrost += gfx_vidro_com_fonte > before;
}
static void card_color(GfxRect r, float radius, float cr, float cg, float cb, float a) {
  if (cr == .19f && cg == .21f && cb == .23f && a > .5f) captionSolid++;
  gfx_cor(r, radius, cr, cg, cb, a);
}
void b2_panel(GfxRect r, float radius, float density, float a);
#define gfx_vidro_painel b2_panel
#define gfx_vidro_material card_material
#define gfx_cor card_color
#ifndef NV_DETAIL_SOURCE
#define NV_DETAIL_SOURCE "../src/detail.c"
#endif
#include NV_DETAIL_SOURCE
#undef gfx_vidro_material
#undef gfx_vidro_painel
#undef gfx_cor
#undef txt_bloco
#undef txt_largura
#undef txt_token_tam

float trakt_bloco(TxtEstilo e, const char *s, int r, int g, int b,
                   float x, float y, float w, float lead, float a, int max) {
  traktLayoutCalls++;
  return txt_bloco(e, s, r, g, b, x, y, w, lead, a, max);
}
int trakt_largura(TxtEstilo e, const char *s) {
  traktLayoutCalls++;
  return txt_largura(e, s);
}
size_t trakt_token_tam(const char *s) {
  traktLayoutCalls++;
  return txt_token_tam(s);
}

#include "dados.h"
#include "perfis.h"
#include "agendaviso.h"
#include "fontepref.h"
#include "streams.h"
#include "ilha.h"
#include "menu.h"
#include "salvospainel.h"
#include "vidro_telas.h"
#include "menu_frost_probe.h"

#define IMDB_SERIE "tt12637874"
#define IMDB_FILME "tt33612209"
#define A "deploy/app/art/"

static int ehSerieDeEnsaio(void) {
  const CatItem *ci = cat_item(idx);
  return ci && !strcmp(ci->imdb, IMDB_SERIE);
}

void fx_pedir(const char *imdb, int serie, long tmdbId) { (void)imdb; (void)serie; (void)tmdbId; }
int fx_carregando(void) { return 0; }

static const char *const COM_USU[] = { "robertaajr", "demarisp", "kfilms" };
static const char *const COM_TXT[] = {
  "A fotografia é o ponto alto: cada cena da redação parece capa de revista. "
  "O roteiro demora a engrenar, mas o terceiro ato compensa.",
  "Esperava mais da história. As atuações seguram, a Emily Blunt rouba todas "
  "as cenas em que aparece.",
  "Perfeito para uma tarde de chuva." };
static const int COM_NOTA[] = { 8, 6, 7 }, COM_CUR[] = { 6, 3, 1 };
int fx_n_comentarios(void)    { return (getenv("NUVIO_DETAIL_CHECK") && !strcmp(getenv("NUVIO_DETAIL_CHECK"), "comments")) || (getenv("NUVIO_TRAKT_CHECK") && (!strcmp(getenv("NUVIO_TRAKT_CHECK"), "perf") || !strcmp(getenv("NUVIO_TRAKT_CHECK"), "vidro"))) ? 8 : 3; }
int fx_n_comentarios_ep(void) { return 0; }
const char *fx_com_usuario(int i)  { return COM_USU[i % 3]; }
const char *fx_com_texto(int i)    {
  static const char *more[] = {
    "Fourth review. A different story, worth watching with friends.",
    "Fifth review. The score carries every scene and the ending works.",
    "Sixth review. Great actors and a beautiful setting throughout.",
    "Seventh review. A quiet story that rewards attention to detail.",
    "Eighth review. My favorite of the year, especially the final act." };
  return i < 3 ? COM_TXT[i] : more[i - 3];
}
int fx_com_curtidas(int i)         { return COM_CUR[i % 3]; }
int fx_com_nota(int i)             { return COM_NOTA[i % 3]; }
const char *fx_com_lingua(int i)   { (void)i; return ""; }

const char *fx_ficha_status(void)        { return "Released"; }
const char *fx_ficha_lancamento(void)    { return getenv("NUVIO_SHOT_AVISOFONTE") ? "2027-07-17" : "2026-05-01"; }
const char *fx_ficha_digital(void)       { return ""; }
int         fx_ficha_duracao(void)       { return 124; }
const char *fx_ficha_paises(void)        { return "United States of America"; }
const char *fx_ficha_classificacao(void) { return "12"; }

static const char *const REL_TIT[] = { "The Prestige", "Frequency", "Outcome", "The Invite",
                                       "Prisoners", "Spaceman", "The Martian" };
static const char *const REL_ANO[] = { "2006", "2000", "2026", "2026", "2013", "2024", "2015" };
static const char *const REL_PO[] = { A "poster/02.jpg", A "poster/05.jpg", A "poster/06.jpg",
  A "poster/08.jpg", A "poster/09.jpg", A "poster/10.jpg", A "poster/12.jpg" };
int fx_n_relacionados(void)   { return 7; }
const char *fx_relacionado_titulo(int i) { return REL_TIT[i]; }
const char *fx_relacionado_ano(int i) { return REL_ANO[i]; }
const char *fx_relacionado_poster(int i) { return REL_PO[i]; }

int fx_n_colecao(void)        { return ehSerieDeEnsaio() ? 0 : 2; }
const char *fx_colecao_nome(void) { return "O Diabo Veste Prada"; }
static const char *const COL_TIT[] = { "The Devil Wears Prada", "The Devil Wears Prada 2" };
static const char *const COL_ANO[] = { "2006", "2026" };
const char *fx_colecao_titulo(int i) { return COL_TIT[i]; }
const char *fx_colecao_ano(int i) { return COL_ANO[i]; }
long fx_colecao_tmdb(int i) { return 350 + i; }
const char *fx_colecao_capa(void) { return A "poster/07.jpg"; }
const char *fx_colecao_fundo(void) { return A "07.jpg"; }
const char *fx_colecao_sinopse(void) { return "Dois filmes sobre a Runway."; }
const char *fx_colecao_poster(int i) { return i ? A "poster/07.jpg" : A "poster/02.jpg"; }
const char *fx_colecao_sinopse_parte(int i) { (void)i; return "Sinopse da parte."; }
int fx_colecao_nota(int i) { return 70 + i * 5; }

static const char *const EST_NOME[] = { "20th Century Studios", "Wendy Finerman Productions", "Fox 2000" };
int fx_n_estudios(void)       { return 3; }
const char *fx_estudio_nome(int i) { return EST_NOME[i]; }
const char *fx_estudio_logo(int i) { static const char *const L[] = { "deploy/app/art/logo/00.png", "deploy/app/art/logo/07.png", "" }; return L[i]; }

static const char *const TR_NOME[] = { "Trailer oficial", "Teaser", "Por trás das câmeras", "Entrevista com o elenco" };
static const char *const TR_MINI[] = { A "07.jpg", A "13.jpg", A "30.jpg", A "22.jpg" };
int fx_n_trailers(void) { return 4; }
const char *fx_trailer_yt(int i) { (void)i; return "abc"; }
const char *fx_trailer_nome(int i) { return TR_NOME[i]; }
const char *fx_trailer_miniatura(int i) { return TR_MINI[i]; }
int fx_nota_trakt(void) { return 85; }
int fx_nota(int f) {
  switch (f) {
    case EX_IMDB: return 80; case EX_TOMATOES: return 920; case EX_METACRITIC: return 780;
    case EX_TRAKT: return 850; case EX_LETTERBOXD: return 39; default: return 0;
  }
}

int fx_n_temporadas(void) { return ehSerieDeEnsaio() ? 2 : 0; }
int fx_temporada_numero(int t) { return t + 1; }
int fx_n_eps(int t) { (void)t; return ehSerieDeEnsaio() ? 8 : 0; }
int fx_ep_numero(int t, int i) { (void)t; return i + 1; }
int fx_ep_nota(int t, int i) { return 78 + ((i * 3 + t * 5) % 12); }
int fx_ep_visto(int t, int e) { (void)t; (void)e; return 0; }
int fx_progresso_pronto(void) { return 0; }
int fx_proximo_episodio(int *t, int *e) { (void)t; (void)e; return 0; }
int fx_progresso_serie(int *v, int *x) { (void)v; (void)x; return 0; }
int fx_agenda_temporada(void) { return 0; }
int fx_agenda_episodio(void)  { return 0; }
const char *fx_agenda_data(void) { return ""; }
const char *fx_agenda_status(void) { return ehSerieDeEnsaio() ? "returning series" : "Released"; }

// --- FRASES (cache de disco, o formato de seriefrases.c; sem rede) ---------
static void cacheFrases(void) {
  char buf[2048];
  size_t k = 0;
  k += (size_t)snprintf(buf + k, sizeof buf - k, "# nuvio seriefrases v1\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "%lld\tO Diabo Veste Prada 2\t1\n",
                        (long long)time(NULL));
  k += (size_t)snprintf(buf + k, sizeof buf - k, "F\tIdioma original\tInglês\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "F\tOrçamento\tUS$ 100 milhões\n");
  k += (size_t)snprintf(buf + k, sizeof buf - k, "Q\tMiranda Priestly\tPor favor, não confunda isto "
                        "com uma volta. É uma correção de rota.\n");
  for (int i = 2; i <= 6; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "Q\tAndy Sachs\tFrase de ensaio %d.\n", i);
  assert(dados_gravar_leve("frases-" IMDB_FILME ".txt", buf));
}

// --- CATALOGO ----------------------------------------------------------------
static CatItem itens[2];
static CatEp episodios[16];
static const char *const EP_NOME[8] = { "The End", "The Target", "The Head", "The Ghouls",
  "The Past", "The Trap", "The Radio", "The Beginning" };

static void montarCatalogo(void) {
  CatFileira fil;
  int i, n = 0;
  memset(itens, 0, sizeof itens);
  memset(&fil, 0, sizeof fil);
  // SERIE: Fallout, assistindo o T1E3 (58%), E1 e E2 vistos.
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Fallout");
  snprintf(itens[0].imdb, sizeof itens[0].imdb, IMDB_SERIE);
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "series");
  snprintf(itens[0].genero, sizeof itens[0].genero, "Programa de TV  \xc2\xb7  Ação  \xc2\xb7  Aventura");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2024  \xc2\xb7  56 min");
  snprintf(itens[0].classificacao, sizeof itens[0].classificacao, "14");
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Num futuro pós-apocalíptico, os moradores de abrigos subterrâneos "
           "precisam voltar à superfície de Los Angeles.");
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop, A "00.jpg");
  snprintf(itens[0].poster, sizeof itens[0].poster, A "poster/00.jpg");
  snprintf(itens[0].logo, sizeof itens[0].logo, A "logo/00.png");
  itens[0].nota = 83;
  itens[0].temporadas[0] = 1; itens[0].temporadas[1] = 2; itens[0].nTemporadas = 2;
  itens[0].progresso = 58; itens[0].temporada = 1; itens[0].episodio = 3;
  itens[0].restanteMin = 24;
  { static const char *const N[] = { "Ella Purnell", "Aaron Moten", "Walton Goggins",
                                     "Kyle MacLachlan", "Moisés Arias", "Xelia Mendes-Jones" };
    for (i = 0; i < 6; i++) {
      snprintf(itens[0].elenco[i].nome, sizeof itens[0].elenco[i].nome, "%s", N[i]);
      snprintf(itens[0].elenco[i].papel, sizeof itens[0].elenco[i].papel, "Papel %d", i + 1);
      snprintf(itens[0].elenco[i].foto, sizeof itens[0].elenco[i].foto, A "elenco/00_%d.jpg", i);
    } }
  itens[0].nElenco = 6;
  // FILME: The Devil Wears Prada 2 (o detalhe.jpg do mockup).
  snprintf(itens[1].titulo, sizeof itens[1].titulo, "O Diabo Veste Prada 2");
  snprintf(itens[1].imdb, sizeof itens[1].imdb, IMDB_FILME);
  snprintf(itens[1].tipo, sizeof itens[1].tipo, "movie");
  snprintf(itens[1].genero, sizeof itens[1].genero, "Filme  \xc2\xb7  Drama  \xc2\xb7  Comédia");
  snprintf(itens[1].meta, sizeof itens[1].meta, "2026  \xc2\xb7  124 min");
  snprintf(itens[1].classificacao, sizeof itens[1].classificacao, "12");
  snprintf(itens[1].sinopse, sizeof itens[1].sinopse,
           "Quase vinte anos depois, Miranda Priestly enfrenta o fim das revistas "
           "impressas, e Andy, agora editora, volta ao prédio onde tudo começou "
           "para tentar salvá-la.");
  snprintf(itens[1].direcao, sizeof itens[1].direcao, "David Frankel");
  snprintf(itens[1].backdrop, sizeof itens[1].backdrop, A "07.jpg");
  snprintf(itens[1].poster, sizeof itens[1].poster, A "poster/07.jpg");
  snprintf(itens[1].logo, sizeof itens[1].logo, A "logo/07.png");
  snprintf(itens[1].pais, sizeof itens[1].pais, "United States");
  itens[1].nota = 80;
  itens[1].tmdb = 351;
  { static const char *const N[] = { "Meryl Streep", "Anne Hathaway", "Emily Blunt",
                                     "Stanley Tucci", "Kenneth Branagh", "Lucy Liu" };
    static const char *const P[] = { "Miranda Priestly", "Andy Sachs", "Emily Charlton",
                                     "Nigel", "", "" };
    for (i = 0; i < 6; i++) {
      snprintf(itens[1].elenco[i].nome, sizeof itens[1].elenco[i].nome, "%s", N[i]);
      snprintf(itens[1].elenco[i].papel, sizeof itens[1].elenco[i].papel, "%s", P[i]);
      snprintf(itens[1].elenco[i].foto, sizeof itens[1].elenco[i].foto, A "elenco/07_%d.jpg", i);
    } }
  itens[1].nElenco = 6;

  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);

  for (int t = 1; t <= 2; t++)
    for (i = 0; i < 8; i++) {
      CatEp *e = &episodios[n++];
      e->temporada = t; e->episodio = i + 1;
      snprintf(e->nome, sizeof e->nome, "%s", EP_NOME[i]);
      snprintf(e->duracao, sizeof e->duracao, "56 min");
      snprintf(e->data, sizeof e->data, "11 de abril de 2024");
      snprintf(e->sinopse, sizeof e->sinopse, "%s",
               i == 2 ? "Lucy, Maximus e o Ghoul disputam a mesma recompensa no deserto."
                      : "Sinopse do episódio.");
      snprintf(e->thumb, sizeof e->thumb, A "ep/00_1_%02d.jpg", (t == 1 ? i : 7 - i) + 1);
    }
  cat_definir_episodios(0, episodios, n);
  for (i = 1; i <= 8; i++) vistoep_definir(IMDB_SERIE, 1, i, i <= 2);
}

// --- CAPTURA -----------------------------------------------------------------
static SDL_Window *janela;
static const char *saida;

static void gravar(const char *id) {
  char nome[700];
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  snprintf(nome, sizeof nome, "%s/%s.bmp", saida, id);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  SDL_SaveBMP(s, nome);
  snprintf(nome, sizeof nome, "%s/%s.png", saida, id);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  // NUVIO_SHOT_MENU: blur proof. Fraction of pixels with a |horizontal luma step| > 6 over the empty
  // right strip of the open sidebar (x 330..410, y 175..780, no labels there):
  // page text/pills seen through a flat tint give a high value, frost a low one.
  if (getenv("NUVIO_SHOT_MENU")) {
    menu_probe_check(pix);
  }
  free(pix);
  printf("captura: %s\n", nome);
  // A rolagem do documento, para montar a pagina inteira (lado a lado com o
  // quadro alto do mockup, 1920x4230).
  printf("rolagem %s %.0f\n", id, scrollY);
  if (getenv("NUVIO_TRAKT_SHOT")) {
    printf("Trakt material: glass=%d frost=%d sampled-panels=%d\n",
           ajustes_vidro(), ajustes_vidro_fosco(), gfx_vidro_com_fonte);
    if (ajustes_vidro() && ajustes_vidro_fosco()) assert(gfx_vidro_com_fonte > 0);
  }
}

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents();
    tex_bombear(3);
    vt_antes();
    detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    // NUVIO_SHOT_RELOGIO=1: the clock island over the page, as app.c draws it.
    if (getenv("NUVIO_SHOT_RELOGIO")) {
      ilha_relogio_visivel(1); ilha_posicionar(1); ilha_desenhar(SDL_GetTicks());
    }
    // NUVIO_SHOT_PAINEL=1: o painel de Salvos/Avisos aberto POR CIMA da pagina,
    // como app.c o desenha agora (sem o `!detail_aberto()` de antes).
    if (getenv("NUVIO_SHOT_PAINEL")) {
      if (!spainel_aberto()) spainel_abrir();
      spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
      if (spainel_visivel()) spainel_desenhar(SDL_GetTicks());
    }
    // NUVIO_SHOT_MENU=1: the sidebar opened over the page (menu_abrir_sobre), as app.c draws it.
    if (getenv("NUVIO_SHOT_MENU")) {
      if (!menu_aberto()) { menu_iniciar(); menu_abrir_sobre(1); }
      menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
      menu_probe_draw();
      menu_desenhar(SDL_GetTicks());
      if (getenv("NUVIO_SHOT_MENU_OPAQUE")) gfx_cor((GfxRect){330, 175, 81, 605}, 0, .1f, .1f, .1f, 1);
      if (i == n - 1) printf("menu-fonte: com_fonte=%d capturas=%d\n", gfx_vidro_com_fonte, gfx_vidro_capturas);
    }
    // The sources sheet over the page, as app.c draws it.
    if (stream_folha_aberta()) { stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks()); stream_folha_desenhar(SDL_GetTicks()); }
    vt_depois();
    if (i < n - 1) SDL_GL_SwapWindow(janela);
  }
}

// nv = nivel; sec/col = foco na pagina (nv 1) ou botao (nv 0, col).
static void abrir(int i, int nv, int sec, int col) {
  HomeItem hi;
  memset(&hi, 0, sizeof hi);
  hi.indice = i;
  hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
  hi.titulo = itens[i].titulo; hi.genero = itens[i].genero; hi.meta = itens[i].meta;
  detail_abrir(&hi);
  quadros(2);
  nivel = nv;
  if (nv == 0) botao = col;
  else { foco.fileira = sec; foco.coluna = col; }
  quadros(160);
  // Os quadros daqui sao muito mais rapidos que os da TV: o esvanecer do logo
  // (260 ms, SDL_GetTicks) ainda nao terminou. Espera e assenta.
  SDL_Delay(400);
  quadros(4);
}

#include "traktcoment_checks.h"
#include "detalheglass_checks.h"
#include "detalhe_espaco_checks.h"
#include "detalhe_b2_checks.h"
#ifdef NV_SHOT_HOOKS
#include "vidro_c9_cache.h"
#endif

static int quer(int argc, char **argv, const char *id) {
  if (argc <= 2) return 1;
  for (int i = 2; i < argc; i++) if (!strcmp(argv[i], id)) return 1;
  return 0;
}

int main(int argc, char **argv) {
  SDL_GLContext gl;
  const char *dd;
  const char *config = getenv("NUVIO_DETAIL_CONFIG");
  int width = 1920, height = 1080;
  if (config) {
    if (!strcmp(config, "720-frost")) { width = 1280; height = 720; }
    else if (!strcmp(config, "2160-frost")) { width = 3840; height = 2160; }
    else if (strcmp(config, "1080-frost") && strcmp(config, "1080-clear") && strcmp(config, "1080-minimal")) return 2;
  }
  saida = argc > 1 ? argv[1] : nvt("/nuvio-detglass");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: detalhe glass", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, width, height,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, width, height);
  gfx_tamanho_alvo(width, height);
  assert(gfx_iniciar());
  { char ic[1024]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-detglass-dados")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria (%s)\n", dd ? dd : "");
    return 1;
  }
  { char caminho[600]; FILE *f;
    const char *lg = getenv("NUVIO_SHOT_IDIOMA");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dd);
    f = fopen(caminho, "w"); assert(f);
    if (lg && *lg) fprintf(f, "idioma %d\n", atoi(lg));
    // Acento azul do mockup (#5aa2ff/#42a5f5): o tema Oceano.
    fputs("selected_theme 2\n", f);
    if (getenv("NUVIO_SHOT_LAYOUT")) fprintf(f, "homeLayoutLocal %d\n", atoi(getenv("NUVIO_SHOT_LAYOUT")));
    if (getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO")))
      fputs("vidroLocal 0\n", f);
    else fputs("vidroLocal 1\n", f);
    // Ajustes > Aparencia > Fundo: 0 Arte, 1 Arte borrada, 2 Frost.
    // NUVIO_SHOT_FONTE=3: Montserrat, the interface font of the owner's TV.
    if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
    if (getenv("NUVIO_SHOT_FUNDO")) fprintf(f, "fundoLocal %d\n", atoi(getenv("NUVIO_SHOT_FUNDO")));
    // NUVIO_SHOT_BORDA=1: "Borda no cartaz em foco" off (focus grows instead of a ring).
    if (getenv("NUVIO_SHOT_BORDA")) fprintf(f, "bordaFocoCartaz %d\n", atoi(getenv("NUVIO_SHOT_BORDA")));
    fclose(f);
    ajustes_dir(dd); }
  ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_FOSCO / _OPAC
  // First-run appearance migration overrides the file; apply shot settings after it.
  if (getenv("NUVIO_TRAKT_SHOT") || getenv("NUVIO_TRAKT_CHECK") || getenv("NUVIO_DETAIL_CHECK") || getenv("NUVIO_DETAIL_SHOT")) {
    ajustes_definir_vidro(getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO")));
    assert(ajustes_vidro() == (getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO"))));
  }
  if (config) {
    setenv("NUVIO_SHOT_VIDRO_FOSCO", !strcmp(config, "1080-clear") ? "0" : "1", 1);
    ajustes_teste_vidro_env();
    gfx_definir_efeitos_minimos(!strcmp(config, "1080-minimal"));
    int w, h;
    gfx_tamanho_tela(&w, &h);
    assert(w == width && h == height);
    printf("PASS: renderer config=%s drawable=%dx%d frost=%d minimal=%d GL=%s (host only)\n",
           config, w, h, ajustes_vidro_fosco(), gfx_efeitos_minimos(), glGetString(GL_VERSION));
  }
  if (vt_ligado()) vt_vidro();
  // As marcas das notas (IMDb, Rotten Tomatoes, Trakt) vem da pasta de arte.
  { char ar[1024]; if (realpath("deploy/app/art", ar)) extras_carregar(ar); }
  montarCatalogo();
  cacheFrases();
#ifdef NV_SHOT_HOOKS
  if (getenv("NUVIO_C9_CACHE")) return vc_run();
#endif
  if (getenv("NUVIO_TRAKT_CHECK")) return trakt_checks();
  if (getenv("NUVIO_DETAIL_CHECK")) {
    const char *m = getenv("NUVIO_DETAIL_CHECK");
    if (!strcmp(m, "comments")) return comments_checks();
    if (!strcmp(m, "cards")) return card_checks();
    if (!strcmp(m, "eps")) return eps_checks();
    if (!strcmp(m, "espaco")) return espaco_checks();
    if (!strcmp(m, "b2")) return b2_checks();
    return strcmp(m, "mais") == 0 ? mais_checks() : rows_checks();
  }
  // NUVIO_SHOT_AMIGOS=1: amigos falsos (feed externo) no filme e na serie —
  // o que a ilha e a lista "O que os amigos acharam" mostram (06/10/2026):
  // quem recomendou, reacao, nota de tracker, progresso, "parou" e "terminou".
  if (getenv("NUVIO_SHOT_AMIGOS")) {
    static SvEvento e[12];
    long long ag = (long long)time(NULL);
    int n = 0;
#define AMG(pid, nome, av, im, ac) do { memset(&e[n], 0, sizeof e[n]); e[n].pct = -1; e[n].restanteMin = -1; \
      e[n].reacao = SV_REAC_NADA; e[n].quando = ag - 600 * n; e[n].acao = (ac); \
      snprintf(e[n].pessoaId, 96, "%s", pid); snprintf(e[n].pessoaNome, 64, "%s", nome); \
      snprintf(e[n].pessoaAvatar, 512, "%s", av); snprintf(e[n].imdb, 24, "%s", im); \
      snprintf(e[n].tipo, 8, "%s", !strcmp(im, IMDB_SERIE) ? "series" : "movie"); } while (0)
    AMG("nuvio:ana", "Ana", A "elenco/00_0.jpg", IMDB_FILME, SV_MANDOU);
    snprintf(e[n].texto, sizeof e[n].texto, "Imperdível, vai por mim"); n++;
    AMG("nuvio:mari", "Mari", A "elenco/00_1.jpg", IMDB_FILME, SV_REACAO); e[n].reacao = SV_REAC_GOSTOU; n++;
    AMG("nuvio:mari", "Mari", A "elenco/00_1.jpg", IMDB_FILME, SV_FIM); n++;
    AMG("trakt:rafa", "Rafa", "", IMDB_FILME, SV_AVALIOU); e[n].nota = 80; n++;
    AMG("nuvio:fabi", "Fabi", A "elenco/00_2.jpg", IMDB_FILME, SV_REACAO); e[n].reacao = SV_REAC_MEIO; n++;
    AMG("nuvio:leo", "Leo", "", IMDB_FILME, SV_FIM); n++;
    AMG("nuvio:mari", "Mari", A "elenco/00_1.jpg", IMDB_SERIE, SV_AGORA); e[n].temporada = 2; e[n].episodio = 3; n++;
    AMG("nuvio:rafa", "Rafa", "", IMDB_SERIE, SV_FIM); e[n].temporada = 2; e[n].episodio = 5; n++;
    AMG("nuvio:bia", "Bia", A "elenco/00_3.jpg", IMDB_SERIE, SV_FIM); e[n].temporada = 2; e[n].episodio = 8;
    e[n].reacao = SV_REAC_GOSTOU; n++;
    AMG("nuvio:duda", "Duda", "", IMDB_SERIE, SV_FIM); e[n].temporada = 1; e[n].episodio = 3;
    e[n].quando = ag - 34 * 86400LL; n++;
#undef AMG
    socialvis_definir_feed(e, n);
    { SvPerfil p; memset(&p, 0, sizeof p);
      p.gostoPct = 82; p.gostoTotal = 11; p.estado = SV_PERFIL_OK;
      socialvis_definir_perfil_extra("nuvio:mari", &p); }
  }

  // NUVIO_VIDRO_TELAS=<dir> (tests/vidro_telas.sh): glass budget with the
  // sources sheet open and with the Trakt comments row focused.
  if (vt_ligado()) {
    static const char *JSON = "{\"streams\":["
      "{\"url\":\"https://x.invalid/1.mkv\",\"name\":\"AIO 4K\",\"description\":\"2160p WEB-DL DV HDR10\"},"
      "{\"url\":\"https://x.invalid/2.mkv\",\"name\":\"Torrentio 1080p\",\"title\":\"Movie.1080p.WEB-DL.mkv\"},"
      "{\"url\":\"https://x.invalid/3.mkv\",\"name\":\"Comet 720p\",\"description\":\"720p WEB\"}]}";
    Stream *v = NULL;
    int n = stream_extrair(JSON, "AIOStreams", &v);
    SDL_Event e;
    abrir(1, 1, SEC_COMENTARIOS, 0);
    vt_zerar(); quadros(30); vt_png("detalhe-comentarios"); vt_relatar("detail-comments", 1);
    memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN;
    for (int k = 0; k < 4; k++) { e.key.keysym.sym = k & 1 ? SDLK_LEFT : SDLK_RIGHT; detail_evento(&e); quadros(5); }
    vt_relatar("detail-comments", 0);
    stream_definir_alvo(IMDB_FILME); stream_definir_lista(v, n);
    stream_folha_nome("O Diabo Veste Prada 2"); stream_folha_abrir();
    quadros(20);
    vt_zerar(); quadros(60); vt_png("detalhe-fontes"); vt_relatar("detail-sources", 1);
    for (int k = 0; k < 4; k++) { e.key.keysym.sym = k & 1 ? SDLK_UP : SDLK_DOWN; stream_folha_evento(&e); quadros(5); }
    vt_relatar("detail-sources", 0);
    return vtFalhas ? 1 : 0;
  }

  if (getenv("NUVIO_SHOT_AVISOFONTE")) {
    agenda_iniciar();
    snprintf(itens[1].titulo, sizeof itens[1].titulo, "The Odyssey");
    itens[1].logo[0] = 0;
    snprintf(itens[1].sinopse, sizeof itens[1].sinopse, "A journey home after the Trojan War. Coming soon.");
    CatFileira movieFil = {0}; movieFil.ini = 0; movieFil.n = 2;
    snprintf(movieFil.titulo, sizeof movieFil.titulo, "Movies");
    snprintf(movieFil.tipo, sizeof movieFil.tipo, "movie");
    cat_definir_tudo(itens, 2, &movieFil, 1);
    abrir(1, 0, 0, 0);
    assert(temLembrar());
    maisAcoes = 1; botao = 1 + (temInicio() ? 1 : 0);
    SDL_Delay(450); quadros(40); gravar("movie-alert-rest");
    int lemb = -1;
    for (int i = 0; i < nBotoes(); i++) if (acaoEm(i) == ACAO_LEMBRAR) lemb = i;
    assert(lemb >= 0); botao = lemb; quadros(40); gravar("movie-alert-focus");
    agenda_filme_alternar(IMDB_FILME, itens[1].titulo, itens[1].poster);
    quadros(40); gravar("movie-alert-active");
    // Feed the real persisted available state, as the worker publishes it.
    char registro[2600], nome[64];
    snprintf(nome, sizeof nome, "lembretes-p%d.txt", perfis_ativo());
    snprintf(registro, sizeof registro, "%s\t\t0\t\t1\t1\t100000\t%s\t%s\n",
             IMDB_FILME, itens[1].titulo, itens[1].poster);
    dados_gravar(nome, registro);
    perfis_definir_ativo(2); agenda_iniciar(); perfis_definir_ativo(1); agenda_iniciar();
    agendaviso_abrir_sessao(); agendaviso_mostrar_se_houver();
    assert(agendaviso_aberto());
    home_iniciar("deploy/app/art");
    cat_definir_tudo(itens, 2, &movieFil, 1);
    for (int i = 0; i < 90; i++) {
      txt_novo_quadro(); tex_novo_quadro(); tex_bombear(32); gfx_novo_quadro();
      glClear(GL_COLOR_BUFFER_BIT);
      home_atualizar(1.0f / 60.0f, SDL_GetTicks()); home_desenhar(SDL_GetTicks());
      agendaviso_atualizar(1.0f / 60.0f, SDL_GetTicks()); agendaviso_desenhar(SDL_GetTicks());
      if (i < 89) SDL_GL_SwapWindow(janela);
    }
    gravar("home-movie-available");
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    agendaviso_evento(&e);
    AgItem aviso; assert(agendaviso_tomar_filme(&aviso));
    assert(!strcmp(aviso.imdb, IMDB_FILME)); assert(!agenda_lembrete(IMDB_FILME));
    return 0;
  }
  if (quer(argc, argv, "filme-topo")) { abrir(1, 0, 0, 0); gravar("filme-topo"); }
  // O ULTIMO circular da linha aberta em foco: hoje o "..." (Mais opcoes).
  if (quer(argc, argv, "filme-explorar")) {
    abrir(1, 0, 0, 0);
    maisAcoes = 1; botao = nBotoesTodos() - 1;
    // A linha abre com mola em tempo REAL (SDL_GetTicks): quadros espacados.
    for (int k = 0; k < 50; k++) { quadros(1); SDL_Delay(16); }
    quadros(4);
    gravar("filme-explorar");
  }
  // "MAIS OPCOES" (06/10/2026): OK no "..." abre a ilha com Trailer, Explorar,
  // Trocar arte (e Recomendar com o servico), foco na segunda linha.
  if (quer(argc, argv, "filme-mais") || quer(argc, argv, "serie-mais")) {
    int k, serie;
    for (serie = 0; serie < 2; serie++) {
      int d[DMAIS_N];
      if (!quer(argc, argv, serie ? "serie-mais" : "filme-mais")) continue;
      abrir(serie ? 0 : 1, 0, 0, 0);
      maisAcoes = 1; botao = nBotoesTodos() - 1;
      for (k = 0; k < 30; k++) { quadros(1); SDL_Delay(16); }
      maisDisponiveis(d);
      detmais_abrir(d);
      { SDL_Event e; memset(&e, 0, sizeof e);
        e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; detail_evento(&e); }
      for (k = 0; k < 40; k++) { quadros(1); SDL_Delay(16); }
      quadros(4);
      gravar(serie ? "serie-mais" : "filme-mais");
      detmais_zerar();
    }
  }
  // A ILHA DE AMIGOS em foco e a lista que o OK abre (06/10/2026).
  if (quer(argc, argv, "filme-amigos")) {
    abrir(1, 0, 0, 0); focoAmigos = 1; quadros(30); gravar("filme-amigos");
  }
  if (quer(argc, argv, "filme-amigos-lista")) {
    AmigosTitulo at; int ut, ue;
    abrir(1, 0, 0, 0); focoAmigos = 1;
    if (amigosDoTitulo(&at)) { ultimoEpisodio(&ut, &ue); amtui_abrir(&at, ut, ue, tituloDe(idx)); }
    for (int k = 0; k < 30; k++) { quadros(1); SDL_Delay(16); }
    gravar("filme-amigos-lista");
    amtui_fechar();
  }
  if (quer(argc, argv, "serie-amigos")) {
    abrir(0, 0, 0, 0); focoAmigos = 1; quadros(30); gravar("serie-amigos");
  }
  if (quer(argc, argv, "serie-amigos-lista")) {
    AmigosTitulo at; int ut, ue;
    abrir(0, 0, 0, 0); focoAmigos = 1;
    if (amigosDoTitulo(&at)) { ultimoEpisodio(&ut, &ue); amtui_abrir(&at, ut, ue, tituloDe(idx)); }
    for (int k = 0; k < 30; k++) { quadros(1); SDL_Delay(16); }
    gravar("serie-amigos-lista");
    amtui_fechar();
  }
  if (quer(argc, argv, "filme-trailers")) { abrir(1, 1, SEC_TRAILERS, 0); gravar("filme-trailers"); }
  if (quer(argc, argv, "filme-elenco")) { abrir(1, 1, SEC_ELENCO, 0); gravar("filme-elenco"); }
  if (quer(argc, argv, "filme-notas")) { abrir(1, 1, SEC_NOTAS, 0); gravar("filme-notas"); }
  if (quer(argc, argv, "filme-comentarios")) { abrir(1, 1, SEC_COMENTARIOS, 0); gravar("filme-comentarios"); }
  // Trakt comments row, movie and series (series: pills first, cards from col 2).
  if (quer(argc, argv, "filme-comentarios-foco")) { abrir(1, 1, SEC_COMENTARIOS, 1); gravar("filme-comentarios-foco"); }
  if (quer(argc, argv, "serie-comentarios")) { abrir(0, 1, SEC_COMENTARIOS, 2); gravar("serie-comentarios"); }
  if (quer(argc, argv, "serie-comentarios-pilula")) { abrir(0, 1, SEC_COMENTARIOS, 0); gravar("serie-comentarios-pilula"); }
  for (int series = 0; series < 2; series++) {
    const char *id = series ? "serie-comentarios-sem-foco" : "filme-comentarios-sem-foco";
    if (!quer(argc, argv, id)) continue;
    abrir(series ? 0 : 1, 1, SEC_COMENTARIOS, series ? 2 : 0);
    // Keep the same viewport while removing row focus, without a scroll update.
    nivel = 0; memset(animFoco, 0, sizeof animFoco);
    txt_novo_quadro(); gfx_novo_quadro();
    glClear(GL_COLOR_BUFFER_BIT); detail_desenhar(SDL_GetTicks());
    gravar(id);
  }
  // Cast / recommendations / trailers / collection on the shared glass card,
  // movie and series, focused (col 0) and without row focus.
  { static const struct { const char *nome; int sec; } fl[] = {
      { "elenco", SEC_ELENCO }, { "relacionados", SEC_RELACIONADOS },
      { "trailers", SEC_TRAILERS }, { "colecao", SEC_COLECAO } };
    for (int series = 0; series < 2; series++)
      for (int l = 0; l < 4; l++) {
        char id[64];
        snprintf(id, sizeof id, "%s-%s", series ? "serie" : "filme", fl[l].nome);
        // The movie focused ids already have their own handlers above.
        if (series && argc > 2 && quer(argc, argv, id)) { abrir(0, 1, fl[l].sec, 0); gravar(id); }
        snprintf(id, sizeof id, "%s-%s-sem-foco", series ? "serie" : "filme", fl[l].nome);
        if (argc > 2 && quer(argc, argv, id)) {
          abrir(series ? 0 : 1, 1, fl[l].sec, 0);
          nivel = 0; memset(animFoco, 0, sizeof animFoco);
          txt_novo_quadro(); gfx_novo_quadro();
          glClear(GL_COLOR_BUFFER_BIT); detail_desenhar(SDL_GetTicks());
          gravar(id);
        }
      } }
  if (quer(argc, argv, "filme-relacionados")) { abrir(1, 1, SEC_RELACIONADOS, 0); gravar("filme-relacionados"); }
  if (quer(argc, argv, "filme-colecao")) { abrir(1, 1, SEC_COLECAO, 0); gravar("filme-colecao"); }
  if (quer(argc, argv, "filme-fim")) { abrir(1, 1, SEC_DETALHES, 0); gravar("filme-fim"); }
  if (quer(argc, argv, "serie-topo")) { abrir(0, 0, 0, 0); gravar("serie-topo"); }
  if (quer(argc, argv, "serie-ep")) { abrir(0, 1, SEC_EPISODIOS, 2); gravar("serie-ep"); }
  if (quer(argc, argv, "serie-temp")) { abrir(0, 1, SEC_TEMPORADAS, 1); gravar("serie-temp"); }
  if (quer(argc, argv, "serie-baixo")) { abrir(0, 1, SEC_ELENCO, 0); gravar("serie-baixo"); }
  // O botao "Assistir do comeco" (temInicio + layout agrupado, o da TV)
  // ganhando o foco: TRES QUADROS NO MEIO da animacao. A pilula abre
  // primeiro e o rotulo entra DEPOIS, inteiro — nunca cortado palavra por
  // palavra pela largura em crescimento (anim_expande). Sem o SDL_DELAY o
  // quadro e rapido demais e a animacao assenta antes da captura.
  if (quer(argc, argv, "serie-inicio")) {
    abrir(0, 0, 0, 0);                     // assenta com o foco no botao primario
    botao = 1;                             // foco vai para o "Assistir do comeco"
    SDL_Delay(80);  quadros(1); gravar("serie-inicio-1");   // largura no meio
    SDL_Delay(80);  quadros(1); gravar("serie-inicio-2");   // largura no fim, sem rotulo
    SDL_Delay(60);  quadros(1); gravar("serie-inicio-3");   // rotulo entrando
    quadros(30); gravar("serie-inicio-fim");
  }

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
