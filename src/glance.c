#include "glance.h"
#include "anim.h"
#include "ajustes.h"
#include "ctxmenu.h"
#include "ctxlista.h"
#include "descoberta.h"
#include "extras.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "salvos.h"
#include "teclavoltar.h"
#include "tex_cache.h"
#include "text.h"
#include "trailer.h"
#include "trailerapple.h"
#include "trailerimdb.h"
#include "trailerfonte.h"
#include <stdio.h>
#include <string.h>

static GlanceFonte fonte;
static CatItem atual, tocar;
static int aberta, indice, botao, trailerPedido, trailerFim, trailerQual;
static Uint32 desde, prazo;
static float focoAnim[4];
static unsigned revisao;
static int acaoPendente;
static CatItem acaoTitulo;
static Uint32 acaoDesde;
static GfxRect videoRect(void) { return (GfxRect){ 1110, 180, 700, 394 }; }

void glance_fechar(void) {
  if (!aberta) return;
  aberta = 0;
  trailer_fechar();
}
int glance_aberto(void) { return aberta; }
int glance_indice(void) { return indice; }
int glance_botao(void) { return botao; }

static int selecionar(int i) {
  CatItem it;
  if (!fonte.total || !fonte.item || i < 0 || i >= fonte.total() || !fonte.item(i, &it)) return 0;
  trailer_fechar();
  atual = it; indice = i; botao = GLANCE_BT_PRIMARIO;
  trailerPedido = trailerFim = trailerQual = 0;
  desde = SDL_GetTicks(); prazo = 0;
  memset(focoAnim, 0, sizeof focoAnim);
  if (fonte.foco) fonte.foco(i);
  return 1;
}
void glance_abrir(const GlanceFonte *f, int i) {
  if (!f) return;
  glance_fechar();
  fonte = *f;
  tocar.imdb[0] = 0;
  aberta = selecionar(i);
  revisao = cat_revisao();
}

int glance_pediu_tocar(const CatItem *ci) {
  if (!ci || !tocar.imdb[0]) return 0;
  if (strcmp(tocar.tipo, ci->tipo)) return 0;
  if (strcmp(tocar.imdb, ci->imdb) && !(tocar.tmdb > 0 && tocar.tmdb == ci->tmdb)) return 0;
  tocar.imdb[0] = 0;
  return 1;
}

void glance_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberta || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (nv_tecla_voltar(e)) { glance_fechar(); return; }
  if (k == SDLK_LEFT || k == SDLK_RIGHT) {
    selecionar(indice + (k == SDLK_LEFT ? -1 : 1));
  } else if (k == SDLK_UP && botao > 0) botao--;
  else if (k == SDLK_DOWN && botao < GLANCE_BT_VISTO) botao++;
  else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) && !e->key.repeat) {
    if (botao == GLANCE_BT_PRIMARIO || botao == GLANCE_BT_DETALHES) {
      int i = indice;
      void (*abrir)(int) = fonte.detalhes;
      if (botao == GLANCE_BT_PRIMARIO) tocar = atual;
      glance_fechar();
      if (abrir) abrir(i);
    } else {
      int k = ctxlista_indice(atual.imdb, atual.tmdb);
      if ((k < 0 && !strncmp(atual.imdb, "tmdb:", 5)) ||
          (botao == GLANCE_BT_VISTO && !strcmp(atual.tipo, "series") && atual.nTemporadas == 0)) {
        if (!desc_titulo_buscando()) {
          acaoPendente = botao; acaoTitulo = atual; acaoDesde = SDL_GetTicks();
          desc_pedir_titulo_semente(!strncmp(atual.imdb, "tmdb:", 5) ? "" : atual.imdb,
                                    atual.tmdb, atual.tipo, atual.titulo, atual.meta, atual.poster);
        }
      } else ctx_acao_rapida(&atual, botao == GLANCE_BT_VISTO);
    }
  }
}

int glance_tomar_acao(int idx) {
  CatItem ci;
  int b;
  if (!acaoPendente) return 0;
  if (!cat_copiar_item(idx, &ci) ||
      (strcmp(ci.imdb, acaoTitulo.imdb) && !(ci.tmdb > 0 && ci.tmdb == acaoTitulo.tmdb))) return 0;
  b = acaoPendente; acaoPendente = 0;
  if (aberta && SDL_GetTicks() - acaoDesde < 20000 && !strcmp(atual.imdb, acaoTitulo.imdb))
    ctx_acao_rapida(&ci, b == GLANCE_BT_VISTO);
  return 1;
}

void glance_atualizar(float dt, Uint32 agora) {
  TrailerCandidatos c = {0};
  const char *url = NULL;
  char youtube[32] = "";
  int qual = 0, venceu;
  if (!aberta) return;
  // A catalog replacement invalidates the owner's positions. Do not let a
  // late publication make an arrow or Details act on another title.
  if (cat_revisao() != revisao) { glance_fechar(); return; }
  for (int b = 0; b < 4; b++) focoAnim[b] = ajustes_animacoes_reduzidas()
    ? b == botao : anim_mola(focoAnim[b], b == botao, dt, NV_MOLA_FOCO);
  if (!trailer_suportado() || trailerFim || !atual.imdb[0]) return;
  if (ctx_aberto()) return;
  if (!trailerPedido) {
    trailerapple_pedir(atual.imdb, atual.titulo, atual.meta, !strcmp(atual.tipo, "series"));
    if (!trailerfonte_tizen() || trailerfonte_imdb_tizen()) trailerimdb_pedir(atual.imdb);
    if (trailerfonte_tizen()) extras_hero_trailer_pedir(atual.imdb, !strcmp(atual.tipo, "series"), atual.tmdb);
    trailerPedido = 1; desde = agora;
  }
  if (trailerQual > 0) {
    if (trailer_tocando()) prazo = 0;
    else if (trailer_falhou() || (prazo && (Sint32)(agora - prazo) >= 0)) {
      trailer_fechar();
      trailerQual = trailerfonte_depois(trailerfonte_ajuste(), trailerfonte_tizen(), trailerQual);
      if (!trailerQual) { trailerFim = 1; return; }
      // Skip failed sources in the shared chooser without changing settings.
      trailerQual = -trailerQual;
    } else if (!trailer_aberto()) { trailerFim = 1; return; }
    else return;
  }
  venceu = agora - desde >= NV_TRAILER_PREPARA_MS;
  c.apple = trailerapple_url(atual.imdb);
  c.appleRespondeu = trailerapple_respondeu(atual.imdb) || venceu;
  c.imdb = trailerimdb_url(atual.imdb, NULL);
  c.imdbRespondeu = trailerimdb_respondeu(atual.imdb) || venceu;
  if (trailerfonte_tizen() && extras_hero_trailer_obter(atual.imdb, youtube, sizeof youtube)) c.youtube = youtube;
  c.youtubeRespondeu = venceu;
  if (trailerQual < 0) {
    int ordem[3], n = trailerfonte_ordem(trailerfonte_ajuste(), trailerfonte_tizen(), ordem), achou = 0;
    for (int i = 0; i < n; i++) {
      if (ordem[i] == -trailerQual) achou = 1;
      if (!achou) {
        if (ordem[i] == TRF_APPLE) { c.apple = NULL; c.appleFalhou = 1; }
        if (ordem[i] == TRF_IMDB) { c.imdb = NULL; c.imdbRespondeu = 1; }
        if (ordem[i] == TRF_YOUTUBE) { c.youtube = NULL; c.youtubeRespondeu = 1; }
      }
    }
  }
  switch (trailerfonte_escolher(trailerfonte_ajuste(), trailerfonte_tizen(), &c, &url, &qual)) {
    case TRF_ABRE:
      trailer_abrir(url, videoRect(), 0, 0);
      trailerQual = qual; prazo = agora + NV_TRAILER_PREPARA_MS;
      break;
    case TRF_NENHUMA: trailerFim = 1; break;
    default: break;
  }
}

static void texto(TxtEstilo estilo, const char *s, float x, float y, float w, int cor) {
  txt_desenhar(txt_linha_corta(estilo, s, cor, cor, cor, 255, w), x, y);
}
static void cartaz(const CatItem *it, GfxRect r) {
  const char *arte = it->poster[0] ? it->poster : it->backdrop;
  GLuint tex = tex_obter_larg(arte, r.w);
  gfx_vidro_painel(r, 0.04f, 0.78f, 1);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(r, tex, GFX_CARD, 0, 0, 0, 0.04f, 0, 0, 0, 1);
    gfx_tex_aspect_atual = 0;
  }
}
void glance_desenhar(Uint32 agora) {
  GfxRect tela = {0, 0, NV_TELA_W, NV_TELA_H}, vr = videoRect();
  char linha[240];
  const char *rot[4], *ic[4] = { "aj_play", "aj_info", "aj_bookmark", "aj_eye" };
  int idx, visto, salvo;
  CatItem live;
  (void)agora;
  if (!aberta) return;
  // Read action state by identity: saving may rebuild the underlying list.
  idx = cat_indice_por_imdb(atual.imdb);
  live = atual;
  if (idx >= 0) cat_copiar_item(idx, &live);
  visto = cat_visto(&live) || live.progresso >= 90;
  salvo = live.naLista || salvos_tem(atual.imdb);
  gfx_cor(tela, 0, 0.025f, 0.025f, 0.03f, 1);
  if (atual.backdrop[0]) {
    GLuint tex = tex_obter_hero(atual.backdrop);
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(atual.backdrop);
      gfx_rect(tela, tex, GFX_DETALHE, 0, 0, 0, 0, 0, 0, 0, 0.65f); gfx_tex_aspect_atual = 0; }
  }
  snprintf(linha, sizeof linha, "%s · %d / %d", i18n(!strcmp(atual.tipo, "series") ? "Série" : "Filme"), indice + 1, fonte.total());
  texto(TXT_DET_META, linha, 104, 100, 900, 185);
  float h = txt_bloco_corta(TXT_TITULO2, atual.titulo, 245, 245, 245, 104, 150, 900, 64, 1, 2);
  float y = 158 + h;
  snprintf(linha, sizeof linha, "%s%s", atual.meta, atual.nota ? " · IMDb " : "");
  if (atual.nota) { size_t n = strlen(linha); snprintf(linha + n, sizeof linha - n, "%.1f", atual.nota / 10.0f); }
  texto(TXT_DET_META, linha, 104, y, 900, 210);
  txt_bloco_corta(TXT_DET_SIN, atual.sinopse, 225, 225, 225, 104, y + 50, 900, 36, 1, 3);
  if (visto) texto(TXT_DET_META, i18n("Visto"), 104, 498, 900, 220);
  else if (live.progresso > 0) { snprintf(linha, sizeof linha, "%d%%", live.progresso);
    texto(TXT_DET_META, linha, 104, 498, 900, 220); }
  rot[0] = i18n(live.progresso > 0 && live.progresso < 90 ? "Continuar" : "Assistir");
  rot[1] = i18n("Detalhes"); rot[2] = i18n(salvo ? "Salvo" : "Salvar");
  rot[3] = i18n(visto ? "Visto" : "Marcar como assistido");
  for (int b = 0; b < 4; b++) {
    GfxRect r = {104, 544 + b * 94.0f, 650, 78};
    gfx_vidro_painel(r, 0.2f, 0.78f, 1);
    gfx_vidro_foco(r, 0.2f, focoAnim[b], 1);
    ctx_menu_linha(r, rot[b], ic[b], focoAnim[b], 1, -1);
  }
  if (live.restanteMin > 0 && live.progresso > 0 && live.progresso < 90) {
    snprintf(linha, sizeof linha, i18n("%d min restantes"), live.restanteMin);
    texto(TXT_DET_META2, linha, 788, 565, 255, 205);
  }
  gfx_vidro_painel(vr, 0.035f, 0.78f, 1);
  if (trailer_aberto() && trailer_tocando() && trailer_mostra_video()) gfx_furo_raio(vr, 0.035f);
  else {
    const char *arte = atual.backdrop[0] ? atual.backdrop : atual.poster;
    GLuint tex = tex_obter_larg(arte, vr.w);
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(vr, tex, GFX_CARD, 0, 0, 0, 0.035f, 0, 0, 0, 1); gfx_tex_aspect_atual = 0; }
  }
  texto(TXT_DET_META, i18n("Trailer"), vr.x, vr.y - 46, vr.w, 210);
  cartaz(&atual, (GfxRect){ 1358, 610, 166, 249 });
  for (int d = -1; d <= 1; d += 2) {
    CatItem viz;
    int i = indice + d;
    if (i < 0 || i >= fonte.total() || !fonte.item(i, &viz)) continue;
    float x = d < 0 ? 1110 : 1610;
    cartaz(&viz, (GfxRect){ x + 48, 650, 106, 159 });
    texto(TXT_DET_META, d < 0 ? "‹" : "›", x + (d < 0 ? 0 : 168), 710, 40, 235);
    texto(TXT_ILHA_ITEM, viz.titulo, x, 830, 210, 195);
  }
  texto(TXT_DET_META2, i18n("Voltar"), 104, 980, 900, 185);
}
