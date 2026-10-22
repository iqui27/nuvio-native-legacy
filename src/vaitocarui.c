// Desenho da faixa "Vai tocar" — ver vaitocarui.h.
#include "vaitocarui.h"
#include "ajustes.h"
#include "idioma.h"
#include "idiomacod.h"
#include "plrui.h"
#include "text.h"
#include <stdio.h>
#include <string.h>

// Medidas do .vai do mockup (px de 1080p): padding 18/24, linha 1 a 30, filete
// de 1 px a 13 acima e abaixo, linha 2 a 26. O cartao e vidro (plrui_material).
#define VAI_PAD_Y   18.0f
#define VAI_PAD_X   24.0f
#define VAI_L1_H    30.0f
#define VAI_SEP     13.0f
#define VAI_L2_H    26.0f
#define VAI_RAIO    24.0f

float vtui_altura(const VaiTocar *vt) {
  float h;
  if (!vt || !vt->tem) return 0.0f;
  h = VAI_PAD_Y + VAI_L1_H + VAI_PAD_Y;
  if (vt->pularOn) h += VAI_SEP + 1.0f + VAI_SEP + VAI_L2_H - 0.0f;
  return h;
}

// Chip de qualidade (.b): 14/800 em caixa alta, fundo branco 10%, raio 7.
static float chipQual(const char *s, float x, float yc, float a) {
  char up[48];
  TxtLinha t;
  float w;
  if (!s || !s[0]) return 0.0f;
  idioma_maiusc(up, sizeof up, s);
  t = txt_linha(TXT_ILHA_NUM, up, 235, 235, 235, 255);
  w = (float)t.w + 18.0f;
  if (x >= 0.0f) {
    gfx_cor((GfxRect){ x, yc - 14.0f, w, 28.0f }, 0.25f, 1, 1, 1, 0.10f * a);
    txt_desenhar_alpha(t, x + 9.0f, yc - (float)t.h * 0.5f, a);
  }
  return w;
}

// Um item "Abertura" do .cob: icone 18 (ok = acento, sem = cinza) + rotulo.
static float itemPular(const char *rotulo, int tem, float x, float yc, float a) {
  float ar, ag, ab;
  TxtLinha t;
  int c = tem ? 158 : 115;
  t = txt_linha(TXT_ILHA_SUB, rotulo, c, c, c, 255);
  if (x < 0.0f) return 18.0f + 7.0f + (float)t.w;
  ajustes_acento(&ar, &ag, &ab);
  if (tem) gfx_icone((GfxRect){ x, yc - 9.0f, 18, 18 }, "check", ar, ag, ab, a);
  else gfx_icone((GfxRect){ x, yc - 9.0f, 18, 18 }, "aj_x", 0.45f, 0.45f, 0.45f, a);
  txt_desenhar_alpha(t, x + 25.0f, yc - (float)t.h * 0.5f, a);
  return 25.0f + (float)t.w;
}

void vtui_desenhar(const VaiTocar *vt, float x, float y, float larg, float a) {
  float h = vtui_altura(vt), cx, yc;
  GfxRect cartao;
  TxtLinha t;
  char buf[200];
  if (h <= 0.0f || a <= 0.002f) return;
  cartao = (GfxRect){ x, y, larg, h };
  plrui_material(cartao, VAI_RAIO, 0, a);
  cx = x + VAI_PAD_X;
  yc = y + VAI_PAD_Y + VAI_L1_H * 0.5f;

  // --- linha 1: Vai tocar [4K] [DOLBY VISION] [EAC3 5.1] · idioma · tamanho · cache
  cx += plrui_kicker("Vai tocar", cx, yc - 8.0f, 158, 158, 158, a) + 14.0f;
  cx += chipQual(vt->res, cx, yc, a) + (vt->res[0] ? 8.0f : 0.0f);
  cx += chipQual(vt->hdr, cx, yc, a) + (vt->hdr[0] ? 8.0f : 0.0f);
  cx += chipQual(vt->audio, cx, yc, a) + (vt->audio[0] ? 8.0f : 0.0f);
  buf[0] = 0;
  if (vt->idiomas[0]) snprintf(buf, sizeof buf, "\xc2\xb7 %s", vt->idiomas);
  if (vt->tamanho[0]) {
    size_t n = strlen(buf);
    snprintf(buf + n, sizeof buf - n, "%s%s", n ? " \xc2\xb7 " : "\xc2\xb7 ", vt->tamanho);
  }
  if (buf[0]) {
    t = txt_linha_corta(TXT_ILHA_SUB, buf, 158, 158, 158, 255,
                        (x + larg - VAI_PAD_X) - cx);
    txt_desenhar_alpha(t, cx, yc - (float)t.h * 0.5f, a);
    cx += (float)t.w + 12.0f;
  }
  if (vt->cache) {
    float ar, ag, ab;
    const char *s = i18n("em cache");
    ajustes_acento(&ar, &ag, &ab);
    t = txt_linha(TXT_ILHA_SUB, s, 235, 235, 235, 255);
    if (cx + 30.0f + (float)t.w <= x + larg - VAI_PAD_X) {
      gfx_icone((GfxRect){ cx, yc - 10.0f, 20, 20 }, "aj_zap", ar, ag, ab, a);
      txt_desenhar_alpha(t, cx + 28.0f, yc - (float)t.h * 0.5f, a);
    }
  }

  // --- linha 2: Dá para pular  [ok] Abertura ...        origem das marcas
  if (vt->pularOn) {
    static const struct { int bit; const char *chave; } ITEM[4] = {
      { VT_T_A, "Abertura" }, { VT_T_R, "Resumo" },
      { VT_T_C, "Créditos" }, { VT_T_P, "Prévia" } };
    float y2 = y + VAI_PAD_Y + VAI_L1_H + VAI_SEP;
    int i;
    gfx_cor((GfxRect){ x + VAI_PAD_X, y2, larg - 2.0f * VAI_PAD_X, 1.0f }, 0, 1, 1, 1, 0.08f * a);
    yc = y2 + 1.0f + VAI_SEP + VAI_L2_H * 0.5f;
    cx = x + VAI_PAD_X;
    cx += plrui_kicker("Dá para pular", cx, yc - 8.0f, 158, 158, 158, a) + 20.0f;
    for (i = 0; i < 4; i++) {
      int tem = (vt->marcas & ITEM[i].bit) != 0;
      const char *r = (!tem && ITEM[i].bit == VT_T_P) ? i18n("Prévia sem marcas")
                                                       : i18n(ITEM[i].chave);
      cx += itemPular(r, tem, cx, yc, a) + 20.0f;
    }
    if (vt->origem) {
      t = txt_linha_corta(TXT_ILHA_APOIO, vt_origem_rotulo(vt->origem), 115, 115, 115, 255,
                          (x + larg - VAI_PAD_X) - cx);
      txt_desenhar_alpha(t, x + larg - VAI_PAD_X - (float)t.w, yc - (float)t.h * 0.5f, a);
    }
  }
}

// --- chips A/R/C do episodio ----------------------------------------------------
#define CHIP_W 20.0f
#define CHIP_GAP 4.0f

// Contorno tracejado (1,5 px, tracos de 4 com vao de 4) nos quatro lados.
static void tracejado(float x, float y, float s, float a) {
  float e = 1.5f, d, in = 4.0f;
  int k;
  for (k = 0; k < 2; k++) {
    d = in + (float)k * 8.0f;
    gfx_cor((GfxRect){ x + d, y, 4, e }, 0, 1, 1, 1, 0.25f * a);
    gfx_cor((GfxRect){ x + d, y + s - e, 4, e }, 0, 1, 1, 1, 0.25f * a);
    gfx_cor((GfxRect){ x, y + d, e, 4 }, 0, 1, 1, 1, 0.25f * a);
    gfx_cor((GfxRect){ x + s - e, y + d, e, 4 }, 0, 1, 1, 1, 0.25f * a);
  }
}

float vtui_chips_episodio(int marcas, float x, float y, float a) {
  static const struct { int bit; const char *chave; } ITEM[3] = {
    { VT_T_A, "Abertura" }, { VT_T_R, "Resumo" }, { VT_T_C, "Créditos" } };
  float w = 0.0f;
  int i;
  if (!(marcas & (VT_T_A | VT_T_R | VT_T_C | VT_T_P))) return 0.0f;
  for (i = 0; i < 3; i++) {
    if (x >= 0.0f) {
      char ini[8] = "", up[16];
      const char *nome = i18n(ITEM[i].chave);
      int n = 1;
      TxtLinha t;
      // Inicial da palavra no idioma da pagina (UTF-8: 1 a 4 bytes).
      if ((unsigned char)nome[0] >= 0xF0) n = 4;
      else if ((unsigned char)nome[0] >= 0xE0) n = 3;
      else if ((unsigned char)nome[0] >= 0xC0) n = 2;
      memcpy(ini, nome, (size_t)n);
      ini[n] = 0;
      idioma_maiusc(up, sizeof up, ini);
      if (marcas & ITEM[i].bit) {
        gfx_cor((GfxRect){ x + w, y, CHIP_W, VTUI_CHIP_H }, 0.3f, 1, 1, 1, 0.12f * a);
        t = txt_linha(TXT_MINI, up, 235, 235, 235, 255);
      } else {
        tracejado(x + w, y, CHIP_W, a);
        t = txt_linha(TXT_MINI, up, 115, 115, 115, 255);
      }
      txt_desenhar_alpha(t, x + w + (CHIP_W - (float)t.w) * 0.5f,
                         y + (VTUI_CHIP_H - (float)t.h) * 0.5f, a);
    }
    w += CHIP_W + (i < 2 ? CHIP_GAP : 0.0f);
  }
  return w;
}
