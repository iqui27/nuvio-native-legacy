// Shared 2.1 vocabulary. DIRETRIZ.md A.1-A.9; existing layout stays authoritative.
#ifndef NV_TELA21_H
#define NV_TELA21_H
#include "layout.h"
#include "escala.h"
#include "anim.h"
#include "focoprof.h"
#include "text.h"
#include "plrui.h"

// Real-pixel anchors. Components and text scale; these edges do not.
#define T21_SAFE_L ajustes_conteudo_x()
#define T21_SAFE_R (NV_LAYOUT_REAL_W - NV_LEGACY_CONTENT_RIGHT)
#define T21_SAFE_T NV_MARGEM_Y
#define T21_SAFE_B (NV_LAYOUT_REAL_H - NV_MARGEM_Y)
#define T21_SHEET_MARGIN NV_FOLHA_MARGEM
#define T21_SHEET_PAD NV_FOLHA_PAD
#define T21_GAP_CARD NV_CARD_GAP
#define T21_GAP_BIG NV_CARD_GAP_GRANDE
#define T21_GAP_ROW NV_FILEIRA_GAP
#define T21_GAP_LIST NV_LINHA_VAO
#define T21_CTRL_H NV_CTRL_H
#define T21_CTRL_GAP NV_CTRL_VAO
#define T21_R_SHEET NV_FOLHA_RAIO
#define T21_R_MODAL 32.0f
#define T21_R_ROW 20.0f
#define T21_R_CARD 20.0f
#define T21_R_PILL 0.5f

#define T21_TITLE TXT_NOV_TITULO
#define T21_SHEET_TITLE TXT_ILHA_TITULO
#define T21_KICKER TXT_V2_KICK
#define T21_GROUP TXT_V2_GRUPO
#define T21_ROW_TITLE TXT_ILHA_NOME
#define T21_BODY TXT_V2_26
#define T21_SHEET_BODY TXT_ILHA_CORPO
#define T21_META TXT_ILHA_SUB
#define T21_SUPPORT TXT_ILHA_APOIO
#define T21_CHIP TXT_ILHA_SEG
#define T21_KEY TXT_AJ_KBD

// Poster dimensions remain screen-owned; use foco_zoom/foco_anel for art,
// plrui_linha_foco for rows and plrui_pilula_foco for buttons (never row rings).
#define T21_POSTER_SCALE 1.08f
#define T21_POSTER_LIFT NV_FOCO_LIFT
#define T21_POSTER_SHADOW NV_FOCO_SOMBRA
#define T21_POSTER_SHADOW_DY 16.0f
#define T21_POSTER_SHADOW_A .30f
#define T21_VEIL (ajustes_vidro() ? .30f : .42f)
#define T21_CHIP_REST .08f
#define T21_SEG_TRACK .05f
#define T21_SEG_SELECTED .14f
// Accent and contrast ink always come from ajustes_acento/ajustes_tinta_foco.
#define T21_TEXT_R 243
#define T21_TEXT_G 242
#define T21_TEXT_B 239
#define T21_TEXT_SECONDARY .60f
#define T21_TEXT_SUPPORT .45f
#define T21_FOCO NV_MOLA_FOCO
#define T21_SCROLL NV_MOLA2_SCROLL
#define T21_TELA NV_MOLA_TELA
#define T21_PAGINA NV_MOLA_PAGINA
#define T21_ENTER_MS 320.0f
#define T21_EXIT_MS 180.0f
#define T21_SHEET_SCALE .96f
#define T21_STAGGER_MS 28.0f
#define T21_STAGGER_N 8

// Returns text-layer units even during input/layout outside a draw call.
// Nested header/UI layers must convert these units to their own scale.
static inline void t21_inicio(float *x, float *w) {
  TXT_INI();
  ajustes_area_conteudo(NV_CONTENT_PAD, NV_LEGACY_CONTENT_RIGHT, x, w);
  TXT_FIM();
}

static inline void t21_ilha(GfxRect r, float raioPx, float a) {
  if (r.w <= 0 || r.h <= 0 || a <= .002f) return;
  float raio = raioPx / r.h;
  gfx_sombra_sob((GfxRect){r.x - 16, r.y - 4, r.w + 32, r.h + 30},
                1, 0, .5f, 0, 0, 0, .36f * a, r, raioPx, a);
  if (ajustes_vidro()) {
    // Modal density plus a dark base, as the 2.0.7 sheet: art behind never beats the text.
    gfx_cor(r, raio, .071f, .075f, .086f, .72f * a);
    gfx_vidro(r, raio, GFX_VIDRO_ILHA, .06f, GFX_VIDRO_DENS_MODAL * gfx_vidro_opacidade(), a);
    return;
  }
  gfx_cor(r, raio, .071f, .075f, .086f, .98f * a);
  gfx_luz_canto(r, raio, r.w * .22f, -r.h * .40f, fmaxf(r.w, r.h) * .62f,
                1, 1, 1, .04f * a);
}

// Text in approved fixed panel geometry still follows the user's text scale.
static inline TxtLinha t21_texto_linha(TxtEstilo estilo, const char *s,
                                      int r, int g, int b, int a, float max) {
  float scale = gfx_escala_txt();
  TXT_INI();
  TxtLinha l = txt_linha_corta(estilo, s, r, g, b, a, max / scale);
  TXT_FIM();
  l.w *= scale; l.h *= scale;
  return l;
}
static inline void t21_texto_desenhar(TxtLinha l, float x, float y, float a) {
  float scale = gfx_escala_txt();
  l.w /= scale; l.h /= scale;
  TXT_INI();
  txt_desenhar_alpha(l, x / scale, y / scale, a);
  TXT_FIM();
}

// Caller owns progress; anim_suave(progress) drives alpha and
// anim_mistura(T21_SHEET_SCALE, 1, alpha) drives a centered sheet transform.
static inline float t21_entrada(float progress, float dt, int aberto) {
  if (ajustes_animacoes_reduzidas_usuario()) return aberto ? 1 : 0;
  return anim_rampa(progress, aberto ? 1 : 0, dt, aberto ? T21_ENTER_MS : T21_EXIT_MS);
}

enum { T21_EMPTY, T21_LOADING, T21_ERROR };
// Text is already localized; action execution/focus registration stays with
// the screen. Return the retry/action rectangle, or an empty rectangle.
static inline GfxRect t21_estado(GfxRect r, int tipo, const char *icone,
                                const char *titulo, const char *texto,
                                const char *acao, float segundos, float a) {
  float x = r.x + T21_SHEET_PAD, y = r.y + T21_SHEET_PAD;
  float w = r.w - 2 * T21_SHEET_PAD;
  GfxRect button = {0};
  t21_ilha(r, T21_R_SHEET, a);
  if (tipo == T21_LOADING) {
    for (int i = 0; i < 3; i++) {
      float pulse = ajustes_animacoes_reduzidas_usuario() ? .7f :
                    .4f + .3f * (1 + sinf(segundos * 6 - i * 1.2f));
      gfx_cor((GfxRect){x + i * 18, y + 12, 8, 8}, .5f, 1, 1, 1, pulse * a);
    }
  } else if (icone) gfx_icone((GfxRect){x, y, 32, 32}, icone, 1, 1, 1, a);
  y += 56;
  y += txt_bloco(T21_SHEET_TITLE, titulo, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B,
                 x, y, w, 48, a, 2) + T21_CTRL_GAP;
  if (texto) txt_bloco(T21_SHEET_BODY, texto, T21_TEXT_R, T21_TEXT_G, T21_TEXT_B,
                      x, y, w, 34, T21_TEXT_SECONDARY * a, 3);
  if (acao && *acao) {
    TxtLinha label = txt_linha(T21_CHIP, acao, 255, 255, 255, 255);
    button = (GfxRect){x, r.y + r.h - T21_SHEET_PAD - T21_CTRL_H,
                      fminf(w, label.w + 48), T21_CTRL_H};
    plrui_pilula_foco(button, a);
    int ink = ajustes_tinta_foco();
    label = txt_linha(T21_CHIP, acao, ink, ink, ink, 255);
    txt_desenhar_alpha(label, button.x + (button.w - label.w) * .5f,
                       button.y + (button.h - label.h) * .5f, a);
  }
  return button;
}

// Zero-based step. Optional localized labels share Onboarding's four-bar grammar.
static inline void t21_passos(GfxRect r, int n, int atual,
                              const char *const *rotulos, float a) {
  if (n <= 0) return;
  float ar, ag, ab, w = (r.w - (n - 1) * T21_CTRL_GAP) / n;
  ajustes_acento(&ar, &ag, &ab);
  for (int i = 0; i < n; i++) {
    float x = r.x + i * (w + T21_CTRL_GAP);
    gfx_cor((GfxRect){x, r.y, w, 5}, .5f, i <= atual ? ar : 1,
            i <= atual ? ag : 1, i <= atual ? ab : 1, (i <= atual ? .9f : .12f) * a);
    if (rotulos) {
      TxtLinha label = txt_linha_corta(T21_SUPPORT, rotulos[i], T21_TEXT_R, T21_TEXT_G,
                                      T21_TEXT_B, 255, w);
      txt_desenhar_alpha(label, x, r.y + 16, (i <= atual ? .78f : .48f) * a);
    }
  }
}

static inline float t21_dica(const char *tecla, const char *rotulo,
                             float x, float yc, float a) {
  return plrui_dicas(&tecla, &rotulo, 1, x, yc, 0, a);
}
#endif
