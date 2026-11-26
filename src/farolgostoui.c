// FAROLGOSTOUI — ver farolgostoui.h.
// Sem armazenamento por thread; zero log com titulo, perfil ou gosto.
#include "farolgostoui.h"
#include "ajustes.h"
#include "anim.h"
#include "botoes.h"
#include "farol.h"
#include "farolgosto.h"
#include "farolsinal.h"
#include "faroltexto.h"
#include "gfx.h"
#include "idioma.h"
#include "ilha.h"
#include "layout.h"
#include "perfis.h"
#include "perfilinfantil.h"
#include "perfisessao.h"
#include "plrui.h"
#include "ponteiro.h"
#include "tela21.h"
#include "text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Ganchos fracos prontos para farolconsent (#53).
enum {
  FAROL_CONSENT_NAO_PERGUNTADO_VAL = 0,
  FAROL_CONSENT_SO_TV_VAL = 1,
  FAROL_CONSENT_SIM_VAL = 2
};
extern int  farolconsent_llm_ativo(void) __attribute__((weak));
extern int  farolconsent_estado(void) __attribute__((weak));
extern int  farolconsent_definir(int estado) __attribute__((weak));
extern void farolconsentui_abrir(void) __attribute__((weak));

enum { FGUI_FECHADO = 0, FGUI_APRENDEU = 1, FGUI_ZERAR = 2 };

static int modoAtual = FGUI_FECHADO;
static int querAberto;
static int foco;          // APRENDEU: 0=Voltar; ZERAR: 0=Zerar, 1=Cancelar
static int scrollIdx;     // APRENDEU: deslocamento vertical leve para listas
static float entradaProg;
static FarolGostoResumo resumoCache;
static int llmConsentLocal;
static int llmPerfil = -1;

static int backKey(const SDL_Event *e) {
  SDL_Keycode k = e->key.keysym.sym;
  return k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_AC_BACK ||
         e->key.keysym.scancode == NV_SCANCODE_BACK;
}

static int okKey(SDL_Keycode k) {
  return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE || k == SDLK_SELECT;
}

void farolgostoui_resumo(FarolGostoResumo *dst) {
  if (!dst) return;
  memset(dst, 0, sizeof *dst);
  dst->nGeneros = farol_gosto_top(FG_DIM_GENERO, dst->generos, FGUI_TOP_N);
  dst->nPessoas = farol_gosto_top(FG_DIM_PESSOA, dst->pessoas, FGUI_TOP_N);
  dst->nTemas   = farol_gosto_top(FG_DIM_TEMA, dst->temas, FGUI_TOP_N);
  dst->vazio = (dst->nGeneros == 0 && dst->nPessoas == 0 && dst->nTemas == 0);
}

void farolgostoui_abrir_aprendeu(void) {
  farolgostoui_resumo(&resumoCache);
  modoAtual = FGUI_APRENDEU;
  querAberto = 1;
  foco = 0;
  scrollIdx = 0;
}

void farolgostoui_abrir_zerar(void) {
  if (perfil_ajustes_travados(perfis_ativo())) return;
  modoAtual = FGUI_ZERAR;
  querAberto = 1;
  foco = 1; // Inicia no botao seguro ("Cancelar"), nunca no destrutivo.
  scrollIdx = 0;
}

void farolgostoui_fechar(void) {
  querAberto = 0;
  modoAtual = FGUI_FECHADO;
  entradaProg = 0.0f;
}

void farolgostoui_abrir_sessao(void) {
  farolgostoui_fechar();
  memset(&resumoCache, 0, sizeof resumoCache);
  llmConsentLocal = 0;
  llmPerfil = -1;
}

int farolgostoui_aberto(void) {
  return querAberto || (modoAtual != FGUI_FECHADO && entradaProg > 0.002f);
}

int farolgostoui_modo(void) { return querAberto ? modoAtual : FGUI_FECHADO; }
int farolgostoui_foco(void) { return foco; }

int farolgostoui_llm_ativo(void) {
  int p = perfis_ativo();
  if (llmPerfil != p) {
    llmConsentLocal = 0;
    llmPerfil = p;
  }
  if (perfil_infantil_ativo()) return 0;
  if (farolconsent_llm_ativo) return farolconsent_llm_ativo() ? 1 : 0;
  if (farolconsent_estado)
    return (farolconsent_estado() == FAROL_CONSENT_SIM_VAL) ? 1 : 0;
  return llmConsentLocal;
}

int farolgostoui_llm_definir(int permitirLlm) {
  int p = perfis_ativo();
  int v = permitirLlm ? 1 : 0;
  if (llmPerfil != p) {
    llmConsentLocal = 0;
    llmPerfil = p;
  }
  if (perfil_infantil_ativo() || perfil_ajustes_travados(p)) return 0;
  if (farolconsent_definir) {
    int ok = farolconsent_definir(v ? FAROL_CONSENT_SIM_VAL : FAROL_CONSENT_SO_TV_VAL);
    llmConsentLocal = ok ? v : farolgostoui_llm_ativo();
    if (ok && v && farolconsentui_abrir) farolconsentui_abrir();
    return ok;
  }
  llmConsentLocal = v;
  if (v && farolconsentui_abrir) farolconsentui_abrir();
  return 1;
}

static void confirmarZerar(void) {
  if (perfil_ajustes_travados(perfis_ativo())) {
    querAberto = 0;
    return;
  }
  farol_gosto_zerar(NULL);
  ilha_avisar("farol-gosto", ILHA_OK, "check", i18n(FT_GOSTO_ZERADO), 4000u, 0);
  querAberto = 0;
}

static void onClickBotao(int id, int b) {
  (void)b;
  if (!querAberto) return;
  if (modoAtual == FGUI_APRENDEU) {
    querAberto = 0;
  } else if (modoAtual == FGUI_ZERAR) {
    if (id == 0) confirmarZerar();
    else querAberto = 0;
  }
}

static void onHoverBotao(int id, int b) {
  (void)b;
  if (querAberto) foco = id;
}

void farolgostoui_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!e || !querAberto) return;
  if (e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEMOTION ||
      e->type == SDL_MOUSEBUTTONUP || e->type == SDL_MOUSEWHEEL)
    return;
  if (e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (backKey(e)) {
    querAberto = 0;
    return;
  }
  if (okKey(k)) {
    if (e->key.repeat) return;
    if (modoAtual == FGUI_APRENDEU) {
      querAberto = 0;
    } else if (modoAtual == FGUI_ZERAR) {
      if (foco == 0) confirmarZerar();
      else querAberto = 0;
    }
    return;
  }
  if (modoAtual == FGUI_APRENDEU) {
    if (k == SDLK_UP && scrollIdx > 0) scrollIdx--;
    else if (k == SDLK_DOWN && scrollIdx < 2) scrollIdx++;
  } else if (modoAtual == FGUI_ZERAR) {
    if (k == SDLK_LEFT || k == SDLK_UP) foco = 0;
    else if (k == SDLK_RIGHT || k == SDLK_DOWN) foco = 1;
  }
}

void farolgostoui_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!querAberto && entradaProg <= 0.002f) {
    modoAtual = FGUI_FECHADO;
    entradaProg = 0.0f;
    return;
  }
  entradaProg = t21_entrada(entradaProg, dt, querAberto);
  if (!querAberto && entradaProg <= 0.002f) {
    modoAtual = FGUI_FECHADO;
    entradaProg = 0.0f;
  }
}

static float desenharGrupoTop(const char *titulo, const char (*itens)[64],
                              int n, float x, float y, float w, float a) {
  float ar, ag, ab, cy = y;
  int i;
  TxtLinha th = t21_texto_linha(T21_GROUP, i18n(titulo),
                                T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
  t21_texto_desenhar(th, x, cy, .85f * a);
  cy += th.h + 10.0f;
  ajustes_acento(&ar, &ag, &ab);
  if (n <= 0) {
    TxtLinha vz = t21_texto_linha(T21_SUPPORT, i18n("Nenhum ainda"),
                                  T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w);
    t21_texto_desenhar(vz, x, cy, T21_TEXT_SUPPORT * a);
    return (cy + vz.h + 18.0f) - y;
  }
  {
    float cx = x, linH = 40.0f;
    for (i = 0; i < n; i++) {
      TxtLinha li = t21_texto_linha(T21_CHIP, itens[i],
                                    T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, w - 32.0f);
      float pw = li.w + 28.0f;
      if (cx > x && cx + pw > x + w) {
        cx = x;
        cy += linH + 8.0f;
      }
      gfx_cor((GfxRect){ cx, cy, pw, linH }, T21_R_PILL, ar, ag, ab, .16f * a);
      t21_texto_desenhar(li, cx + 14.0f, cy + (linH - li.h) * 0.5f, .95f * a);
      cx += pw + 10.0f;
    }
    cy += linH + 22.0f;
  }
  return cy - y;
}

void farolgostoui_desenhar(Uint32 agora) {
  float a, s, sw = NV_LAYOUT_REAL_W, sh = NV_LAYOUT_REAL_H;
  float sheetW, sheetH, sx, sy, px, py, pw;
  GfxRect r;
  (void)agora;
  if (modoAtual == FGUI_FECHADO || (!querAberto && entradaProg <= 0.002f)) return;
  a = anim_suave(entradaProg);
  if (a <= 0.002f) return;
  s = anim_mistura(T21_SHEET_SCALE, 1.0f, a);

  gfx_cor((GfxRect){ 0, 0, sw, sh }, 0.0f, 0.0f, 0.0f, 0.0f, T21_VEIL_BLUR * a);

  sheetW = modoAtual == FGUI_ZERAR ? 760.0f : 960.0f;
  sheetH = modoAtual == FGUI_ZERAR ? 340.0f : 680.0f;
  if (sheetW > sw - 2.0f * T21_SHEET_MARGIN) sheetW = sw - 2.0f * T21_SHEET_MARGIN;
  if (sheetH > sh - 2.0f * T21_SHEET_MARGIN) sheetH = sh - 2.0f * T21_SHEET_MARGIN;
  sx = (sw - sheetW) * 0.5f;
  sy = (sh - sheetH) * 0.5f;
  r = (GfxRect){ sx, sy, sheetW, sheetH };

  {
    float cx = sx + sheetW * 0.5f, cy = sy + sheetH * 0.5f;
    r.w *= s; r.h *= s;
    r.x = cx - r.w * 0.5f;
    r.y = cy - r.h * 0.5f;
  }
  t21_ilha_folha(r, T21_R_SHEET, a);

  px = r.x + T21_SHEET_PAD;
  py = r.y + T21_SHEET_PAD;
  pw = r.w - 2.0f * T21_SHEET_PAD;

  gfx_icone((GfxRect){ px, py + 4.0f, 28.0f, 28.0f }, "aj_farol", 1, 1, 1, .9f * a);
  {
    TxtLinha kicker = t21_texto_linha(T21_KICKER, i18n(FT_NOME),
                                      T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw - 40.0f);
    t21_texto_desenhar(kicker, px + 38.0f, py + 6.0f, .75f * a);
  }
  py += 42.0f;

  if (modoAtual == FGUI_APRENDEU) {
    TxtLinha tit = t21_texto_linha(T21_SHEET_TITLE, i18n(FT_GOSTO_APRENDEU),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    t21_texto_desenhar(tit, px, py, a);
    py += tit.h + 22.0f;

    if (resumoCache.vazio) {
      TxtLinha vz = t21_texto_linha(T21_SHEET_BODY,
        i18n("O Farol ainda não aprendeu preferências neste perfil."),
        T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
      t21_texto_desenhar(vz, px, py + 12.0f, T21_TEXT_SECONDARY * a);
    } else {
      if (scrollIdx <= 0)
        py += desenharGrupoTop("Gêneros", resumoCache.generos, resumoCache.nGeneros,
                               px, py, pw, a);
      if (scrollIdx <= 1 && py < r.y + r.h - 140.0f)
        py += desenharGrupoTop("Pessoas", resumoCache.pessoas, resumoCache.nPessoas,
                               px, py, pw, a);
      if (scrollIdx <= 2 && py < r.y + r.h - 120.0f)
        desenharGrupoTop("Temas", resumoCache.temas, resumoCache.nTemas,
                         px, py, pw, a);
    }
    {
      const char *rot = i18n("Voltar");
      TxtLinha bl = t21_texto_linha(T21_CHIP, rot, 255, 255, 255, 255, 220.0f);
      float bw = fmaxf(140.0f, bl.w + 48.0f);
      GfxRect br = { px, r.y + r.h - T21_SHEET_PAD - T21_CTRL_H, bw, T21_CTRL_H };
      int ink = ajustes_tinta_foco();
      plrui_pilula_foco(br, a);
      bl = t21_texto_linha(T21_CHIP, rot, ink, ink, ink, 255, bw);
      t21_texto_desenhar(bl, br.x + (br.w - bl.w) * 0.5f,
                         br.y + (br.h - bl.h) * 0.5f, a);
      if (querAberto && a > 0.5f)
        ponteiro_alvo(br.x, br.y, br.w, br.h, onClickBotao, onHoverBotao, 0, 0);
    }
  } else if (modoAtual == FGUI_ZERAR) {
    TxtLinha tit = t21_texto_linha(T21_SHEET_TITLE, i18n(FT_GOSTO_ZERAR),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    TxtLinha msg = t21_texto_linha(T21_SHEET_BODY, i18n(FT_GOSTO_ZERAR_CONF),
                                   T21_TEXT_R, T21_TEXT_G, T21_TEXT_B, 255, pw);
    const char *labels[2] = { i18n(FT_GOSTO_ZERAR), i18n("Cancelar") };
    float bx = px, by = r.y + r.h - T21_SHEET_PAD - T21_CTRL_H;
    int i;
    t21_texto_desenhar(tit, px, py, a);
    py += tit.h + 18.0f;
    t21_texto_desenhar(msg, px, py, .85f * a);

    for (i = 0; i < 2; i++) {
      TxtLinha bl = t21_texto_linha(T21_CHIP, labels[i], 255, 255, 255, 255, 280.0f);
      float bw = fmaxf(150.0f, bl.w + 48.0f);
      GfxRect br = { bx, by, bw, T21_CTRL_H };
      if (foco == i) {
        int ink = ajustes_tinta_foco();
        plrui_pilula_foco(br, a);
        bl = t21_texto_linha(T21_CHIP, labels[i], ink, ink, ink, 255, bw);
      } else {
        gfx_cor(br, T21_R_PILL, 1, 1, 1, T21_CHIP_REST * a);
      }
      t21_texto_desenhar(bl, br.x + (br.w - bl.w) * 0.5f,
                         br.y + (br.h - bl.h) * 0.5f, a);
      if (querAberto && a > 0.5f)
        ponteiro_alvo(br.x, br.y, br.w, br.h, onClickBotao, onHoverBotao, i, 0);
      bx += bw + T21_GAP_CARD;
    }
  }
}
