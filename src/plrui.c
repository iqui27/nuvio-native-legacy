// Pecas do player no Glass UI — ver plrui.h.
#include "plrui.h"
#include "ajustes.h"
#include "anim.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static float raioDe(GfxRect r, float px) {
  float m = r.h > 1.0f ? r.h : 1.0f, f = px / m;
  return f > 0.5f ? 0.5f : f;
}

void plrui_material(GfxRect r, float raioPx, int modal, float a) {
  plrui_material_s(r, raioPx, modal, a, 0);
}

void plrui_material_s(GfxRect r, float raioPx, int modal, float a, int semSombra) {
  float raio = raioDe(r, raioPx);
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.002f) return;
  if (ajustes_vidro()) {
    // box-shadow 0 14 40 rgba(0,0,0,.36): caida, nunca halo.
    if (!semSombra) gfx_rect((GfxRect){ r.x - 20.0f, r.y - 6.0f, r.w + 40.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
             1.0f, 0, 0, 0.5f, 0, 0, 0, 0.36f * a);
    gfx_vidro_material(r, raio, modal ? GFX_VIDRO_DENS_MODAL : GFX_VIDRO_DENS, a);   // + gfx_vidro_luz
  } else {
    if (!semSombra) gfx_sombra_sob((GfxRect){ r.x - 16.0f, r.y - 4.0f, r.w + 32.0f, r.h + 30.0f }, 1.0f, 0, 0.5f,
                   0, 0, 0, 0.45f * a, r, raio * r.h, a);
    gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, a);
  }
}

// Same material as the title-page cards (detail.c cartaoVidro): the shared
// glass at GFX_VIDRO_DENS with the island drop shadow. Frost only where the
// glass samples it; glass off keeps the solid #15161A of plrui_material.
void plrui_cartao(GfxRect r, float raioPx, float a) {
  float raio = raioDe(r, raioPx);
  if (!ajustes_vidro()) { plrui_material(r, raioPx, 1, a); return; }
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.002f) return;
  gfx_rect((GfxRect){ r.x - 20.0f, r.y - 6.0f, r.w + 40.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, 0.36f * a);
  gfx_vidro_material(r, raio, GFX_VIDRO_DENS, a);
}

void plrui_linha_foco(GfxRect r, float raioPx, float a) {
  float raio = raioDe(r, raioPx);
  if (a <= 0.002f) return;
  if (ajustes_vidro()) { gfx_cor(r, raio, 1, 1, 1, 0.12f * a); return; }
  gfx_rect((GfxRect){ r.x - 14.0f, r.y - 2.0f, r.w + 28.0f, r.h + 30.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, 0, 0, 0, 0.40f * a);
  gfx_cor(r, raio, 0.169f, 0.176f, 0.204f, a);
  // inset 0 1px 0 rgba(255,255,255,.06): o filete claro de cima.
  gfx_brilho_topo(r, raio, 2.0f / (r.h > 2.0f ? r.h : 2.0f), 1, 1, 1, 0.06f * a);
}

static void pilulaFoco(GfxRect r, float a, int video);
void plrui_pilula_foco(GfxRect r, float a) {
  pilulaFoco(r, a, 0);
}
void plrui_pilula_foco_video(GfxRect r, float a) {
  pilulaFoco(r, a, 1);
}
static void pilulaFoco(GfxRect r, float a, int video) {
  float ar, ag, ab;
  if (a <= 0.002f) return;
  ajustes_acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 22.0f, r.y - 4.0f, r.w + 44.0f, r.h + 40.0f }, 0, GFX_SOMBRA,
           1.0f, 0, 0, 0.5f, ar, ag, ab, 0.42f * a);
  if (video && !ajustes_vidro())
    gfx_cor(r, 0.5f, ar, ag, ab, (0.30f + 0.45f * ajustes_vidro_opacidade()) * a);
  else gfx_cor(r, 0.5f, ar, ag, ab, a);
}

void plrui_botao_repouso(GfxRect r, float a) {
  if (ajustes_vidro()) { gfx_vidro_painel(r, 0.5f, 0.55f, a); return; }
  gfx_cor(r, 0.5f, 0.141f, 0.149f, 0.173f, a);
}

void plrui_botao_repouso_video(GfxRect r, float a) {
  if (ajustes_vidro()) plrui_botao_repouso(r, a);
  else plrui_disco_osd(r, a);
}

// Post-play / error islands: live video behind the GL plane (same reasoning
// as plrui_disco_osd). Glass off = the same see-through tint, not the opaque
// island material, so an idle button in it has no opaque parent.
void plrui_material_video(GfxRect r, float raioPx, float a) {
  float raio = raioDe(r, raioPx);
  if (ajustes_vidro()) { plrui_material(r, raioPx, 0, a); return; }
  if (r.w <= 0.0f || r.h <= 0.0f || a <= 0.002f) return;
  gfx_sombra_sob((GfxRect){ r.x - 16.0f, r.y - 4.0f, r.w + 32.0f, r.h + 30.0f }, 1.0f, 0, 0.5f,
                 0, 0, 0, 0.45f * a, r, raio * r.h, a);
  gfx_cor(r, raio, 0.082f, 0.086f, 0.102f, (0.30f + 0.45f * ajustes_vidro_opacidade()) * a);
}

// The OSD discs float over the live video (hardware plane under an alpha-0
// hole): nothing to sample, so glass off is the same see-through tint glass on
// uses there (gfx_vidro: 0.30 + 0.45 t), never the opaque #15161A, plus a rim.
void plrui_disco_osd(GfxRect r, float a) {
  if (ajustes_vidro()) { gfx_vidro_material(r, 0.5f, 0.52f, a); return; }
  gfx_cor(r, 0.5f, 0.082f, 0.086f, 0.102f, (0.30f + 0.45f * ajustes_vidro_opacidade()) * a);
  gfx_anel(r, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.14f * a);
}

int plrui_tinta(void) { return ajustes_tinta_foco(); }

void plrui_decimal(char *s) {
  idioma_decimal_texto(s, ajustes_idioma());
}

#define BT_H     60.0f
#define BT_PAD   28.0f
#define BT_ICONE 22.0f
#define BT_VAO   12.0f
float plrui_botao_largura(const char *rotulo, const char *icone) {
  float w = (float)txt_largura(TXT_G21B, i18n(rotulo)) + BT_PAD * 2.0f;
  if (icone) w += BT_ICONE + BT_VAO;
  return w;
}
static float botao(float x, float y, const char *rotulo, const char *icone, float foco, float a, int video) {
  float w = plrui_botao_largura(rotulo, icone);
  GfxRect r = { x, y, w, BT_H };
  int c = 225;
  float f = anim_clamp(foco, 0.0f, 1.0f), tx = x + BT_PAD;
  // O FOCO E UMA MOLA, nao um limiar: quem chama ja anima o 0..1 (ilha.c,
  // reacao.c) e os que ainda passam 0/1 cravado simplesmente param nos extremos.
  // Repouso esvai enquanto a pilula do acento cresce; o TEXTO rasterizado nao
  // pode trocar de cor a cada passo (cache), entao a tinta troca no ponto medio.
  if (f > 0.005f) {
    if (video) plrui_pilula_foco_video(r, a * f);
    else plrui_pilula_foco(r, a * f);
  }
  if (f < 0.995f) {
    if (video) plrui_botao_repouso_video(r, a * (1.0f - f));
    else plrui_botao_repouso(r, a * (1.0f - f));
  }
  if (f > 0.5f) c = plrui_tinta();
  if (icone) {
    float k = c / 255.0f;
    gfx_icone((GfxRect){ tx, y + (BT_H - BT_ICONE) * 0.5f, BT_ICONE, BT_ICONE }, icone, k, k, k, a);
    tx += BT_ICONE + BT_VAO;
  }
  { TxtLinha l = txt_linha(TXT_G21B, rotulo, c, c, c, 255);
    txt_desenhar_alpha(l, tx, y + (BT_H - (float)l.h) * 0.5f, a); }
  return w;
}

float plrui_botao(float x, float y, const char *rotulo, const char *icone, float foco, float a) {
  return botao(x, y, rotulo, icone, foco, a, 0);
}
float plrui_botao_video(float x, float y, const char *rotulo, const char *icone, float foco, float a) {
  return botao(x, y, rotulo, icone, foco, a, 1);
}

float plrui_kicker(const char *s, float x, float y, int r, int g, int b, float a) {
  char up[200];
  idioma_maiusc(up, sizeof up, i18n(s));
  if (x < 0.0f) return txt_tracking(TXT_MINI, up, r, g, b, -10000.0f, y, 0.0f, 2.1f);
  return txt_tracking(TXT_MINI, up, r, g, b, x, y, a, 2.1f);
}

#define KBD_H   30.0f
static float dicas(const char *const *teclas, const char *const *rotulos, int n,
                   float x, float yc, int alinhaDir, float a, int video) {
  float tot = 0.0f, xx;
  int i, pass;
  for (pass = 0; pass < 2; pass++) {
    xx = pass == 0 ? 0.0f : (alinhaDir ? x - tot : x);
    for (i = 0; i < n; i++) {
      TxtLinha k = txt_linha(TXT_G14B, teclas[i], 209, 207, 204, 255);
      TxtLinha l = txt_linha(TXT_ILHA_APOIO, rotulos[i], 128, 128, 126, 255);
      float kw = (float)k.w + 18.0f;
      if (kw < 34.0f) kw = 34.0f;
      if (pass == 1 && x >= 0.0f) {
        GfxRect kr = { xx, yc - KBD_H * 0.5f, kw, KBD_H };
        if (ajustes_vidro()) gfx_cor(kr, 0.5f, 1, 1, 1, 0.09f * a);
        else if (video) gfx_cor(kr, 0.5f, 0.082f, 0.086f, 0.102f,
                                (0.30f + 0.45f * ajustes_vidro_opacidade()) * a);
        else gfx_cor(kr, 0.5f, 0.141f, 0.149f, 0.173f, a);
        txt_desenhar_alpha(k, xx + (kw - (float)k.w) * 0.5f, yc - (float)k.h * 0.5f, a);
        txt_desenhar_alpha(l, xx + kw + 9.0f, yc - (float)l.h * 0.5f, a);
      }
      xx += kw + 9.0f + (float)l.w + (i + 1 < n ? 22.0f : 0.0f);
    }
    if (pass == 0) tot = xx;
  }
  return tot;
}

float plrui_dicas(const char *const *teclas, const char *const *rotulos, int n,
                  float x, float yc, int alinhaDir, float a) {
  return dicas(teclas, rotulos, n, x, yc, alinhaDir, a, 0);
}
float plrui_dicas_video(const char *const *teclas, const char *const *rotulos, int n,
                        float x, float yc, int alinhaDir, float a) {
  return dicas(teclas, rotulos, n, x, yc, alinhaDir, a, 1);
}

#define SEG_PAD  5.0f
#define SEG_VAO  4.0f
#define SEG_IH  44.0f
// A PILULA DE FOCO ENTRE OS CHIPS, na posicao fracionaria `sel`: desliza de
// chip em chip em vez de acender no lugar. Pura (sem desenho), com teste em
// tests/animacoes205.c; `sel` < 0 = nada selecionado/focado.
static GfxRect segPilulaRect(const float *w, int n, float sel, float x, float y) {
  float xs[8], xx = x + SEG_PAD;
  int i, lo, hi;
  float t;
  if (n <= 0 || sel < 0.0f) return (GfxRect){ 0, 0, 0, 0 };
  if (sel > (float)(n - 1)) sel = (float)(n - 1);
  lo = (int)floorf(sel); hi = lo + 1;
  t = sel - (float)lo;
  if (hi > n - 1) { hi = n - 1; lo = n - 1; t = 0.0f; }
  for (i = 0; i < n; i++) { xs[i] = xx; xx += w[i] + SEG_VAO; }
  { GfxRect r = { anim_mistura(xs[lo], xs[hi], t), y + SEG_PAD,
                  anim_mistura(w[lo], w[hi], t), SEG_IH };
    return r; }
}
float plrui_seg(const char *const *rotulos, const int *contagem, int n, float sel, float ed,
                float x, float y, float a) {
  float w[8], tot = SEG_PAD * 2.0f, xx;
  TxtLinha t[8], c[8];
  int i, vid = ajustes_vidro(), tinta = plrui_tinta();
  // `sel` e `ed` podem ser FRACIONARIOS: quem chama anima (mola) e a pilula
  // desliza/esmaece entre os chips em vez de saltar. Inteiros cravados
  // continuam desenhando exatamente como antes.
  float pos = sel, fEd = anim_clamp(ed, 0.0f, 1.0f);
  int selI = pos < 0.0f ? -1 : (int)(pos + 0.5f);
  if (n > 8) n = 8;
  for (i = 0; i < n; i++) {
    int s = i == selI, f = s && fEd > 0.5f;
    int cor = f ? tinta : 243;
    t[i] = txt_linha(TXT_ILHA_SEG, rotulos[i], cor, cor, cor, f || s ? 255 : 140);
    memset(&c[i], 0, sizeof c[i]);
    w[i] = (float)t[i].w + 40.0f;
    if (contagem && contagem[i] >= 0) {
      char b[16];
      int cc = f ? tinta : 243;
      snprintf(b, sizeof b, "%d", contagem[i]);
      c[i] = txt_linha(TXT_ILHA_NUM, b, cc, cc, cc, f ? 150 : 89);
      w[i] += 9.0f + (float)c[i].w;
    }
    tot += w[i] + (i ? SEG_VAO : 0.0f);
  }
  if (x < 0.0f) return tot;
  { GfxRect tr = { x, y, tot, SEG_IH + SEG_PAD * 2.0f };
    if (vid) gfx_vidro_folha(tr, 0.5f, a);
    else gfx_cor(tr, 0.5f, 0.114f, 0.118f, 0.137f, a); }
  xx = x + SEG_PAD;
  for (i = 0; i < n; i++) {
    GfxRect ir = { xx, y + SEG_PAD, w[i], SEG_IH };
    // A pilula de foco desliza entre os chips (posicao fracionaria) e esmaece
    // ao entrar/sair do segmentado; o realce de SELECAO fica no chip inteiro.
    if (i == selI) {
      if (fEd < 0.5f) {
        if (vid) gfx_cor(ir, 0.5f, 1, 1, 1, 0.14f * a);
        else gfx_cor(ir, 0.5f, 0.204f, 0.212f, 0.243f, a);
      } else if (fEd < 0.995f) {
        if (vid) gfx_cor(ir, 0.5f, 1, 1, 1, 0.14f * (1.0f - fEd) * a);
        else gfx_cor(ir, 0.5f, 0.204f, 0.212f, 0.243f, (1.0f - fEd) * a);
      }
    }
    xx += w[i] + SEG_VAO;
  }
  // Pilula antes do texto: o acento nao pode cobrir os rotulos durante a mola.
  if (fEd > 0.005f && selI >= 0)
    plrui_pilula_foco(segPilulaRect(w, n, pos, x, y), a * fEd);
  xx = x + SEG_PAD;
  for (i = 0; i < n; i++) {
    float yb = y + SEG_PAD + (SEG_IH - (float)t[i].h) * 0.5f;
    txt_desenhar_alpha(t[i], xx + 20.0f, yb, a);
    if (c[i].w) txt_desenhar_alpha(c[i], xx + 20.0f + (float)t[i].w + 9.0f,
                                   yb + (float)t[i].h - (float)c[i].h - 1.0f, a);
    xx += w[i] + SEG_VAO;
  }
  return tot;
}

void plrui_barra(float x, float y, float w, float frac, float buf, int foco,
                 const float *caps, int nCaps, float a) {
  float ar, ag, ab, h = foco ? 10.0f : 6.0f, yy = foco ? y - 2.0f : y;
  int i;
  if (a <= 0.002f) return;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  ajustes_acento(&ar, &ag, &ab);
  gfx_cor((GfxRect){ x, yy, w, h }, 0.5f, 1, 1, 1, 0.20f * a);
  if (buf > frac + 0.002f) {
    if (buf > 1.0f) buf = 1.0f;
    gfx_cor((GfxRect){ x, yy, w * buf, h }, 0.5f, 1, 1, 1, 0.30f * a);
  }
  // Mesmo o primeiro pixel aparece: o raio 0,5 vale para qualquer largura >= h.
  if (w * frac > 0.5f) {
    float fw = w * frac;
    if (fw < h) gfx_cor((GfxRect){ x, yy, fw, h }, 0.0f, ar, ag, ab, a);
    else gfx_cor((GfxRect){ x, yy, fw, h }, 0.5f, ar, ag, ab, a);
  }
  for (i = 0; i < nCaps; i++)
    gfx_cor((GfxRect){ x + w * caps[i] - 1.5f, yy - 4.0f, 3.0f, h + 8.0f }, 0.5f, 1, 1, 1, 0.55f * a);
  if (foco) {
    float cx = x + w * frac, cy = yy + h * 0.5f;
    gfx_rect((GfxRect){ cx - 26.0f, cy - 20.0f, 52.0f, 56.0f }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
             0, 0, 0, 0.5f * a);
    gfx_cor((GfxRect){ cx - 13.0f, cy - 13.0f, 26.0f, 26.0f }, 0.5f, 1, 1, 1, a);
  }
}

void plrui_trilho(GfxRect r, float frac, float cr, float cg, float cb, float a) {
  if (cr < 0.0f) ajustes_acento(&cr, &cg, &cb);
  if (ajustes_vidro()) gfx_vidro_chip(r, 0.5f, 0.55f, a);
  else gfx_cor(r, 0.5f, 1, 1, 1, 0.16f * a);
  if (frac > 1.0f) frac = 1.0f;
  if (frac > 0.0f && r.w * frac > 0.5f)
    gfx_cor((GfxRect){ r.x, r.y, r.w * frac, r.h }, r.w * frac >= r.h ? 0.5f : 0.0f, cr, cg, cb, a);
}

static void anelPontos(float cx, float cy, float d, int cinza, int sombra, Uint32 agora, float a);
void plrui_anel(float cx, float cy, float d, int cinza, Uint32 agora, float a) {
  anelPontos(cx, cy, d, cinza, 0, agora, a);
}
// O ANEL SOLTO: so os pontos girando, sem disco por tras (dono, 05/10: "tira o
// fundo cinza opaco"). Sem placa, o contraste sobre cena clara vem de uma
// sombra suave sob CADA ponto (um disco preto maior e fraco), nao de um fundo.
void plrui_anel_solto(float cx, float cy, float d, Uint32 agora, float a) {
  anelPontos(cx, cy, d, 0, 1, agora, a);
}
static void anelPontos(float cx, float cy, float d, int cinza, int sombra, Uint32 agora, float a) {
  float cr = 0.953f, cg = 0.949f, cb = 0.937f, rp = d / 14.0f, ra = d * 0.5f - 6.0f;
  int k, giro;
  if (!cinza && !sombra) ajustes_acento(&cr, &cg, &cb);   // solto: branco, o acento some em cena clara
  // Gira por passos de 1/12 (o desenho do mockup e parado; aqui a cauda anda).
  giro = ajustes_animacoes_reduzidas() ? 0 : (int)((agora / 83u) % 12u);
  for (k = 0; k < 12; k++) {
    float ang = (k / 12.0f) * 6.2831853f;
    int idade = ((12 - k) + giro) % 12;
    float op = 0.15f + 0.85f * (float)idade / 11.0f;
    float px = cx + sinf(ang) * ra, py = cy - cosf(ang) * ra;
    if (sombra) {
      float rs = rp + 3.0f;
      gfx_cor((GfxRect){ px - rs, py - rs + 1.0f, rs * 2.0f, rs * 2.0f }, 0.5f, 0, 0, 0, 0.30f * op * a);
    }
    gfx_cor((GfxRect){ px - rp, py - rp, rp * 2.0f, rp * 2.0f }, 0.5f, cr, cg, cb,
            op * a * (cinza ? 0.7f : 1.0f));
  }
}

void plrui_respira(float cx, float cy, float d, Uint32 agora, float a) {
  float cr, cg, cb, p;
  ajustes_acento(&cr, &cg, &cb);
  p = ajustes_animacoes_reduzidas() ? 1.0f
      : 0.70f + 0.30f * sinf((float)agora * (6.2831853f / 1200.0f));
  gfx_rect((GfxRect){ cx - d * 0.5f - 6.0f, cy - d * 0.5f - 6.0f, d + 12.0f, d + 12.0f }, 0,
           GFX_DISCO, 0, 0, 0, 0, cr, cg, cb, 0.22f * a * p);
  gfx_cor((GfxRect){ cx - d * 0.5f, cy - d * 0.5f, d, d }, 0.5f, cr, cg, cb, a);
}

void plrui_sep(float x, float yc, float a) {
  gfx_cor((GfxRect){ x, yc - 11.0f, 1.0f, 22.0f }, 0.0f, 1, 1, 1, 0.18f * a);
}

void plrui_tempo(char *b, size_t n, double seg) {
  int t = seg > 0.0 ? (int)(seg + 0.5) : 0, h = t / 3600, m = (t / 60) % 60, s = t % 60;
  if (h > 0) snprintf(b, n, "%d:%02d:%02d", h, m, s);
  else snprintf(b, n, "%d:%02d", m, s);
}

void plrui_limpar_sep(char *s) {
  static const char PONTO[] = "\xc2\xb7";
  char *r, *w, *p;
  int mudou = 1;
  if (!s) return;
  while (mudou) {
    size_t n;
    mudou = 0;
    // pontas: espacos e o ponto medio
    while (*s == ' ' || !strncmp(s, PONTO, 2)) { memmove(s, s + (*s == ' ' ? 1 : 2), strlen(s) - (*s == ' ' ? 1 : 2) + 1); mudou = 1; }
    n = strlen(s);
    while (n && (s[n - 1] == ' ' || (n >= 2 && !strncmp(s + n - 2, PONTO, 2)))) {
      n -= s[n - 1] == ' ' ? 1 : 2; s[n] = 0; mudou = 1;
    }
    // "· ·" (com espacos no meio) vira um so
    for (p = strstr(s, PONTO); p; p = strstr(p + 2, PONTO)) {
      char *q = p + 2;
      while (*q == ' ') q++;
      if (!strncmp(q, PONTO, 2)) { memmove(p, q, strlen(q) + 1); mudou = 1; break; }
    }
  }
  (void)r; (void)w;
}
