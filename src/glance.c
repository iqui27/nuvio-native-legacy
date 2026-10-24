#define NV_TXT_TELA
#include "glance.h"
#include "tela21.h"
#include "botoes.h"
#include "revela.h"
#include "anim.h"
#include "ajustes.h"
#include "ctxmenu.h"
#include "ctxlista.h"
#include "descoberta.h"
#include "extras.h"
#include "gfx.h"
#include "idioma.h"
#include "layout.h"
#include "player.h"
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
static float focoAnim[4], entrada;
static Uint32 entradaEm;
static unsigned revisao;
static int acaoPendente, acaoResolucao;
static int acaoEpisodios = -1;
static CatItem acaoTitulo, resolucaoTitulo;
static Uint32 acaoDesde;
static GfxRect videoRect(void) {
  TXT_INI();
  float x, w; t21_inicio(&x, &w);
  GfxRect r = {x + w - 700, ANC(180), 700, 394};
  TXT_FIM();
  return r;
}

void glance_fechar(void) {
  acaoPendente = 0; acaoEpisodios = -1;
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
  acaoPendente = 0; acaoEpisodios = -1;
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
  entrada = 0; entradaEm = SDL_GetTicks();
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
      CatItem ci = atual;
      if (k >= 0 && (!cat_copiar_item(k, &ci) || strcmp(ci.tipo, atual.tipo))) {
        k = -1; ci = atual;
      }
      if (k >= 0 && botao == GLANCE_BT_VISTO && !strcmp(ci.tipo, "series") && ci.nTemporadas == 0) {
        acaoPendente = botao; acaoTitulo = atual; acaoDesde = SDL_GetTicks();
        acaoEpisodios = k; desc_episodios(k, 0);
        return;
      }
      if ((k < 0 && !strncmp(atual.imdb, "tmdb:", 5)) ||
          (botao == GLANCE_BT_VISTO && !strcmp(atual.tipo, "series") && atual.nTemporadas == 0)) {
        if (!desc_titulo_buscando()) {
          acaoPendente = botao; acaoTitulo = atual; acaoDesde = SDL_GetTicks();
          acaoResolucao = 1; resolucaoTitulo = atual;
          desc_pedir_titulo_semente(!strncmp(atual.imdb, "tmdb:", 5) ? "" : atual.imdb,
                                    atual.tmdb, !strcmp(atual.tipo, "series") ? "tv" : atual.tipo,
                                    atual.titulo, atual.meta, atual.poster);
        }
      } else ctx_acao_rapida(&ci, botao == GLANCE_BT_VISTO);
    }
  }
}

int glance_tomar_acao(int idx) {
  CatItem ci;
  int b;
  if (!acaoResolucao) return 0;
  if (!cat_copiar_item(idx, &ci) || strcmp(ci.tipo, resolucaoTitulo.tipo) ||
      (strcmp(ci.imdb, resolucaoTitulo.imdb) && !(ci.tmdb > 0 && ci.tmdb == resolucaoTitulo.tmdb))) return 0;
  acaoResolucao = 0;
  // An episode-only action can outlive an older canceled title request.
  if (acaoEpisodios >= 0) return 1;
  // Cancellation ends the action, but its late metadata still belongs here.
  if (!acaoPendente || !aberta || SDL_GetTicks() - acaoDesde >= 20000 ||
      strcmp(atual.tipo, acaoTitulo.tipo) || strcmp(atual.imdb, acaoTitulo.imdb)) {
    acaoPendente = 0;
    return 1;
  }
  if (acaoPendente == GLANCE_BT_VISTO && !strcmp(ci.tipo, "series") && ci.nTemporadas == 0) {
    acaoEpisodios = idx; desc_episodios(idx, 0);
    return 1;
  }
  b = acaoPendente; acaoPendente = 0; acaoEpisodios = -1;
  ctx_acao_rapida(&ci, b == GLANCE_BT_VISTO);
  return 1;
}

void glance_atualizar(float dt, Uint32 agora) {
  TrailerCandidatos c = {0};
  const char *url = NULL;
  char youtube[32] = "";
  int qual = 0, venceu;
  entrada = anim_mola(entrada, aberta ? 1 : 0, dt, NV_MOLA_TELA);
  if (!aberta) return;
  if (acaoPendente && acaoEpisodios >= 0) {
    CatItem ci;
    if (agora - acaoDesde < 20000 && cat_copiar_item(acaoEpisodios, &ci) &&
        !strcmp(ci.tipo, acaoTitulo.tipo) &&
        (!strcmp(ci.imdb, acaoTitulo.imdb) || (ci.tmdb > 0 && ci.tmdb == acaoTitulo.tmdb)) && ci.nTemporadas > 0) {
      acaoPendente = 0; acaoEpisodios = -1;
      ctx_acao_rapida(&ci, 1);
    } else if (agora - acaoDesde >= 20000 || !desc_episodios_carregando(acaoEpisodios)) {
      acaoPendente = 0; acaoEpisodios = -1;
    }
  } else if (acaoPendente && (agora - acaoDesde >= 20000 || !desc_titulo_buscando()))
    acaoPendente = 0;
  // A catalog replacement invalidates the owner's positions. Do not let a
  // late publication make an arrow or Details act on another title.
  if (cat_revisao() != revisao) { glance_fechar(); return; }
  for (int b = 0; b < 4; b++) focoAnim[b] = ajustes_animacoes_reduzidas()
    ? b == botao : anim_mola(focoAnim[b], b == botao, dt, NV_MOLA_FOCO);
  if (player_mini_ativo() || player_retido()) return;
  if (!trailer_suportado() || trailerFim || !atual.imdb[0]) return;
  if (ctx_aberto()) return;
  if (!trailerPedido) {
    trailerapple_pedir(atual.imdb, atual.titulo, atual.meta, !strcmp(atual.tipo, "series"));
    if (!trailerfonte_tizen() || trailerfonte_imdb_tizen()) trailerimdb_pedir(atual.imdb);
    if (trailerfonte_tizen()) extras_hero_trailer_pedir(atual.imdb, !strcmp(atual.tipo, "series"), atual.tmdb);
    trailerPedido = 1; desde = agora;
  }
  if (trailerQual > 0) {
    if (trailer_tocando()) { prazo = 0; return; }
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
      ;
      GfxRect video = videoRect();
      float scale = gfx_escala_txt();
      video.x *= scale; video.y *= scale; video.w *= scale; video.h *= scale;
      trailer_abrir(url, video, 0, 0);
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
  plrui_cartao(r, T21_R_CARD, 1);
  if (!tex) gfx_icone((GfxRect){r.x + (r.w - 32) / 2, r.y + (r.h - 32) / 2, 32, 32}, "aj_film", 1, 1, 1, .6f);
  if (tex) {
    gfx_tex_aspect_atual = tex_aspecto(arte);
    gfx_rect(r, tex, GFX_CARD, 0, 0, 0, T21_R_CARD / r.h, 0, 0, 0, 1);
    gfx_tex_aspect_atual = 0;
  }
}
static void glance_desenhar_(Uint32 agora) {
  GfxRect tela = {0, 0, NV_TELA_W, NV_TELA_H}, vr = videoRect();
  char linha[240];
  const char *rot[4], *ic[4] = { "aj_play", "aj_info", "aj_bookmark", "aj_eye" };
  int idx, visto, salvo;
  CatItem live;
  (void)agora;
  if (!aberta && entrada < .005f) return;
  float x, w; t21_inicio(&x, &w);
  float leftW = fminf(900, vr.x - x - 64);
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
  texto(TXT_DET_META, linha, x, ANC(100), leftW, 185);
  float h = txt_bloco_corta(TXT_TITULO2, atual.titulo, 245, 245, 245, x, ANC(150), leftW, 64, 1, 2);
  float y = ANC(158) + h;
  snprintf(linha, sizeof linha, "%s%s", atual.meta, atual.nota ? " · IMDb " : "");
  if (atual.nota) { size_t n = strlen(linha); snprintf(linha + n, sizeof linha - n, "%.1f", atual.nota / 10.0f); }
  texto(TXT_DET_META, linha, x, y, leftW, 210);
  txt_bloco_corta(TXT_DET_SIN, atual.sinopse, 225, 225, 225, x, y + 50, leftW, 36, 1, 3);
  if (visto) texto(TXT_DET_META, i18n("Visto"), x, ANC(498), leftW, 220);
  else if (live.progresso > 0) { snprintf(linha, sizeof linha, "%d%%", live.progresso);
    texto(TXT_DET_META, linha, x, ANC(498), leftW, 220); }
  rot[0] = i18n(live.progresso > 0 && live.progresso < 90 ? "Continuar" : "Assistir");
  rot[1] = i18n("Detalhes"); rot[2] = i18n(salvo ? "Salvo" : "Salvar");
  rot[3] = i18n(visto ? "Visto" : "Marcar como assistido");
  for (int b = 0; b < 4; b++) {
    float entry = revela_entra(entradaEm, revela_onda_atraso(0, b), agora);
    GfxRect r = {x, ANC(544) + b * 88 + (1 - entry) * NV_ENTRA_DY,
                 botao_largura(rot[b], ic[b], b == 0), b == 0 ? BOTAO_H_PRIMARIO : BOTAO_H_SECUNDARIO};
    botao_pilula(r, rot[b], ic[b], focoAnim[b], b == 0, 1, entry);
  }
  if (live.restanteMin > 0 && live.progresso > 0 && live.progresso < 90) {
    snprintf(linha, sizeof linha, i18n("%d min restantes"), live.restanteMin);
    texto(TXT_DET_META2, linha, x + 680, ANC(565), 255, 205);
  }
  plrui_cartao(vr, T21_R_CARD, 1);
  if (trailer_aberto() && trailer_tocando() && trailer_mostra_video()) gfx_furo_raio(vr, 0.035f);
  else {
    const char *arte = atual.backdrop[0] ? atual.backdrop : atual.poster;
    GLuint tex = tex_obter_larg(arte, vr.w);
    if (tex) { gfx_tex_aspect_atual = tex_aspecto(arte);
      gfx_rect(vr, tex, GFX_CARD, 0, 0, 0, 0.035f, 0, 0, 0, 1); gfx_tex_aspect_atual = 0; }
  }
  texto(TXT_DET_META, i18n("Trailer"), vr.x, vr.y - 46, vr.w, 210);
  cartaz(&atual, (GfxRect){ vr.x + 248, ANC(610), 166, 249 });
  for (int d = -1; d <= 1; d += 2) {
    CatItem viz;
    int i = indice + d;
    if (i < 0 || i >= fonte.total() || !fonte.item(i, &viz)) continue;
    float nx = vr.x + (d < 0 ? 0 : 500);
    cartaz(&viz, (GfxRect){ nx + 48, ANC(650), 106, 159 });
    gfx_icone((GfxRect){nx + (d < 0 ? 0 : 168), ANC(710), 28, 28},
               d < 0 ? "pl_chevron-left" : "aj_chevron-right", 1, 1, 1, .9f);
    texto(TXT_ILHA_ITEM, viz.titulo, nx, ANC(830), 200, 195);
  }
  t21_dica("Voltar", i18n("Voltar"), x, ANC(T21_SAFE_B), 1);
}

void glance_desenhar(Uint32 agora) {
  TXT_INI();
  float opacity = gfx_opacidade_grupo;
  gfx_opacidade_grupo *= entrada;
  gfx_transformar(0, 0, 1, 0, (1 - entrada) * NV_ENTRA_DY);
  glance_desenhar_(agora);
  gfx_sem_transformar();
  gfx_opacidade_grupo = opacity;
  TXT_FIM();
}
