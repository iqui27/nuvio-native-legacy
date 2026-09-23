#include "botoes.h"
#include "ajustes.h"
#include "anim.h"
#include "text.h"

// Superficie de repouso do primario: o cinza do painel flutuante (0.055/0.058/
// 0.068) um degrau acima, tingido para o azul do mesmo jeito. Nunca #000 nem
// cinza neutro puro — sobre o painel translucido o neutro puro sai esverdeado.
#define BT_REP_R 0.14f
#define BT_REP_G 0.15f
#define BT_REP_B 0.17f
// Texto em repouso: 235, o mesmo dos selos (badges.h). 255 sobre 0.15 vibra.
#define BT_TEXTO_REP 235

void botao_luz(GfxRect r, float foco, float a) {
  float fr, fg, fb;
  GfxRect luz;
  if (foco < 0.01f) return;
  ajustes_acento(&fr, &fg, &fb);
  // 0,9x a altura de folga em cada lado e alpha 0,35 x mola: a luz da pilula
  // em foco do menu lateral, copiada e nao reinterpretada.
  luz.x = r.x - r.h * 0.9f; luz.y = r.y - r.h * 0.9f;
  luz.w = r.w + r.h * 1.8f; luz.h = r.h * 2.8f;
  gfx_rect(luz, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f, fr, fg, fb, 0.35f * foco * a);
}

float botao_largura(const char *rotulo, const char *icone, int primario) {
  TxtLinha l = txt_linha(TXT_DET_BOTAO, rotulo, 0, 0, 0, 255);
  float w = (float)l.w + (primario ? BOTAO_PAD_X : BOTAO_PAD_X2) * 2.0f;
  if (icone && icone[0]) w += BOTAO_ICONE + BOTAO_ICONE_GAP;
  return w;
}

void botao_pilula(GfxRect r, const char *rotulo, const char *icone,
                  float foco, int primario, int alinhar, float a) {
  float fr, fg, fb, ti = ajustes_acento_tinta(&fr, &fg, &fb);
  int emFoco = foco >= 0.5f;
  int c = emFoco ? (int)(ti * 255.0f + 0.5f) : BT_TEXTO_REP;
  float raio = 0.5f;
  TxtLinha l;
  float temIcone = (icone && icone[0]) ? 1.0f : 0.0f;
  float grupo, x0, padx = primario ? BOTAO_PAD_X : BOTAO_PAD_X2;

  botao_luz(r, foco, a);
  if (primario) {
    gfx_cor(r, raio, anim_mistura(BT_REP_R, fr, foco),
            anim_mistura(BT_REP_G, fg, foco), anim_mistura(BT_REP_B, fb, foco), a);
  } else {
    // O miolo so existe na medida do foco; em repouso e o contorno. A
    // espessura do anel esta em fracao da ALTURA (o SDF normaliza por ela):
    // 1,5 px em 56 px.
    // (23/09) Em repouso tambem ha miolo: a superficie escura dos circulares
    // vizinhos (BT_REP), um pouco translucida. So contorno sumia sobre arte
    // clara e colorida — "Play from beginning" ilegivel sobre o One Piece.
    gfx_cor(r, raio, anim_mistura(BT_REP_R, fr, foco),
            anim_mistura(BT_REP_G, fg, foco), anim_mistura(BT_REP_B, fb, foco),
            (0.82f + 0.18f * foco) * a);
    if (foco < 0.99f)
      gfx_rect(r, 0, GFX_ANEL, 0, 1.5f / r.h, 0, raio, 1, 1, 1, 0.22f * (1.0f - foco) * a);
  }

  l = txt_linha(TXT_DET_BOTAO, rotulo, c, c, c, 255);
  grupo = (float)l.w + temIcone * (BOTAO_ICONE + BOTAO_ICONE_GAP);
  x0 = alinhar ? r.x + padx : r.x + (r.w - grupo) * 0.5f;
  if (temIcone > 0.0f) {
    float ic = (float)c / 255.0f;
    GfxRect ig = { x0, r.y + (r.h - BOTAO_ICONE) * 0.5f, BOTAO_ICONE, BOTAO_ICONE };
    gfx_icone(ig, icone, ic, ic, ic, a);
    x0 += BOTAO_ICONE + BOTAO_ICONE_GAP;
  }
  txt_desenhar_alpha(l, x0, r.y + (r.h - (float)l.h) * 0.5f, a);
}

void botao_disco(GfxRect r, const char *icone, float foco, float a) {
  float fr, fg, fb, ti = ajustes_acento_tinta(&fr, &fg, &fb);
  float ic = foco >= 0.5f ? ti : 1.0f;
  // O glifo mede um terco do disco — proporcao medida no aparelho (detail.h,
  // NV_DETW2_CIRC_GLIFO).
  float g = r.w * 0.333f;
  GfxRect ig = { r.x + (r.w - g) * 0.5f, r.y + (r.h - g) * 0.5f, g, g };
  botao_luz(r, foco, a);
  gfx_cor(r, 0.5f, anim_mistura(BT_REP_R, fr, foco),
          anim_mistura(BT_REP_G, fg, foco), anim_mistura(BT_REP_B, fb, foco), a);
  gfx_icone(ig, icone, ic, ic, ic, a);
}
