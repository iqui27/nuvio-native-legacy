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
#define extras_comentario_spoiler fx_com_spoiler
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
#define extras_livro_autor      fx_livro_autor
#define extras_colecao_do_autor fx_colecao_do_autor
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
#define extras_titulo_original  fx_titulo_original
#define extras_idioma_original  fx_idioma_original
#define extras_estudio_rede     fx_estudio_rede

#if defined(NV_DETAIL21_PROOF) || defined(NV_LEGACY_PROOF)
#define mus_estado fx_mus_estado
#include "musica.h"
MusEstado fx_mus_estado(const char *imdb,int temp,MusAlbum *out) {
  (void)imdb; (void)temp;
  if (out) {
    memset(out,0,sizeof *out);
    snprintf(out->titulo,sizeof out->titulo,"The Dark Knight (Original Motion Picture Soundtrack)");
    snprintf(out->artista,sizeof out->artista,"Hans Zimmer & James Newton Howard");
    snprintf(out->capa,sizeof out->capa,"/Volumes/ExternalSSD/tmp/musica-visual/src/assets/film.jpg");
    static const char *tracks[]={"Why So Serious?","I'm Not a Hero","Harvey Two-Face"};
    out->n=3;
    for (int i=0;i<3;i++) {
      static const int durations[]={554,394,175};
      out->faixa[i].n=i+1; out->faixa[i].dur=durations[i];
      snprintf(out->faixa[i].titulo,sizeof out->faixa[i].titulo,"%s",tracks[i]);
      snprintf(out->faixa[i].artista,sizeof out->faixa[i].artista,"%s",out->artista);
    }
  }
  return MUS_PRONTO;
}
#endif
#include "text.h"
static int traktLayoutCalls;
float trakt_bloco(TxtEstilo, const char *, int, int, int, float, float, float, float, float, int);
int trakt_largura(TxtEstilo, const char *);
size_t trakt_token_tam(const char *);
#define txt_bloco trakt_bloco
#define txt_largura trakt_largura
#define txt_token_tam trakt_token_tam
#include "gfx.h"
#include "botoes.h"
#include "catalogo.h"
#include "reacao.h"
#include "plrui.h"
static int cardMaterial, cardFrost, captionSolid, commentMaterial;
// Record actual draw arguments, so missing hero offsets fail the PNG proof.
static GfxRect serverRects[7];
static int serverRectCount;
static int serverRims;
static GfxRect serverPrimary;
static int serverHints;
static float server_hints(const char *const *keys, const char *const *labels, int n,
                          float x, float y, int right, float a) {
  if (a > .01f) serverHints++;
  return plrui_dicas(keys, labels, n, x, y, right, a);
}
static void server_rating_hint(const CatItem *item, float a);
void server_panel(GfxRect r, float radius, float density, float a) {
  if (getenv("NUVIO_SERVER_CHECK") && a > .01f && !gfx_vidro_sem_aro) serverRims++;
  gfx_vidro_painel(r, radius, density, a);
}
static void server_primary(GfxRect r, const char *label, const char *icon, float focus, int primary, int align, float a) {
  serverPrimary = r;
  botao_pilula(r, label, icon, focus, primary, align, a);
}
static float server_compact(float x, float y, const char *label, const char *icon, float focus, float a) {
  float w = plrui_botao(x, y, label, icon, focus, a);
  serverPrimary = (GfxRect){x, y, w, 60};
  return w;
}
#define botao_pilula server_primary
#define plrui_botao server_compact
static void server_rect(GfxRect r) {
  if (getenv("NUVIO_SHOT_SERVIDORES") && serverRectCount < 7)
    serverRects[serverRectCount++] = r;
}
#include "plrui.h"
#include "vaitocarui.h"
static void server_material(GfxRect r, float radius, int modal, float a) {
  if (r.h == 76.0f) server_rect(r);
  plrui_material(r, radius, modal, a);
}
static void server_vai(const VaiTocar *vt, float x, float y, float w, float a) {
  server_rect((GfxRect){x, y, w, vtui_altura(vt)});
  vtui_desenhar(vt, x, y, w, a);
}
static void server_plano(GfxRect r, float radius, float a) {
  if (r.h == 76.0f || r.h == 132.0f || (r.w == 180 && r.h == 48)) server_rect(r);
  plrui_material_plano(r, radius, a);
}
static void server_button(GfxRect r, float a) {
  if (r.w == 180 && r.h == 48) server_rect(r);
  plrui_botao_repouso(r, a);
}
#define plrui_botao_repouso server_button
#define plrui_material server_material
#define plrui_material_plano server_plano
#define vtui_desenhar server_vai
static void card_material(GfxRect r, float radius, float density, float alpha) {
  int before = gfx_vidro_com_fonte;
  gfx_vidro_material(r, radius, density, alpha);
  cardMaterial++;
  if (r.w == 600.0f && r.h == 340.0f) commentMaterial++;
  cardFrost += gfx_vidro_com_fonte > before;
}
static void card_color(GfxRect r, float radius, float cr, float cg, float cb, float a) {
  if (r.w == 180 && r.h == 48) server_rect(r);
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
// Freeze only Detail's wall clock for reproducible estimated-finish labels.
static time_t detailShotTime(time_t *result) {
  const char *fixed = getenv("NUVIO_SHOT_TIME");
  time_t value = fixed ? (time_t)strtoll(fixed, NULL, 10) : time(NULL);
  if (result) *result = value;
  return value;
}
#define time detailShotTime
#define servidores_contem_item fx_servidor_contem
#define plrui_dicas server_hints
#define reacao_detalhe_dica server_rating_hint
#include NV_DETAIL_SOURCE
#ifdef NV_LEGACY_PROOF
// Compatibility only for unexecuted 2.1-specific capture scenarios.
static Uint32 heroDesde, secVisto[N_SECOES];
static int focoServidor;
static int nNotasTiles(void) { return 1; }
int detail_fonte_servidor(void) { return 0; }
int detail_tem_servidores(void) { return 0; }
int musicaui_visivel(void) { return musicaui_aberta(); }
#endif
#undef time
#undef plrui_dicas
#undef reacao_detalhe_dica
static void server_rating_hint(const CatItem *item, float a) {
  if (a > .01f) serverHints++;
  reacao_detalhe_dica(item, a);
}
#undef plrui_material_plano
#undef gfx_vidro_material
#undef gfx_vidro_painel
#undef gfx_cor
#undef txt_bloco
#undef txt_largura
#undef txt_token_tam
#undef servidores_contem_item
#undef plrui_material
#undef plrui_botao_repouso
#undef vtui_desenhar
int fx_servidor_contem(const CatItem *item, int provider) {
  return getenv("NUVIO_SHOT_SERVIDORES") && item && provider < 2;
}

float trakt_bloco(TxtEstilo e, const char *s, int r, int g, int b,
                   float x, float y, float w, float lead, float a, int max) {
  traktLayoutCalls++;
  float h = txt_bloco(e, s, r, g, b, x, y, w, lead, a, max);
  if (e == TXT_DET_META2 && x >= 0 && s && !strcmp(s, tituloDe(idx)))
    server_rect((GfxRect){x, y, w, h});
  return h;
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
int fx_com_spoiler(int ep, int i) { return i == 1; }
int fx_n_comentarios(void)    { return ehSerieDeEnsaio() && !getenv("NUVIO_SHOT_REFERENCE") ? 0 :
    (getenv("NUVIO_DETAIL_CHECK") && !strcmp(getenv("NUVIO_DETAIL_CHECK"), "comments")) || (getenv("NUVIO_TRAKT_CHECK") && (!strcmp(getenv("NUVIO_TRAKT_CHECK"), "perf") || !strcmp(getenv("NUVIO_TRAKT_CHECK"), "vidro"))) ? 8 : 3; }
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
int         fx_ficha_duracao(void)       { return getenv("NUVIO_SHOT_REFERENCE") ? 156 : 124; }
const char *fx_ficha_paises(void)        { return "United States of America"; }
const char *fx_ficha_classificacao(void) { return "12"; }
const char *fx_titulo_original(void)  {
  return getenv("NUVIO_SHOT_REFERENCE") ? (ehSerieDeEnsaio() ? "Fargo" : "Project Hail Mary") :
    ehSerieDeEnsaio() ? "Fallout" : "The Devil Wears Prada 2";
}
const char *fx_idioma_original(void)  { return "en"; }
int fx_estudio_rede(int i) { return ehSerieDeEnsaio() && i==0; }

static const char *const REL_TIT[] = { "The Prestige", "Frequency", "Outcome", "The Invite",
                                       "Prisoners", "Spaceman", "The Martian" };
static const char *const REL_ANO[] = { "2006", "2000", "2026", "2026", "2013", "2024", "2015" };
static const char *const REL_PO[] = { A "poster/02.jpg", A "poster/05.jpg", A "poster/06.jpg",
  A "poster/08.jpg", A "poster/09.jpg", A "poster/10.jpg", A "poster/12.jpg" };
int fx_n_relacionados(void)   { return 7; }
const char *fx_relacionado_titulo(int i) { return REL_TIT[i]; }
const char *fx_relacionado_ano(int i) { return REL_ANO[i]; }
const char *fx_relacionado_poster(int i) { return REL_PO[i]; }

// Reference movie (Project Hail Mary): the row is the book author's adaptations,
// as the mock (Andy Weir: The Martian, this film, the announced Artemis).
#define REF_FILME (getenv("NUVIO_SHOT_REFERENCE") && !ehSerieDeEnsaio())
const char *fx_livro_autor(void) { return REF_FILME ? "Andy Weir" : ""; }
int fx_colecao_do_autor(void) { return REF_FILME; }
int fx_n_colecao(void)        { return ehSerieDeEnsaio() ? 6 : REF_FILME ? 3 : 2; }
const char *fx_colecao_nome(void) { return ehSerieDeEnsaio() ? "Fargo" : REF_FILME ? "Andy Weir" : "O Diabo Veste Prada"; }
static const char *const COL_TIT_F[] = { "The Devil Wears Prada", "The Devil Wears Prada 2" };
static const char *const COL_ANO_F[] = { "2006", "2026" };
static const char *const COL_TIT_R[] = { "Perdido em Marte", "Project Hail Mary", "Artemis" };
static const char *const COL_ANO_R[] = { "2015", "2026", "" };
static const char *const COL_TIT_S[] = { "Filme", "T1", "T2", "T3", "T4", "T5" };
static const char *const COL_ANO_S[] = { "1996", "2014", "2015", "2017", "2020", "2023" };
const char *fx_colecao_titulo(int i) { return (ehSerieDeEnsaio() ? COL_TIT_S : REF_FILME ? COL_TIT_R : COL_TIT_F)[i]; }
const char *fx_colecao_ano(int i) { return (ehSerieDeEnsaio() ? COL_ANO_S : REF_FILME ? COL_ANO_R : COL_ANO_F)[i]; }
long fx_colecao_tmdb(int i) { return 350 + i; }
const char *fx_colecao_capa(void) { return A "poster/07.jpg"; }
const char *fx_colecao_fundo(void) { return A "07.jpg"; }
const char *fx_colecao_sinopse(void) { return "Dois filmes sobre a Runway."; }
const char *fx_colecao_poster(int i) {
  static char ref[2][400];
  if (REF_FILME && i < 3) {
    if (i == 2) return "";   // Artemis: announced, no art
    snprintf(ref[i], sizeof ref[i], "%s/%s.jpg", getenv("NUVIO_SHOT_REFERENCE"), i ? "phm-arte" : "martian");
    return ref[i];
  }
  if (getenv("NUVIO_PROOF_ASSETS")) {
    snprintf(ref[i%2],sizeof ref[0],"%s/%s.jpg",getenv("NUVIO_PROOF_ASSETS"),i ? "tt33612209" : "tt0458352");
    return ref[i%2];
  }
  return i ? A "poster/07.jpg" : A "poster/02.jpg";
}
const char *fx_colecao_sinopse_parte(int i) { (void)i; return "Sinopse da parte."; }
int fx_colecao_nota(int i) { return 70 + i * 5; }

static const char *const EST_NOME_F[] = { "20th Century Studios", "Wendy Finerman Productions", "Fox 2000" };
static const char *const EST_NOME_S[] = { "FX", "MGM Television", "26 Keys" };
static const char *const EST_LOGO[] = { "deploy/app/art/logo/00.png", "deploy/app/art/logo/07.png", "" };
int fx_n_estudios(void)       { return 3; }
const char *fx_estudio_nome(int i) { return (ehSerieDeEnsaio() ? EST_NOME_S : EST_NOME_F)[i]; }
const char *fx_estudio_logo(int i) { return EST_LOGO[i]; }

static const char *const TR_NOME_F[] = { "Trailer oficial", "Trailer 2", "Bastidores", "Cena · Rocky" };
static const char *const TR_NOME_S[] = { "Recap · T1 em 3 min", "Trailer · T1", "Teaser · T5", "Bastidores" };
static const char *const TR_MINI[] = { A "07.jpg", A "13.jpg", A "30.jpg", A "22.jpg" };
int fx_n_trailers(void) { return 4; }
const char *fx_trailer_yt(int i) { (void)i; return "abc"; }
const char *fx_trailer_nome(int i) { return (ehSerieDeEnsaio() ? TR_NOME_S : TR_NOME_F)[i]; }
const char *fx_trailer_miniatura(int i) {
  const char *ref=getenv("NUVIO_SHOT_REFERENCE");
  static char path[1024];
  // NUVIO_SHOT_TRAILERS=<dir>: per-card crops like the mock (<phm|fargo>-<i>.jpg).
  if (getenv("NUVIO_SHOT_TRAILERS")) { snprintf(path,sizeof path,"%s/%s-%d.jpg",getenv("NUVIO_SHOT_TRAILERS"),ehSerieDeEnsaio() ? "fargo" : "phm",i); return path; }
  if (ref) { snprintf(path,sizeof path,"%s/%s-arte.jpg",ref,ehSerieDeEnsaio() ? "fargo" : "phm"); return path; }
  return TR_MINI[i];
}
int fx_nota_trakt(void) { return 85; }
int fx_nota(int f) {
  switch (f) {
    case EX_IMDB: return 80; case EX_TOMATOES: return 920; case EX_METACRITIC: return 780;
    case EX_TRAKT: return 850; case EX_LETTERBOXD: return 39; default: return 0;
  }
}

int fx_n_temporadas(void) { return ehSerieDeEnsaio() ? (getenv("NUVIO_SHOT_REFERENCE") ? 5 : 2) : 0; }
int fx_temporada_numero(int t) { return t + 1; }
int fx_n_eps(int t) { (void)t; return ehSerieDeEnsaio() ? (getenv("NUVIO_SHOT_REFERENCE") ? 10 : 8) : 0; }
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
  if (getenv("NUVIO_SHOT_REFERENCE")) {
    snprintf(buf,sizeof buf,"# nuvio seriefrases v1\n%lld\tProject Hail Mary\t1\nQ\tRocky\tAmaze! Amaze! Amaze!\n",(long long)time(NULL));
    assert(dados_gravar_leve("frases-" IMDB_FILME ".txt",buf));
    snprintf(buf,sizeof buf,"# nuvio seriefrases v1\n%lld\tFargo\t1\nQ\tLorne Malvo · T1:E1\tOs mapas diziam: aqui há dragões. Hoje não dizem mais. Isso não quer dizer que os dragões não estejam lá.\n",(long long)time(NULL));
    assert(dados_gravar_leve("frases-" IMDB_SERIE ".txt",buf));
  } else assert(dados_gravar_leve("frases-" IMDB_FILME ".txt", buf));
}

// --- CATALOGO ----------------------------------------------------------------
static CatItem itens[2];
static CatEp episodios[50];
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

  if (getenv("NUVIO_SHOT_REFERENCE")) {
    const char *ref=getenv("NUVIO_SHOT_REFERENCE");
    snprintf(itens[1].titulo,sizeof itens[1].titulo,"Project Hail Mary");
    snprintf(itens[1].meta,sizeof itens[1].meta,"2026 · 156 min");
    snprintf(itens[1].genero,sizeof itens[1].genero,"Filme · Ficção científica");
    snprintf(itens[1].sinopse,sizeof itens[1].sinopse,"Ryland Grace acorda sozinho numa nave a anos-luz da Terra, sem lembrar quem é nem por que está ali. Aos poucos a memória volta — e com ela a missão de salvar o Sol.");
    snprintf(itens[1].direcao,sizeof itens[1].direcao,"Phil Lord, Christopher Miller");
    snprintf(itens[0].titulo,sizeof itens[0].titulo,"Fargo");
    snprintf(itens[0].direcao,sizeof itens[0].direcao,"Noah Hawley");
    itens[0].nTemporadas=5;
    for (int t=0;t<5;t++) itens[0].temporadas[t]=t+1;
    snprintf(itens[0].meta,sizeof itens[0].meta,"2014 · 54 min");
    snprintf(itens[0].genero,sizeof itens[0].genero,"Série · Crime · Drama");
    snprintf(itens[0].sinopse,sizeof itens[0].sinopse,"Um vendedor de seguros frustrado cruza o caminho de um assassino de aluguel. A partir daí, uma cidadezinha de Minnesota nunca mais será a mesma.");
    for (int k=0;k<2;k++) {
      snprintf(itens[k].backdrop,sizeof itens[k].backdrop,"%s/%s-arte.jpg",ref,k ? "phm" : "fargo");
      snprintf(itens[k].logo,sizeof itens[k].logo,"%s/%s-logo.png",ref,k ? "phm" : "fargo");
      static const char *const names[2][6]={
        {"Billy Bob Thornton","Martin Freeman","Allison Tolman","Colin Hanks","Bob Odenkirk","Keith Carradine"},
        {"Ryan Gosling","Sandra Hüller","James Ortiz","Lionel Boyce","Ken Leung","Milana Vayntrub"}};
      static const char *const roles[2][6]={
        {"Lorne Malvo","Lester Nygaard","Molly Solverson","Gus Grimly","Bill Oswalt","Lou Solverson"},
        {"Ryland Grace","Eva Stratt","Rocky (voz)","Carl","Yáo","Olesya"}};
      for (int j=0;j<6;j++) {
        snprintf(itens[k].elenco[j].foto,sizeof itens[k].elenco[j].foto,"%s/elenco/%c%d.jpg",ref,k ? 'a' : 'b',j);
        snprintf(itens[k].elenco[j].nome,sizeof itens[k].elenco[j].nome,"%s",names[k][j]);
        snprintf(itens[k].elenco[j].papel,sizeof itens[k].elenco[j].papel,"%s",roles[k][j]);
      }
      itens[k].elenco[5].foto[0]=0;
    }
  }
  fil.ini = 0; fil.n = 2;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "series");
  cat_definir_tudo(itens, 2, &fil, 1);

  int reference=getenv("NUVIO_SHOT_REFERENCE")!=NULL;
  for (int t = 1; t <= (reference ? 5 : 2); t++)
    for (i = 0; i < (reference ? 10 : 8); i++) {
      CatEp *e = &episodios[n++];
      e->temporada = t; e->episodio = i + 1;
      e->nota=78+(i*3+t)%16;
      snprintf(e->nome, sizeof e->nome, "%s", EP_NOME[i%8]);
      snprintf(e->duracao, sizeof e->duracao, "56 min");
      snprintf(e->data, sizeof e->data, "11 de abril de 2024");
      snprintf(e->sinopse, sizeof e->sinopse, "%s",
               i == 2 ? "Lucy, Maximus e o Ghoul disputam a mesma recompensa no deserto."
                      : "Sinopse do episódio.");
      snprintf(e->thumb, sizeof e->thumb, A "ep/00_1_%02d.jpg", (t == 1 ? i : 7 - i) + 1);
      const char *ref=getenv("NUVIO_SHOT_REFERENCE");
      if (ref) {
        snprintf(e->thumb,sizeof e->thumb,"%s/ep%d.jpg",ref,1+i%4);
        snprintf(e->nome,sizeof e->nome,"Episódio %d",i+1);
      }
    }
  cat_definir_episodios(0, episodios, n);
  for (i = 1; i <= (reference ? 10 : 8); i++) vistoep_definir(IMDB_SERIE, 1, i, i <= 2);
  if (reference) {
    agenda_definir_hoje("2026-10-22");
    agenda_registrar(IMDB_SERIE,"Fargo",NULL,"Returning Series",6,1,"Premiere","2027-01-01","");
  }
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
  if (!getenv("NUVIO_DETAIL21_PROOF") && !getenv("NUVIO_LEGACY_PROOF")) SDL_SaveBMP(s, nome);
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

static float shotScroll=-1;
static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents();
    tex_bombear(3);
    vt_antes();
    detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    if (shotScroll>=0) scrollY=shotScroll;
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    serverRectCount = 0;
    serverRims = 0;
    serverHints = 0;
    detail_desenhar(SDL_GetTicks());
    // NUVIO_SHOT_RELOGIO=1: the clock island over the page, as app.c draws it.
    if (getenv("NUVIO_SHOT_RELOGIO")) {
      ilha_relogio_visivel(1); ilha_posicionar(getenv("NUVIO_SHOT_SERVIDORES") ? 2 : 1); ilha_desenhar(SDL_GetTicks());
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
  if (getenv("NUVIO_SHOT_REFERENCE")) {
    cat_historico_definir_id(itens[i].imdb,itens[i].tipo,i ? 0 : 1);
    char path[1024]; snprintf(path,sizeof path,"%s/ajustes.txt",dados_dir());
    FILE *f=fopen(path,"a"); assert(f);
    fprintf(f,"selected_theme %d\n",i ? 5 : 1); fclose(f);
    ajustes_dir(dados_dir());
  }
  if (getenv("NUVIO_SHOT_SOURCE")) {
    Stream source={0};
    snprintf(source.rotulo,sizeof source.rotulo,"Fixture 4K Dolby Vision Atmos");
    snprintf(source.url,sizeof source.url,"https://example.invalid/fixture.mp4");
    snprintf(source.provedor,sizeof source.provedor,"Offline fixture");
    source.altura=2160; source.dolbyVision=source.dolbyAtmos=source.mp4=1;
    source.badges=badges_detectar(source.rotulo);
    source.tamanhoMB=8000;
    char alvo[96]; int st=0,ep=0;
    if (ehSerie()) episodioAlvo(&st,&ep,NULL);
    if (st && ep) snprintf(alvo,sizeof alvo,"%s:%d:%d",itens[i].imdb,st,ep);
    else snprintf(alvo,sizeof alvo,"%s",itens[i].imdb);
    stream_definir_alvo(alvo); stream_definir_lista(&source,1);
  }
  quadros(2);
  nivel = nv;
  if (nv == 0) botao = col;
  else { foco.fileira = sec; foco.coluna = col; }
  quadros(160);
  // Os quadros daqui sao muito mais rapidos que os da TV: o esvanecer do logo
  // (260 ms, SDL_GetTicks) ainda nao terminou. Espera e assenta.
  SDL_Delay(400);
  heroDesde = SDL_GetTicks() - 10000;   // stills show the settled hero enter wave
  // Springs and crossfades step by real ms: sub-ms frames give dt 0, so pace
  // the last frames like the TV (16 ms) until focus fills settle.
  for (int k = 0; k < 16; k++) { SDL_Delay(16); quadros(1); }
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
  tex_iniciar(getenv("NUVIO_SHOT_REFERENCE") ? 30 : 16);
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
    fprintf(f, "idioma %d\n", lg && *lg ? atoi(lg) : 0);
    // Acento azul do mockup (#5aa2ff/#42a5f5): o tema Oceano.
    fputs("selected_theme 2\n", f);
    if (getenv("NUVIO_DETAIL21_PROOF") || getenv("NUVIO_LEGACY_PROOF"))
      fputs("notaTituloTrakt 0\nnotaTituloMeta 0\nnotaTituloLetter 0\n",f);
    if (getenv("NUVIO_SHOT_REFERENCE")) fputs("blurUnwatchedEpisodes 0\n",f);
    if (getenv("NUVIO_SHOT_TOPO")) fprintf(f, "detalheTopoLocal %d\n", atoi(getenv("NUVIO_SHOT_TOPO")));
    if (getenv("NUVIO_SHOT_SPOILERS")) fputs("blurUnwatchedEpisodes 0\n",f);
    if (getenv("NUVIO_SHOT_LAYOUT")) fprintf(f, "homeLayoutLocal %d\n", atoi(getenv("NUVIO_SHOT_LAYOUT")));
    if (getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO")))
      fputs("vidroLocal 0\n", f);
    else fputs("vidroLocal 1\n", f);
    // Ajustes > Aparencia > Fundo: 0 Arte, 1 Arte borrada, 2 Frost.
    // NUVIO_SHOT_FONTE=3: Montserrat, the interface font of the owner's TV.
    if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
    // NUVIO_SHOT_ESCALA=<0..5>: text/component scale (0 = 85%, 2 = 100%).
    if (getenv("NUVIO_SHOT_ESCALA")) fprintf(f, "escalaTextoVersaoLocal 1\nescalaTextoLocal %d\n", atoi(getenv("NUVIO_SHOT_ESCALA")));
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

  if (quer(argc,argv,"video")) {
    static const struct { const char *name; int movie,row,col; } shots[]={
      {"hero",1,-1,0},{"notas",1,SEC_NOTAS,0},{"trailers",1,SEC_TRAILERS,1},
      {"elenco",1,SEC_ELENCO,1},{"autor",1,SEC_COLECAO,1},
      {"similares",1,SEC_RELACIONADOS,0},{"comentarios",1,SEC_COMENTARIOS,0},
      {"ficha",1,SEC_DETALHES,0},{"serie-hero",0,-1,0},
      {"episodios",0,SEC_EPISODIOS,2},{"progresso",0,SEC_PROGTEMP,2},
      {"universo",0,SEC_COLECAO,1}
    };
    for (int j=0;j<(int)(sizeof shots/sizeof *shots);j++) {
      tex_encerrar(); assert(tex_iniciar(30));
      abrir(shots[j].movie,shots[j].row>=0,shots[j].row<0 ? 0 : shots[j].row,shots[j].col);
      frasesAberta=1; seriefrases_abrir(itens[shots[j].movie].imdb);
      quadros(50); gravar(shots[j].name);
    }
  }

  // anim: enter, focus moves (ratings row to Friends, cast) and exit, as frames.
  if (argc > 2 && quer(argc,argv,"anim")) {
    SDL_Event e;
    char name[100];
    memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN;
    tex_encerrar(); assert(tex_iniciar(30));
    abrir(1,0,0,0); quadros(1);
    // Art first, so the enter frames show the page and not empty slots.
    { Uint32 dl = SDL_GetTicks() + 15000; int pend = 1;
      while (pend && (Sint32)(dl - SDL_GetTicks()) > 0) {
        tex_estatisticas(NULL,&pend,NULL,NULL,NULL); SDL_Delay(16); tex_bombear(3); } }
    detail_fechar_seco();
    for (int k=0;k<60 && detail_aberto();k++) quadros(1);
    printf("anim enter from closed=%d\n", !detail_aberto());
    { HomeItem hi; memset(&hi, 0, sizeof hi);   // raw open: abrir() settles 160 frames
      hi.indice = 1; hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
      hi.titulo = itens[1].titulo; hi.genero = itens[1].genero; hi.meta = itens[1].meta;
      detail_abrir(&hi); }
    // 5 frames (83 ms) of page time per capture: the hero wave clock is pinned
    // to the same simulated time, since each capture takes longer than that.
    for (int k=0;k<12;k++) {
      int ms = k * 5 * 1000 / 60;
      quadros(k ? 4 : 0); heroDesde = SDL_GetTicks() - (Uint32)ms; quadros(1);
      printf("anim t=%.3f a2=%.3f ms=%d\n", t, fase2(), ms);
      snprintf(name,sizeof name,"anim-enter-%d",k); gravar(name); }
    quadros(40); SDL_Delay(400); quadros(8);
    // Hero focus Play -> +: fill, ink and scale ease (draw dt is capped at 50 ms).
    e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
    for (int k=0;k<4;k++) { quadros(1); snprintf(name,sizeof name,"anim-botao-%d",k); gravar(name); }
    e.key.keysym.sym = SDLK_LEFT; detail_evento(&e); quadros(30);
    abrir(1,1,SEC_NOTAS,0); quadros(60); SDL_Delay(400); quadros(8);
    gravar("anim-notas-0");
    for (int c=1;c<nNotasTiles();c++) { e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e); quadros(3); }
    for (int k=0;k<4;k++) { quadros(k ? 5 : 1); snprintf(name,sizeof name,"anim-notas-fim-%d",k); gravar(name); }
    quadros(40); gravar("anim-notas-amigos");
    // Section reveal (forgotten once, as when it scrolls in) and the row dim
    // easing from the ratings row down to trailers.
    // Each saved frame takes longer than the 300 ms reveal, so the start of
    // every section is pinned to 90 ms more of reveal per frame.
    memset(secVisto, 0, sizeof secVisto);
    quadros(1);
    { Uint32 t0 = SDL_GetTicks(), atraso[N_SECOES];
      for (int r=0;r<N_SECOES;r++) atraso[r] = secVisto[r] ? secVisto[r] - t0 : 0;
      for (int k=0;k<5;k++) {
        Uint32 ag = SDL_GetTicks();
        for (int r=0;r<N_SECOES;r++) if (atraso[r]) secVisto[r] = (ag + atraso[r] - k*90) | 1u;
        quadros(1); snprintf(name,sizeof name,"anim-revela-%d",k); gravar(name);
      } }
    quadros(40); SDL_Delay(600); quadros(8);
    e.key.keysym.sym = SDLK_DOWN; detail_evento(&e);
    for (int k=0;k<4;k++) { quadros(k ? 2 : 1); snprintf(name,sizeof name,"anim-dim-%d",k); gravar(name); }
    // Footer card focused: its text scales with the card.
    abrir(1,1,SEC_DETALHES,0); quadros(60); SDL_Delay(400); quadros(8); gravar("anim-rodape");
    abrir(1,1,SEC_ELENCO,0); quadros(60); SDL_Delay(400); quadros(8);
    printf("anim focus row=%d col=%d\n", foco.fileira, foco.coluna);
    e.key.keysym.sym = SDLK_RIGHT; detail_evento(&e);
    for (int k=0;k<4;k++) { quadros(k ? 4 : 1); snprintf(name,sizeof name,"anim-elenco-%d",k); gravar(name); }
    detail_fechar();
    for (int k=0;k<5;k++) { quadros(k ? 4 : 1); snprintf(name,sizeof name,"anim-exit-%d",k); gravar(name); }
  }

  if (quer(argc,argv,"full")) {
    assert(ajustes_idioma()==IDIOMA_PT);
    for (int movie=1;movie>=0;movie--) {
      tex_encerrar(); assert(tex_iniciar(30));
      abrir(movie,0,0,0);
      frasesAberta=1; seriefrases_abrir(itens[movie].imdb);
      quadros(20); recalcularLayout();
      int height=(int)ceilf(docFim);
      char name[100],path[1024];
      snprintf(path,sizeof path,"%s/%s-height.txt",saida,movie ? "movie" : "series");
      FILE *f=fopen(path,"w"); assert(f); fprintf(f,"%d\n",height); fclose(f);
      snprintf(path,sizeof path,"%s/%s-sections.txt",saida,movie ? "movie" : "series");
      f=fopen(path,"w"); assert(f);
      for (int r=0;r<N_SECOES;r++) if (secaoN(r)>0)
        fprintf(f,"%d %.0f %.0f\n",r,topoSec[r],conteudoSec[r]);
      fclose(f);
      for (int sy=0;sy<height;sy+=900) {
        shotScroll=sy; quadros(40); SDL_Delay(450); quadros(12);
        // The asynchronous cache can still be decoding under host build load.
        Uint32 deadline = SDL_GetTicks() + 15000;
        for (;;) {
          int pending = 0;
          tex_estatisticas(NULL, &pending, NULL, NULL, NULL);
          if (!pending) break;
          assert((Sint32)(deadline - SDL_GetTicks()) > 0);
          SDL_Delay(16); quadros(4);
        }
        SDL_Delay(400); quadros(4); // Settle artwork fades after the last upload.
        snprintf(name,sizeof name,"%s-full-%04d",movie ? "movie" : "series",sy);
        gravar(name);
        int live=0,pending=0,hot=0; long bytes=0,hotBytes=0;
        tex_estatisticas(&live,&pending,&bytes,&hot,&hotBytes);
        printf("textures %s live=%d hot=%d pending=%d bytes=%ld\n",name,live,hot,pending,bytes);
        assert(live<=30);
      }
      shotScroll=-1;
    }
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
  if (quer(argc,argv,"legacy-proof")) {
    const float scales[]={.85f,1};
    for (int scale=0;scale<2;scale++) for (int glass=0;glass<2;glass++) {
      if (getenv("NUVIO_PROOF_SCALE") && atoi(getenv("NUVIO_PROOF_SCALE"))!=(scale ? 100 : 85)) continue;
      if (getenv("NUVIO_PROOF_GLASS") && atoi(getenv("NUVIO_PROOF_GLASS"))!=glass) continue;
      gfx_escala_txt_definir(scales[scale]); ajustes_definir_vidro(glass);
      char name[128];
      const int rows[]={-1,SEC_NOTAS,SEC_COLECAO,SEC_MUSICAS,SEC_COMENTARIOS};
      const char *names[]={"hero","ratings","collection","music","reviews"};
      for (int section=0;section<5;section++) {
        if (getenv("NUVIO_PROOF_SECTION") && strcmp(getenv("NUVIO_PROOF_SECTION"),names[section])) continue;
        montarCatalogo();
        if (section==3) {
          snprintf(itens[1].titulo,sizeof itens[1].titulo,"The Dark Knight");
          snprintf(itens[1].meta,sizeof itens[1].meta,"2008 · 152 min");
          cat_definir_tudo(itens,2,NULL,0);
        }
        abrir(1,section>0,section>0 ? rows[section] : 0,0);
        for (int tick=0;tick<60;tick++) { SDL_Delay(16); quadros(1); }
        snprintf(name,sizeof name,"%d-%s-%s",scale ? 100 : 85,glass ? "glass" : "solid",names[section]); gravar(name);
#ifdef NV_DETAIL21_PROOF
        if (section==1) {
          SDL_Event key={0}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_RIGHT;
          for (int c=1;c<nNotasTiles();c++) { detail_evento(&key); assert(foco.coluna==c); }
          for(int tick=0;tick<30;tick++) { SDL_Delay(16); quadros(1); }
          snprintf(name,sizeof name,"%d-%s-ratings-last",scale ? 100 : 85,glass ? "glass" : "solid"); gravar(name);
          key.key.keysym.sym=SDLK_ESCAPE; detail_evento(&key); assert(nivel==0 && !saindo);
        }
        if (section==2) {
          SDL_Event key={0}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_RETURN; detail_evento(&key);
          key.type=SDL_KEYUP; detail_evento(&key); assert(colListaAberta);
          key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_ESCAPE; detail_evento(&key);
          assert(!colListaAberta && foco.fileira==SEC_COLECAO && nivel==1);
        }
        if (section==4) {
          SDL_Event key={0}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_RIGHT;
          for (int c=1;c<secaoN(SEC_COMENTARIOS);c++) { detail_evento(&key); assert(foco.coluna==c); }
          key.key.keysym.sym=SDLK_RETURN; detail_evento(&key); key.type=SDL_KEYUP; detail_evento(&key);
          assert(comInteiro>=0); key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_ESCAPE; detail_evento(&key);
          assert(comInteiro<0 && nivel==1 && foco.fileira==SEC_COMENTARIOS);
        }
#endif
        if (section==3) {
          snprintf(itens[1].titulo,sizeof itens[1].titulo,"The Dark Knight");
          snprintf(itens[1].meta,sizeof itens[1].meta,"2008 · 152 min");
          cat_definir_tudo(itens,2,NULL,0);
          SDL_Event key={0}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_RETURN; detail_evento(&key);
          key.type=SDL_KEYUP; detail_evento(&key); assert(musicaui_aberta());
          for (int frame=0;frame<4;frame++) {
            for (int tick=0;tick<(frame==3 ? 28 : 5);tick++) { SDL_Delay(16); quadros(1); }
            snprintf(name,sizeof name,"%d-%s-music-open-%d",scale ? 100 : 85,glass ? "glass" : "solid",frame); gravar(name);
          }
          key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_ESCAPE; detail_evento(&key);
#ifdef NV_DETAIL21_PROOF
          assert(musicaui_visivel() && !musicaui_aberta());
          assert(detail_relogio_oculto() && !detail_relogio_navegavel());
          key.key.keysym.sym=SDLK_DOWN; detail_evento(&key);
          assert(nivel==1 && foco.fileira==SEC_MUSICAS);
#endif
          for (int tick=0;tick<50;tick++) {
            SDL_Delay(16); quadros(1);
            if (tick==4 || tick==10 || tick==49) {
              snprintf(name,sizeof name,"%d-%s-music-close-%d",scale ? 100 : 85,glass ? "glass" : "solid",tick); gravar(name);
            }
          }
          assert(!musicaui_aberta());
#ifdef NV_DETAIL21_PROOF
          assert(!musicaui_visivel() && nivel==1 && foco.fileira==SEC_MUSICAS);
#endif
          montarCatalogo();
        }
      }
      montarCatalogo();
      itens[1].progresso=40; itens[1].restanteMin=68;
      cat_definir_tudo(itens,2,NULL,0);
      abrir(1,0,0,0);
#ifdef NV_DETAIL21_PROOF
      focoVersao=1;
#endif
      quadros(30);
      snprintf(name,sizeof name,"%d-%s-hot-source",scale ? 100 : 85,glass ? "glass" : "solid"); gravar(name);
#ifdef NV_DETAIL21_PROOF
      assert(topoFonteDisponivel());
      SDL_Event key={0}; key.type=SDL_KEYDOWN; key.key.keysym.sym=SDLK_DOWN; detail_evento(&key);
      assert(!pedFontes && !focoVersao && nivel==1);
      key.key.keysym.sym=SDLK_UP; detail_evento(&key);
      assert(nivel==0 && focoVersao && !pedFontes);
      nivel=0; focoVersao=1;
      key.key.keysym.sym=SDLK_RETURN; detail_evento(&key); key.type=SDL_KEYUP; detail_evento(&key);
      assert(detail_pediu_fontes());
#endif
    }
    puts("PASS: legacy components, music open/Back, hot-source navigation");
    tex_encerrar();
    SDL_Quit(); return 0;
  }
  if (quer(argc, argv, "filme-topo")) { abrir(1, 0, 0, 0); gravar("filme-topo"); }
  if (quer(argc, argv, "detalhe-chip")) {
    assert(ajustes_rapido_cor_definir(11));
    assert(dados_gravar("perfis.txt", "1\t0\t1\t0\t#7655D6\tMarina\t\t\n"));
    perfis_carregar_ativo();
    // Confirmed foreign logo, friends feed, and a real candidate stream.
    snprintf(itens[1].logoIdiomaUrl, sizeof itens[1].logoIdiomaUrl, "%s", itens[1].logo);
    snprintf(itens[1].logoIdioma, sizeof itens[1].logoIdioma, "ja");
    cat_definir_tudo(itens, 2, NULL, 0);
    static Stream candidate;
    snprintf(candidate.url, sizeof candidate.url, "https://example.invalid/movie.mkv");
    candidate.altura = 2160;
    stream_definir_alvo(IMDB_FILME); stream_definir_lista(&candidate, 1);
    abrir(1,0,0,0);
    if (getenv("NUVIO_SERVER_CHECK")) {
      int failures = 0;
      const float scales[] = {1, .85f};
      for (int scale = 0; scale < 2; scale++) for (int glass = 0; glass < 2; glass++) {
        gfx_escala_txt_definir(scales[scale]); ajustes_definir_vidro(glass);
        quadros(160);
        float red, green, blue; ajustes_acento(&red, &green, &blue);
        unsigned char pixel[4];
        glReadPixels((int)((serverPrimary.x + serverPrimary.w * .5f) * gfx_escala_txt()),
                     1080 - (int)((serverPrimary.y + 8) * gfx_escala_txt()), 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        int compact = serverPrimary.w <= 180 && serverPrimary.h <= 64;
        int accent = fabsf(pixel[0] / 255.f - red) < .08f &&
                     fabsf(pixel[1] / 255.f - green) < .08f && fabsf(pixel[2] / 255.f - blue) < .08f;
        printf("%s: P1 primary %d%% %s width=%.1f height=%.1f accent=%d\n",
               compact && accent ? "PASS" : "FAIL", scale ? 85 : 100, glass ? "glass" : "solid",
               serverPrimary.w, serverPrimary.h, accent);
        failures += !compact || !accent;
        printf("%s: P1 rimless controls %d%% %s outlines=%d\n", serverRims ? "FAIL" : "PASS",
               scale ? 85 : 100, glass ? "glass" : "solid", serverRims);
        failures += serverRims != 0;
        assert(serverRectCount == 4 && serverHints == 0);
        GfxRect friends = serverRects[0], next = serverRects[1];
        assert(fabsf(friends.y + hSinVis + NV_DETW2_GAP_SIN + NV_DETW2_M1_H -
                      1048 / gfx_escala_txt()) < 1);
        for (int p = 2; p < 4; p++) {
          GfxRect source = serverRects[p];
          assert(next.x + next.w < source.x);
          assert(fabsf(next.y + next.h * .5f - source.y - source.h * .5f) < 1);
          assert(source.y + source.h < friends.y);
        }
        assert(friends.x >= serverRects[3].x);
        printf("PASS: bottom anchor, source row, recommendation, no ratings or hints\n");
      }
      return failures ? 1 : 0;
    }
    const float scales[] = {1, .85f};
    for (int scale = 0; scale < 2; scale++) for (int glass = 0; glass < 2; glass++) {
      char name[80]; gfx_escala_txt_definir(scales[scale]); ajustes_definir_vidro(glass);
      for (int frame = 0; frame < 40; frame++) { SDL_Delay(16); quadros(1); }
      snprintf(name, sizeof name, "server-%d-%s", (int)(scales[scale] * 100 + .5f), glass ? "glass" : "solid"); gravar(name);
    }
    gfx_escala_txt_definir(1); ajustes_definir_vidro(1);
    for (int frame = 0; frame < 40; frame++) { SDL_Delay(16); quadros(1); }
    gravar("detalhe-chip");
    SDL_Event e = {0}; e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN; detail_evento(&e);
    const int times[] = {0, 80, 160, 320};
    quadros(1);
    for (int i = 0, elapsed = 0; i < 4; i++) {
      while (elapsed < times[i]) { SDL_Delay(16); quadros(1); elapsed += 16; }
      char name[80]; snprintf(name, sizeof name, "server-focus-%03d", times[i]); gravar(name);
    }
    e.key.keysym.sym = SDLK_UP; detail_evento(&e); quadros(90);
    HomeItem hi = {0}; hi.indice = 1; hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
    hi.titulo = itens[1].titulo; hi.genero = itens[1].genero; hi.meta = itens[1].meta;
    detail_fechar_seco(); quadros(1);
    detail_abrir(&hi);
    quadros(1);
    assert(fase2() == 0 && textogate_passo(&gateHero, 0, SDL_GetTicks()) < .1f);
    for (int i = 0, elapsed = 0; i < 4; i++) {
      while (elapsed < times[i]) { SDL_Delay(16); quadros(1); elapsed += 16; }
      char name[80]; snprintf(name, sizeof name, "server-entry-%03d", times[i]); gravar(name);
    }
    quadros(160);
    int nav = nBotoes() == 3 && acaoEm(2) == ACAO_MAIS;
#define STEP(code, expected) do { e.key.keysym.sym = code; detail_evento(&e); nav &= !!(expected); } while (0)
    STEP(SDLK_DOWN, focoServidor == 0 && !focoAmigos);
    STEP(SDLK_RIGHT, focoServidor == 1 && !focoAmigos);
    STEP(SDLK_UP, focoServidor == -1 && !focoAmigos);
    STEP(SDLK_DOWN, focoServidor == 0 && !focoAmigos);
    STEP(SDLK_DOWN, focoServidor == -1 && focoAmigos);
    STEP(SDLK_UP, focoServidor == 0 && !focoAmigos);
    STEP(SDLK_UP, focoServidor == -1 && !focoAmigos);
    STEP(SDLK_DOWN, focoServidor == 0 && !focoAmigos);
    STEP(SDLK_DOWN, focoServidor == -1 && focoAmigos);
    STEP(SDLK_DOWN, nivel == 1 && !focoAmigos && focoServidor == -1);
#undef STEP
    char path[700]; snprintf(path, sizeof path, "%s/detalhe-chip-rects.json", saida);
    FILE *proof = fopen(path, "w"); assert(proof);
    fprintf(proof, "{\"navigation_ok\":%d,\"rects\":[", nav);
    for (int i = 0; i < serverRectCount; i++) {
      GfxRect r = serverRects[i];
      fprintf(proof, "%s[%.2f,%.2f,%.2f,%.2f]", i ? "," : "", r.x, r.y, r.w, r.h);
    }
    fputs("]}\n", proof); fclose(proof);
    nivel = 0; focoAmigos = 0; focoServidor = 0;
    e.key.keysym.sym = SDLK_RETURN; detail_evento(&e);
    e.type = SDL_KEYUP; detail_evento(&e);
    assert(detail_pediu_fontes() && detail_fonte_servidor() == 0);
  }
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
  if (quer(argc, argv, "serie-progresso")) { abrir(0, 1, SEC_PROGTEMP, 2); gravar("serie-progresso"); }
  if (quer(argc, argv, "serie-fim")) { abrir(0, 1, SEC_DETALHES, 0); gravar("serie-fim"); }
  if (quer(argc, argv, "serie-comentarios")) { abrir(0, 1, SEC_COMENTARIOS, 2); gravar("serie-comentarios"); }
  if (quer(argc, argv, "serie-ep")) {
    abrir(0, 1, SEC_EPISODIOS, getenv("NUVIO_SHOT_REFERENCE") ? 3 : 2);
    gravar("serie-ep");
  }
  if (quer(argc, argv, "serie-temp")) { abrir(0, 1, SEC_TEMPORADAS, 0); gravar("serie-temp"); }
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
