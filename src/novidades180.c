// Cartao de NOVIDADES DA 1.8.0 — ver novidades180.h.
//
// MEDIDAS: as do mockup aprovado (novidades-mockup.html, quadros "novidades",
// "novidades-peek-2/3", "-agora-foco", "-guia-foco", "-previa-foco" e
// "-instalacao-nova"), em px de 1080p, conferidas lado a lado com a captura
// (tests/novidades180_shot.sh) nos dois materiais.
//
// A PREVIA E UMA TELA DE VERDADE EM MINIATURA. As cenas sao desenhadas na
// escala 1 do mockup (uma tela virtual de 972 x 1278) com as pecas do Glass UI
// — o material da ilha (ajustes_ui_ilha), as marcas de formato de badges.h, o
// grafico de memoria de Ajustes — num alvo proprio (gfx_mini_*) e mostradas a
// 72%, como o mockup faz com transform: scale(.72). Nao chamam streams.c,
// ilha.c nem ajustes.c de verdade: aqueles modulos sao estado global da tela
// (a folha aberta, a fila de avisos, o foco de Ajustes) e mexer neles daqui
// mudaria o app debaixo do cartao. Os textos sao os deles (as chaves de i18n
// sao as mesmas: "Pedido de amizade", "Recusar não avisa a pessoa."...).
//
// DADOS, NAO CODIGO: cenas (CENAS) e linhas (GRUPOS/ITENS) sao tabelas.
#include "novidades180.h"
#include "novidadesfila.h"
#include "ajustes.h"
#include "anim.h"
#include "badges.h"
#include "dados.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "ponteiro.h"
#include "teclado.h"
#include "tex_cache.h"
#include "text.h"
#include "textogate.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------------ geometria
#define N180_X        100.0f
#define N180_Y         40.0f
#define N180_W       1720.0f
#define N180_H       1000.0f
#define N180_RAIO      40.0f
#define PV_X          (N180_X + 40.0f)
#define PV_Y          (N180_Y + 40.0f)
#define PV_W          700.0f
#define PV_H          920.0f
#define PV_RAIO        28.0f
#define PV_ESC         0.72f
#define COL_X         (N180_X + 800.0f)
#define COL_Y         (N180_Y + 52.0f)
#define COL_W         (N180_W - 800.0f - 52.0f)
#define N180_ABRIR_MS 280.0f
#define N180_FECHAR_MS 160.0f
#define N180_TROCA_S    0.7f    // a passagem de uma cena a outra (.cena opacity .7s)
#define N180_CENA_S     4.6f    // cada cena na tela
#define CW  N180_CENA_W
#define CH  N180_CENA_H
#define CR  128.0f               // a faixa de baixo da cena, onde mora o seletor (92 / .72)

enum { B_AGORA = 0, B_GUIA, B_VIDRO, B_N };

static int   aberto, decidido, boasVindas, foco = B_VIDRO, naPrevia, pedido;
static int   cena, cenaAntiga;
static float entrada, transicao = 1.0f, relogioCena;
static char  dirArte[512] = "deploy/app/art";
static TextoGate gateLista;
static GfxMini mini;

// ------------------------------------------------------------------ material
#define TX 243, 242, 239
static int vid(void) { return ajustes_vidro(); }
static void acento(float *r, float *g, float *b) { ajustes_acento(r, g, b); }
static void acento8(int *r, int *g, int *b) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  *r = (int)(ar * 255.0f + 0.5f); *g = (int)(ag * 255.0f + 0.5f); *b = (int)(ab * 255.0f + 0.5f);
}
// Neutro claro: branco a `vidA` no vidro, o cinza opaco do mockup no solido.
static void neutro(GfxRect r, float raioPx, float vidA, float sr, float sg, float sb, float a) {
  if (vid()) gfx_cor(r, raioPx / r.h, 1, 1, 1, vidA * a);
  else gfx_cor(r, raioPx / r.h, sr, sg, sb, a);
}
#define SOL_CHIP 0.141f, 0.149f, 0.173f   // #24262C
// Pilula cheia no acento, com a sombra colorida do botao em foco.
static void pilulaAcento(GfxRect r, float a) {
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  gfx_rect((GfxRect){ r.x - 18, r.y + 2, r.w + 36, r.h + 40 }, 0, GFX_SOMBRA, 1.0f, 0, 0, 0.5f,
           ar, ag, ab, 0.35f * a);
  gfx_cor(r, 0.5f, ar, ag, ab, a);
}
static TxtLinha txt(TxtEstilo e, const char *s) { return txt_linha(e, s, TX, 255); }
static TxtLinha txtC(TxtEstilo e, const char *s, float w) { return txt_linha_corta(e, s, TX, 255, w); }
static TxtLinha txtT(TxtEstilo e, const char *s) { int t = ajustes_tinta_foco(); return txt_linha(e, s, t, t, t, 255); }
static TxtLinha txtA(TxtEstilo e, const char *s) { int r, g, b; acento8(&r, &g, &b); return txt_linha(e, s, r, g, b, 255); }
// Caixa alta espacada (kicker), na regra de maiusculas do idioma.
static float caps(TxtEstilo e, const char *s, float track, float x, float y, int r, int g, int b, float a) {
  char up[240];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(e, up, r, g, b, x, y, a, track);
}
static void icone(const char *nome, GfxRect r, float c, float a) { gfx_icone(r, nome, c * 0.953f, c * 0.949f, c * 0.937f, a); }
static void iconeA(const char *nome, GfxRect r, float a) { float ar, ag, ab; acento(&ar, &ag, &ab); gfx_icone(r, nome, ar, ag, ab, a); }

// ------------------------------------------------------------------ a arte
static char caminho[8][600];
static int  nCaminho;
static const char *arte(const char *rel) {
  char *b = caminho[nCaminho++ % 8];
  snprintf(b, sizeof caminho[0], "%s/%s", dirArte, rel);
  return b;
}
// "cover" com o ponto de ancoragem do object-position do mockup (0..1).
static void img(const char *rel, GfxRect r, float raioPx, float px, float a) {
  const char *c = arte(rel);
  GLuint t = tex_obter_larg(c, r.w > r.h * 1.78f ? r.w : r.h * 1.78f);
  float asp;
  if (!t) { gfx_cor(r, raioPx / r.h, 0.10f, 0.11f, 0.13f, a); return; }
  asp = tex_aspecto(c);
  if (asp <= 0.0f) asp = 16.0f / 9.0f;
  // GFX_VITRINE = cover com ancoragem vertical; para a horizontal o recorte e
  // feito aqui: a arte e desenhada mais larga e a tesoura da miniatura corta.
  if (asp > r.w / r.h && px != 0.5f) {
    float w = r.h * asp, x = r.x - (w - r.w) * px;
    gfx_tex_aspect_atual = asp;
    gfx_recorte(r.x, r.y, r.w, r.h);
    gfx_rect((GfxRect){ x, r.y, w, r.h }, t, GFX_ARTE, 0, 0, 0, 0, 1, 1, 1, a);
    gfx_sem_recorte();
    gfx_tex_aspect_atual = 0.0f;
    return;
  }
  gfx_tex_aspect_atual = asp;
  gfx_card_forcar_cover_atual = 1.0f;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, raioPx / r.h, 0, 0, 0, a);
  gfx_card_forcar_cover_atual = 0.0f;
  gfx_tex_aspect_atual = 0.0f;
}
// Logo do titulo inteiro em maxW x maxH, a partir de (x, y).
static void logo(const char *rel, float x, float y, float maxW, float maxH, float a) {
  const char *c = arte(rel);
  GLuint t = tex_obter_larg(c, maxW);
  float asp, w, h;
  if (!t) return;
  asp = tex_aspecto(c);
  if (asp <= 0.0f) asp = 3.0f;
  w = maxW; h = w / asp;
  if (h > maxH) { h = maxH; w = h * asp; }
  gfx_tex_aspect_atual = 0.0f;
  gfx_rect((GfxRect){ x, y, w, h }, t, tex_marca_escura(c) ? GFX_MARCA : GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, a);
}
static void pedirArtes(void) {
  static const char *const A[] = { "00.jpg", "03.jpg", "21.jpg", "13.jpg", "logo/03.png",
                                   "poster/02.jpg", "poster/12.jpg", "poster/15.jpg", "poster/19.jpg" };
  int i;
  for (i = 0; i < (int)(sizeof A / sizeof *A); i++) tex_obter_larg(arte(A[i]), i < 4 ? 1400.0f : 330.0f);
}

// Marca de formato (o pacote branco de badges.h) com a altura do mockup.
static float marca(const char *id, float x, float y, float h, float a) {
  return badges_desenhar_tom(badges_bit(id), x, y, 400.0f, h, 1, 1, 1, a);
}

// ============================================================ CENA: FONTES
// A folha de Fontes (streams.c no Glass UI): kicker e titulo, os filtros, as
// abas por addon, a linha "Melhor para esta TV" em foco e os grupos 4K/1080p.
typedef struct { const char *mk; int mkAc; const char *gb; const char *b[3]; const char *tag; int tagAc; const char *ad; } LinhaF;
static float linhaFonte(const LinhaF *l, int focada, float x, float y, float w) {
  float h = l->mk ? 123.0f : 101.0f, cy, bx;
  int i;
  if (focada) {
    if (vid()) gfx_cor((GfxRect){ x, y, w, h }, 22.0f / h, 1, 1, 1, 0.12f);
    else {
      gfx_rect((GfxRect){ x - 10, y + 2, w + 20, h + 26 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.30f);
      gfx_cor((GfxRect){ x, y, w, h }, 22.0f / h, 0.169f, 0.176f, 0.204f, 1);
    }
  }
  x += 22.0f; w -= 44.0f; y += 18.0f;
  if (l->mk) {
    float ar, ag, ab;
    acento(&ar, &ag, &ab);
    if (l->mkAc) gfx_cor((GfxRect){ x, y + 4.5f, 7, 7 }, 0.5f, ar, ag, ab, 1);
    else gfx_cor((GfxRect){ x, y + 4.5f, 7, 7 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.5f);
    if (l->mkAc) { int r, g, b; acento8(&r, &g, &b); caps(TXT_AJ_CAPS13, l->mk, 1.56f, x + 15, y, r, g, b, 1); }
    else caps(TXT_AJ_CAPS13, l->mk, 1.56f, x + 15, y, TX, 0.5f);
    y += 22.0f;
  }
  { TxtLinha n = txt(TXT_G26B, "Fallout"), te = txt(TXT_AJ_18, "T1 E3");
    TxtLinha g = txt(TXT_AJ_NOME, l->gb), u = txt(TXT_AJ_16B, "GB");
    cy = y + 15.0f;
    txt_desenhar_alpha(n, x, cy - n.h * 0.5f, focada ? 1.0f : 0.85f);
    txt_desenhar_alpha(te, x + n.w + 6.0f + 7.0f, cy - te.h * 0.5f + 3.0f, 0.5f);
    txt_desenhar_alpha(u, x + w - u.w, cy - u.h * 0.5f + 3.0f, 0.45f);
    txt_desenhar_alpha(g, x + w - u.w - 5.0f - g.w, cy - g.h * 0.5f, 1.0f); }
  y += 31.0f;
  cy = y + 24.0f;
  bx = x;
  for (i = 0; i < 3 && l->b[i]; i++) bx += marca(l->b[i], bx, cy - 12.0f, 24.0f, focada ? 0.78f : 0.52f) + 14.0f;
  if (l->tag) {
    TxtLinha t = l->tagAc ? txtA(TXT_AJ_KBD, l->tag) : txt(TXT_AJ_KBD, l->tag);
    char up[64];
    idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(l->tag));
    if (l->tagAc) { int r, g, b; acento8(&r, &g, &b); txt_tracking(TXT_AJ_KBD, up, r, g, b, bx, cy - t.h * 0.5f, 1, 0.9f); }
    else txt_tracking(TXT_AJ_KBD, up, TX, bx, cy - t.h * 0.5f, 0.5f, 0.9f);
  }
  { TxtLinha ad = txt(TXT_ILHA_GENERO, l->ad);
    txt_desenhar_alpha(ad, x + w - ad.w, cy - ad.h * 0.5f + 2.0f, 0.42f); }
  return h;
}
static float grupoFonte(const char *nome, const char *sub, const char *qtd, float x, float y, float w) {
  TxtLinha n = txt(TXT_AJ_SECAO, nome), q = txt(TXT_ILHA_GENERO, qtd);
  float yc = y + 13.0f;
  txt_desenhar_alpha(n, x + 22, yc - n.h * 0.5f, 1);
  caps(TXT_AJ_CAPS13, sub, 1.82f, x + 22 + n.w + 12, yc - 6.0f, TX, 0.4f);
  txt_desenhar_alpha(q, x + w - 22 - q.w, yc - q.h * 0.5f, 0.38f);
  gfx_cor((GfxRect){ x, y + 36, w, 1 }, 0, 1, 1, 1, 0.08f);
  return 37.0f + 6.0f;
}
static float chipF(const char *rot, int on, float xDir, float y) {
  TxtLinha t = on ? txtA(TXT_AJ_SEG, rot) : txt(TXT_AJ_SEG, rot);
  float w = 22 + t.w + 22, ar, ag, ab;
  acento(&ar, &ag, &ab);
  if (on) gfx_cor((GfxRect){ xDir - w, y, w, 56 }, 0.5f, ar, ag, ab, 0.22f);
  else neutro((GfxRect){ xDir - w, y, w, 56 }, 28, 0.08f, SOL_CHIP, 1);
  txt_desenhar_alpha(t, xDir - w + 22, y + (56 - t.h) * 0.5f, on ? 1.0f : 0.85f);
  return w;
}
// Segmentado: trilho 6% (#1D1E23), o selecionado 14% (#34363E), contagem a 35%.
static float segmentado(const char *const *rot, const char *const *num, int n, int sel, float x, float y) {
  float w = 10.0f, xx;
  int i;
  for (i = 0; i < n; i++) w += (float)txt_largura(TXT_AJ_SEG, i18n(rot[i])) + 40.0f + 9.0f +
                             (float)txt_largura(TXT_AJ_16B, num[i]) + (i ? 4.0f : 0.0f);
  neutro((GfxRect){ x, y, w, 55 }, 27.5f, 0.06f, 0.114f, 0.118f, 0.137f, 1);
  xx = x + 5.0f;
  for (i = 0; i < n; i++) {
    TxtLinha t = txt(TXT_AJ_SEG, rot[i]), q = txt(TXT_AJ_16B, num[i]);
    float iw = t.w + 40.0f + 9.0f + q.w;
    if (i == sel) neutro((GfxRect){ xx, y + 5, iw, 45 }, 22.5f, 0.14f, 0.204f, 0.212f, 0.243f, 1);
    txt_desenhar_alpha(t, xx + 20, y + 27.5f - t.h * 0.5f, i == sel ? 1.0f : 0.55f);
    txt_desenhar_alpha(q, xx + 20 + t.w + 9, y + 27.5f - q.h * 0.5f + 2.0f, 0.35f);
    xx += iw + 4.0f;
  }
  return w;
}
static void cenaFontes(float t) {
  static const LinhaF L[5] = {
    { "Melhor para esta TV", 1, "9,4", { "v-dv", "a-ddp", "q-webdl" }, "MP4", 1, "AIOStreams" },
    { NULL, 0, "18,2", { "v-hdr10", "a-atmos", "q-remux" }, NULL, 0, "AIOStreams" },
    { NULL, 0, "7,8", { "v-hdr10plus", "a-ddp", NULL }, "Fora do cache", 0, "Torrentio" },
    { "Sua escolha anterior", 0, "2,1", { "a-ddp", "q-webdl", NULL }, "MP4", 1, "AIOStreams" },
    { NULL, 0, "1,6", { "a-ddp", "q-webdl", NULL }, "Dublado", 0, "Torrentio" },
  };
  static const char *const SEG[3] = { "Todos", "AIOStreams", "Torrentio" };
  static const char *const NUM[3] = { "6", "4", "2" };
  GfxRect f = { CW - 40.0f - 780.0f, 40.0f, 780.0f, CH - CR - 40.0f };
  float x, y, w, xd;
  (void)t;
  img("00.jpg", (GfxRect){ 0, 0, CW, CH }, 0, 0.30f, 1);
  gfx_cor((GfxRect){ 0, 0, CW, CH }, 0, 0, 0, 0, 0.30f);
  ajustes_ui_ilha(f, 36.0f, 0);
  // Cabecalho (.fh: padding 0 22 dentro do padding 26 da folha).
  x = f.x + 26.0f + 22.0f; y = f.y + 40.0f;
  caps(TXT_MINI, "Fallout · T1 E3", 2.1f, x, y, TX, 0.45f);
  { TxtLinha tt = txt(TXT_ILHA_TITULO, "Fontes");
    txt_desenhar_alpha(tt, x, y + 23.0f + (46.0f - tt.h) * 0.5f, 1); }
  xd = f.x + f.w - 26.0f - 22.0f;
  xd -= chipF("Dublado", 0, xd, y + 13.0f) + 10.0f;
  xd -= chipF("Em cache", 1, xd, y + 13.0f) + 10.0f;
  chipF("Só MP4", 0, xd, y + 13.0f);
  // Abas por addon.
  segmentado(SEG, NUM, 3, 0, f.x + 26.0f + 16.0f, y + 91.0f);
  // A lista.
  x = f.x + 26.0f; w = f.w - 52.0f; y = f.y + 208.0f;
  y += linhaFonte(&L[0], 1, x, y, w);
  y += 18.0f; y += grupoFonte("4K", "Ultra HD · HDR", "3 fontes", x, y, w);
  y += linhaFonte(&L[1], 0, x, y, w);
  y += linhaFonte(&L[2], 0, x, y, w);
  y += 18.0f; y += grupoFonte("1080p", "Full HD · SDR", "2 fontes", x, y, w);
  y += linhaFonte(&L[3], 0, x, y, w);
  linhaFonte(&L[4], 0, x, y, w);
}

// ====================================================== CENA: ILHA DO RELOGIO
// O pedido de amizade que a ilha mostra (ilhasinais.c: kicker, apelido, bio,
// ate tres generos, rosto, Aceitar / Recusar e a nota do rodape), com a home
// viva atras.
static float botaoCena(const char *rot, const char *ic, float x, float y, int focado) {
  TxtLinha t = focado ? txtT(TXT_ILHA_ITEM, rot) : txt(TXT_ILHA_ITEM, rot);
  float w = 28 + 22 + 12 + t.w + 28;
  GfxRect r = { x, y, w, 60 };
  if (focado) pilulaAcento(r, 1);
  else neutro(r, 30, 0.08f, SOL_CHIP, 1);
  if (focado) { float k = ajustes_tinta_foco() / 255.0f; gfx_icone((GfxRect){ x + 28, y + 19, 22, 22 }, ic, k, k, k, 1); }
  else icone(ic, (GfxRect){ x + 28, y + 19, 22, 22 }, 1, 0.88f);
  txt_desenhar_alpha(t, x + 28 + 34, y + (60 - t.h) * 0.5f, focado ? 1.0f : 0.88f);
  return w;
}
static void cenaIlha(float t) {
  const float H = CH - (CR - 40.0f);
  const float pw = floorf((CW - 112.0f - 3.0f * 18.0f) / 4.0f), ph = roundf(pw * 1.5f), y0 = H - 40.0f - ph;
  static const char *const PO[4] = { "poster/02.jpg", "poster/12.jpg", "poster/15.jpg", "poster/19.jpg" };
  GfxRect il = { 40, 40, CW - 80.0f, 306.0f };
  float ar, ag, ab, x, y;
  int i;
  (void)t;
  acento(&ar, &ag, &ab);
  img("03.jpg", (GfxRect){ 0, 0, CW, CH }, 0, 0.62f, 1);
  gfx_veu_css((GfxRect){ 0, 0, CW * 0.70f, CH }, 2, 1.6f, 1.0f, 0.60f);
  gfx_veu_css((GfxRect){ 0, CH * 0.55f, CW, CH * 0.45f }, 0, 1.0f, 1.0f, 0.60f);
  logo("logo/03.png", 56, y0 - 330.0f, 430.0f, 170.0f, 1);
  { static const char *const M[3] = { "2025", "2h 42min", "Drama" };
    float mx = 56.0f;
    for (i = 0; i < 3; i++) {
      TxtLinha l = txt(TXT_CAPTION, i == 2 ? i18n(M[i]) : M[i]);
      if (i) { gfx_cor((GfxRect){ mx + 12, y0 - 140.0f + 11.0f, 4, 4 }, 0.5f, 0.953f, 0.949f, 0.937f, 0.4f); mx += 28.0f; }
      txt_desenhar_alpha(l, mx, y0 - 140.0f + (26.0f - l.h) * 0.5f, 0.72f);
      mx += l.w;
    } }
  { TxtLinha l = txt(TXT_ILHA_NOME, "Continuar assistindo");
    txt_desenhar_alpha(l, 56, y0 - 50.0f + (29.0f - l.h) * 0.5f, 1); }
  for (i = 0; i < 4; i++) {
    GfxRect p = { 56.0f + i * (pw + 18.0f), y0, pw, ph };
    gfx_rect((GfxRect){ p.x - 12, p.y + 4, p.w + 24, p.h + 36 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, 0.35f);
    img(PO[i], p, 14.0f, 0.5f, 1);
  }
  // A ilha crescida no cartao do pedido.
  ajustes_ui_ilha(il, 34.0f, 0);
  if (vid()) gfx_luz_canto(il, 34.0f / il.h, il.w * 0.25f, -il.h * 0.9f, il.h * 1.3f, ar, ag, ab, 0.40f);
  { GfxRect av = { il.x + 32, il.y + 30, 132, 132 };
    TxtLinha b = txt_linha(TXT_G52B, "B", 255, 255, 255, 255);
    gfx_cor(av, 0.5f, 0.231f, 0.502f, 0.835f, 1);
    gfx_luz_canto(av, 0.5f, av.w * 0.15f, -av.h * 0.05f, av.w * 1.0f, 0.310f, 0.765f, 0.969f, 0.85f);
    txt_desenhar(b, av.x + (av.w - b.w) * 0.5f, av.y + (av.h - b.h) * 0.5f); }
  x = il.x + 32 + 132 + 28; y = il.y + 30;
  iconeA("aj_user-plus", (GfxRect){ x, y - 1, 20, 20 }, 1);
  { int r, g, b; acento8(&r, &g, &b); caps(TXT_MINI, "Pedido de amizade", 2.1f, x + 30, y, r, g, b, 1); }
  { TxtLinha n = txt(TXT_ILHA_TITULO, "Bruno");
    txt_desenhar_alpha(n, x, y + 28.0f, 1); }
  txt_bloco(TXT_CAPTION, i18n("Vejo de tudo, mas fico com ficção científica e série policial."), TX,
            x, y + 85.0f, il.x + il.w - 32 - x, 29.0f, 0.62f, 2);
  { static const char *const G[3] = { "Ficção científica", "Crime", "Drama" };
    float gx = x;
    for (i = 0; i < 3; i++) {
      TxtLinha l = txt(TXT_AJ_CHIP, G[i]);
      float w = 16 + l.w + 16;
      neutro((GfxRect){ gx, y + 128.0f, w, 36 }, 18, 0.08f, SOL_CHIP, 1);
      txt_desenhar_alpha(l, gx + 16, y + 128.0f + (36 - l.h) * 0.5f, 0.78f);
      gx += w + 10.0f;
    } }
  y = il.y + il.h - 26.0f - 60.0f;
  x = il.x + 32;
  x += botaoCena("Aceitar", "aj_check", x, y, 1) + 14.0f;
  botaoCena("Recusar", "aj_x", x, y, 0);
  { TxtLinha l = txt(TXT_ILHA_GENERO, "Recusar não avisa a pessoa.");
    txt_desenhar_alpha(l, il.x + il.w - 32 - l.w, y + (60 - l.h) * 0.5f, 0.45f); }
}

// ================================================================ CENA: AJUSTES
// Buscar nos ajustes com "memória" digitado, os dois resultados e o painel de
// pressao da memoria de imagens com o grafico de 2 min (o de Ajustes).
static void corPressao(float t, float *r, float *g, float *b) {
  if (t < 0.70f)      { *r = 0.298f; *g = 0.765f; *b = 0.541f; }
  else if (t < 0.90f) { *r = 0.910f; *g = 0.722f; *b = 0.290f; }
  else                { *r = 0.898f; *g = 0.325f; *b = 0.294f; }
}
static void linhaAj(const char *ic, const char *nm, const char *sb, const char *vl, int f, float x, float y, float w) {
  const float h = 86.0f;
  if (f) {
    if (vid()) gfx_cor((GfxRect){ x, y, w, h }, 22.0f / h, 1, 1, 1, 0.12f);
    else gfx_cor((GfxRect){ x, y, w, h }, 22.0f / h, 0.169f, 0.176f, 0.204f, 1);
  }
  icone(ic, (GfxRect){ x + 22, y + h * 0.5f - 13, 26, 26 }, 1, f ? 1.0f : 0.6f);
  { TxtLinha n = txt(TXT_AJ_NOME, nm), s = txt(TXT_ILHA_GENERO, sb), v = txt(TXT_AJ_VALOR, vl);
    float ty = y + (h - (n.h + 5 + s.h)) * 0.5f;
    txt_desenhar_alpha(n, x + 66, ty, f ? 1.0f : 0.86f);
    txt_desenhar_alpha(s, x + 66, ty + n.h + 5, 0.48f);
    txt_desenhar_alpha(v, x + w - 22 - v.w, y + (h - v.h) * 0.5f, f ? 0.92f : 0.6f); }
}
static void cenaAjustes(float t) {
  const float iw = CW - 80.0f, tela = 96.2f / 240.0f, uso = 151.4f / 240.0f;
  float pr, pg, pb, ar, ag, ab, x, y;
  GfxRect b = { 40, 40, iw, 76 }, res = { 40, 136, iw, 204 }, pm = { 40, 376, iw, 576 };
  acento(&ar, &ag, &ab);
  corPressao(uso, &pr, &pg, &pb);
  img("21.jpg", (GfxRect){ 0, 0, CW, CH }, 0, 0.5f, 1);
  gfx_cor((GfxRect){ 0, 0, CW, CH }, 0, 0, 0, 0, 0.40f);
  gfx_veu_css((GfxRect){ 0, 0, CW * 0.5f, CH }, 2, 1.0f, 1.0f, 0.22f);
  gfx_veu_css((GfxRect){ CW * 0.5f, 0, CW * 0.5f, CH }, 3, 1.0f, 1.0f, 0.10f);
  // A caixa de busca.
  ajustes_ui_ilha(b, 38.0f, 0);
  icone("aj_search", (GfxRect){ b.x + 28, b.y + 24, 28, 28 }, 1, 1);
  { char na[48];
    TxtLinha q, n;
    snprintf(na, sizeof na, i18n("%d ajustes"), 2);
    q = txt(TXT_CALLOUT, i18n("memória"));
    n = txt(TXT_ILHA_GENERO, na);
    txt_desenhar(q, b.x + 72, b.y + (76 - q.h) * 0.5f);
    if (fmodf(t, 1.0f) < 0.6f) gfx_cor((GfxRect){ b.x + 72 + q.w + 3, b.y + 23, 2, 30 }, 0, ar, ag, ab, 1);
    txt_desenhar_alpha(n, b.x + b.w - 28 - n.w, b.y + (76 - n.h) * 0.5f, 0.45f); }
  // Os dois resultados.
  ajustes_ui_ilha(res, 32.0f, 0);
  { char v[48];
    snprintf(v, sizeof v, i18n("%d de %d MB"), 151, 240);
    linhaAj("aj_memory-stick", "Memória usada por imagens", "Desempenho desta TV", v, 1,
            res.x + 14, res.y + 14, res.w - 28); }
  { char sb[120];
    snprintf(sb, sizeof sb, "%s · %s", i18n("Desempenho desta TV"), i18n("Avançado"));
    linhaAj("aj_sliders-horizontal", "Memória para imagens", sb, "Automático", 0,
            res.x + 14, res.y + 104, res.w - 28); }
  // O painel da memoria.
  ajustes_ui_ilha(pm, 32.0f, 0);
  x = pm.x + 26; y = pm.y + 26;
  caps(TXT_MINI, "Memória para imagens agora", 2.1f, x, y, TX, 0.45f);
  { char dm[48];
    TxtLinha n = txt(TXT_AJ_NUM64, ajustes_idioma_ingles() ? "151.4" : "151,4"), d;
    snprintf(dm, sizeof dm, i18n("de %d MB"), 240);
    d = txt(TXT_ILHA_CORPO, dm);
    char p[64];
    TxtLinha pc;
    snprintf(p, sizeof p, i18n("%d%% do teto"), 63);
    pc = txt_linha(TXT_AJ_CHIP, p, (int)(pr * 255), (int)(pg * 255), (int)(pb * 255), 255);
    float ny = y + 18.0f, base = y + 30.0f + 70.0f - 8.0f;
    txt_desenhar(n, x, ny);
    txt_desenhar_alpha(d, x + n.w + 14, base - d.h, 0.55f);
    txt_desenhar(pc, pm.x + pm.w - 26 - pc.w, base - pc.h - 2.0f);
    gfx_cor((GfxRect){ pm.x + pm.w - 26 - pc.w - 17, base - pc.h * 0.5f - 2.0f - 4.5f, 9, 9 }, 0.5f, pr, pg, pb, 1);
    y += 30.0f + 70.0f; }
  gfx_cor((GfxRect){ x, y, iw - 52, 10 }, 0.5f, 1, 1, 1, 0.08f);
  gfx_cor((GfxRect){ x, y, (iw - 52) * uso, 10 }, 0.5f, pr, pg, pb, 0.45f);
  gfx_cor((GfxRect){ x, y, (iw - 52) * tela, 10 }, 0.5f, pr, pg, pb, 1);
  y += 30.0f;
  { static const char *const LG[3] = { "Na tela", "No cache", "Livre" };
    float lx = x;
    int i;
    for (i = 0; i < 3; i++) {
      TxtLinha l = txt(TXT_ILHA_HORA, LG[i]);
      if (i == 2) gfx_cor((GfxRect){ lx, y + 5, 10, 10 }, 3.0f / 10.0f, 1, 1, 1, 0.12f);
      else gfx_cor((GfxRect){ lx, y + 5, 10, 10 }, 3.0f / 10.0f, pr, pg, pb, i ? 0.45f : 1.0f);
      txt_desenhar_alpha(l, lx + 17, y + (20 - l.h) * 0.5f, 0.5f);
      lx += 17 + l.w + 18.0f;
    } }
  y += 40.0f;
  ajustes_ui_grafico_exemplo(x, y, iw - 52, 180);
  y += 188.0f;
  { char ha[48]; snprintf(ha, sizeof ha, i18n("há %d min"), 2);
    TxtLinha a = txt(TXT_AJ_MINI13, ha), b2 = txt(TXT_AJ_MINI13, "Agora");
    txt_desenhar_alpha(a, x, y, 0.42f);
    txt_desenhar_alpha(b2, x + iw - 52 - b2.w, y, 0.42f); }
  y += 31.0f;
  { static const char *const K[3] = { "Na tela agora", "No cache", "Teto" };
    char v[3][96];
    int i;
    snprintf(v[0], sizeof v[0], i18n("%d imagens · %s MB"), 38, ajustes_idioma_ingles() ? "96.2" : "96,2");
    snprintf(v[1], sizeof v[1], i18n("%d imagens"), 64);
    snprintf(v[2], sizeof v[2], "240 MB · %s", i18n("pela RAM da TV"));
    for (i = 0; i < 3; i++) {
      TxtLinha k = txt(TXT_ILHA_GENERO, K[i]), l = txt(TXT_ILHA_GENERO, v[i]);
      gfx_cor((GfxRect){ x, y, iw - 52, 1 }, 0, 1, 1, 1, 0.07f);
      txt_desenhar_alpha(k, x, y + (43 - k.h) * 0.5f, 0.45f);
      txt_desenhar_alpha(l, x + iw - 52 - l.w, y + (43 - l.h) * 0.5f, 0.88f);
      y += 43.0f;
    } }
}

// ================================================================ as tabelas
enum { ID_NADA = 0, ID_VISUAL, ID_MENU, ID_CONTINUAR, ID_AVISOS, ID_FAIXAS, ID_FONTES, ID_TEXTO,
       ID_AJUSTES, ID_GUIA };
typedef struct {
  const char *nome;
  void (*desenhar)(float t);
  int id, id2;
  float luz[4];   // a luz do cartao enquanto a cena esta na tela (vidro)
} Cena;
static const Cena CENAS[N180_NCENAS] = {
  { "Fontes",          cenaFontes,  ID_FONTES,  ID_TEXTO, { 0.839f, 0.627f, 0.361f, 0.20f } },
  { "Ilha do relógio", cenaIlha,    ID_AVISOS,  ID_NADA,  { 0.886f, 0.424f, 0.329f, 0.18f } },
  { "Ajustes",         cenaAjustes, ID_AJUSTES, ID_GUIA,  { 0.471f, 0.627f, 1.000f, 0.16f } },
};

// A LISTA, em grupos. A frase pode ter um trecho em <b>...</b> (o caminho):
// ele sai em Medium e mais claro, como o mockup.
typedef struct { int id; const char *grupo, *icone, *nome, *linha; } Item;
static const Item ITENS[] = {
  { ID_VISUAL,    "Visual",          "aj_panel-top",          "Visual novo",
    "Todo painel virou uma ilha, como o relógio. Vidro ou sólido em <b>Ajustes › Aparência</b>." },
  { ID_MENU,      "Home",            "aj_panel-left",         "Menu lateral",
    "Na Moderna e na Padrão, a barra vai de cima a baixo da tela." },
  { ID_CONTINUAR, NULL,              "aj_list-x",             "Tirar de Continuar",
    "Agora pede confirmação, porque apaga o ponto onde você parou." },
  { ID_AVISOS,    "Ilha do relógio", "aj_bell",               "Avisos no relógio",
    "Pedido de amizade, episódio novo, rede e addon fora do ar aparecem ali." },
  { ID_FAIXAS,    NULL,              "aj_audio-lines",        "Áudio e Legendas",
    "No player, abrem da ilha do canto, sem cobrir o filme." },
  { ID_FONTES,    "Fontes",          "aj_layers",             "Folha de Fontes",
    "Melhor para esta TV primeiro, grupos por qualidade e filtros Só MP4, Em cache e Dublado." },
  { ID_TEXTO,     NULL,              "aj_type",               "Texto das fontes",
    "Do Nuvio ou do jeito que o addon manda. Em <b>Ajustes › Reprodução</b>." },
  { ID_AJUSTES,   "Ajustes",         "aj_sliders-horizontal", "Ajustes",
    "Busca, prévia de verdade em cada opção, gráfico de memória e Fileiras da Home." },
  { ID_GUIA,      NULL,              "aj_book-open",          "Guia de uso",
    "Tudo o que o Nuvio faz, explicado num lugar só. Em <b>Ajustes › Sobre e ajuda</b>." },
};
#define N180_NI ((int)(sizeof ITENS / sizeof *ITENS))

// Frase com um trecho em <b>: desenha (ou so mede, com a < 0) e devolve a
// largura. Corta com "…" no fim se passar de `maxW`.
static float espaco(void) {
  return (float)(txt_largura(TXT_AJ_SUB, "a a") - txt_largura(TXT_AJ_SUB, "aa"));
}
static float frase(const char *s, float x, float y, float maxW, float a) {
  char pre[400], neg[200], pos[200];
  const char *b = strstr(s, "<b>"), *e = b ? strstr(b, "</b>") : NULL;
  float w = 0;
  TxtLinha l;
  if (!b || !e) {
    l = txtC(TXT_AJ_SUB, s, maxW);
    if (a >= 0) txt_desenhar_alpha(l, x, y, 0.55f * a);
    return (float)l.w;
  }
  snprintf(pre, sizeof pre, "%.*s", (int)(b - s), s);
  { size_t n = strlen(pre); int sp = n && pre[n - 1] == ' ';
    while (n && pre[n - 1] == ' ') pre[--n] = 0;
    l = txtC(TXT_AJ_SUB, pre, maxW);
    if (a >= 0) txt_desenhar_alpha(l, x, y, 0.55f * a);
    w = (float)l.w + (sp ? espaco() : 0.0f); }
  snprintf(neg, sizeof neg, "%.*s", (int)(e - b - 3), b + 3);
  snprintf(pos, sizeof pos, "%s", e + 4);
  if (w < maxW - 20) {
    TxtLinha n = txtC(TXT_G20M, neg, maxW - w);
    if (a >= 0) txt_desenhar_alpha(n, x + w, y + (l.h - n.h) * 0.5f, 0.72f * a);
    w += (float)n.w;
  }
  if (pos[0] && w < maxW - 10) {
    TxtLinha p = txtC(TXT_AJ_SUB, pos, maxW - w);
    if (a >= 0) txt_desenhar_alpha(p, x + w, y, 0.55f * a);
    w += (float)p.w;
  }
  return w;
}

// ------------------------------------------------------------------- estado
int novidades180_itens(void) { return N180_NI; }
int novidades180_item_largura(int i, int *limite, const char **nome) {
  if (i < 0 || i >= N180_NI) return 0;
  if (limite) *limite = (int)(COL_W - 64.0f);
  if (nome) *nome = ITENS[i].nome;
  return (int)frase(i18n(ITENS[i].linha), 0, 0, 100000.0f, -1.0f);
}
int novidades180_cenas(void) { return N180_NCENAS; }
int novidades180_cena(void) { return cena; }
int novidades180_aberto(void) { return aberto; }
int novidades180_boas_vindas(void) { return boasVindas; }
int novidades180_foco(void) { return foco; }
int novidades180_foco_na_previa(void) { return naPrevia; }
void novidades180_dir(const char *d) { if (d && d[0]) snprintf(dirArte, sizeof dirArte, "%s", d); }
int novidades180_pedido(void) { int p = pedido; pedido = N180_PEDIU_NADA; return p; }

void novidades180_cena_desenhar(int c, float t) {
  if (c < 0 || c >= N180_NCENAS) return;
  CENAS[c].desenhar(t);
}

static void mudarCena(int nova) {
  nova = (nova % N180_NCENAS + N180_NCENAS) % N180_NCENAS;
  if (nova == cena) return;
  cenaAntiga = cena;
  cena = nova;
  transicao = ajustes_animacoes_reduzidas() ? 1.0f : 0.0f;
  relogioCena = 0.0f;
}
void novidades180_ir(int c, float t) {
  cena = cenaAntiga = (c % N180_NCENAS + N180_NCENAS) % N180_NCENAS;
  transicao = 1.0f;
  relogioCena = t;
}

static void comecar(int bv, float e) {
  aberto = decidido = 1;
  boasVindas = bv;
  foco = bv ? B_GUIA : B_VIDRO;
  naPrevia = 0;
  cena = cenaAntiga = 0;
  entrada = e;
  transicao = 1.0f;
  relogioCena = 0.0f;
  textogate_reiniciar(&gateLista);
}
void novidades180_abrir(int bv) { comecar(bv, 1.0f); }
void novidades180_reabrir(void) { comecar(boasVindas, 0.0f); foco = B_GUIA; }

void novidades180_primeira_vez(void) {
  int f;
  if (decidido) return;
  decidido = 1;
  f = novidadesfila_preparar();
  if (f == NF_NADA) return;
  comecar(f == NF_NOVA, 0.0f);
}

static void fechar(int oQue) {
  aberto = 0;
  naPrevia = 0;
  dados_gravar(NF_ARQ_180, "1\n");
  pedido = oQue;
}

// D-PAD. Duas fileiras de foco: as pilulas (de fabrica, na principal; na
// instalacao nova, no guia) e o seletor da previa (cima). Na previa,
// esquerda/direita trocam a cena e a troca automatica para.
void novidades180_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return;
  k = e->key.keysym.sym;
  if (k == SDLK_UP)   { naPrevia = 1; return; }
  if (k == SDLK_DOWN) { naPrevia = 0; return; }
  if (k == SDLK_LEFT) {
    if (naPrevia) mudarCena(cena - 1);
    else if (foco > 0) foco--;
    return;
  }
  if (k == SDLK_RIGHT) {
    if (naPrevia) mudarCena(cena + 1);
    else if (foco < B_N - 1) foco++;
    return;
  }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
    if (naPrevia) { mudarCena(cena + 1); return; }
    fechar(foco == B_GUIA ? N180_PEDIU_GUIA : foco == B_VIDRO ? N180_PEDIU_VIDRO : N180_PEDIU_NADA);
    return;
  }
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE ||
      k == SDLK_DELETE || e->key.keysym.scancode == NV_SCANCODE_BACK)
    fechar(N180_PEDIU_NADA);
}

void novidades180_atualizar(float dt, Uint32 agora) {
  (void)agora;
  if (!aberto && entrada < 0.002f) {
    entrada = 0.0f;
    if (mini.fbo) gfx_mini_liberar(&mini);   // 2,5 MB de textura so enquanto o cartao existe
    return;
  }
  if (aberto) pedirArtes();
  if (ajustes_animacoes_reduzidas()) {
    entrada = aberto ? 1.0f : 0.0f;
    transicao = 1.0f;
  } else {
    entrada = anim_rampa(entrada, aberto ? 1.0f : 0.0f, dt, aberto ? N180_ABRIR_MS : N180_FECHAR_MS);
    if (transicao < 1.0f) { transicao += dt / N180_TROCA_S; if (transicao > 1.0f) transicao = 1.0f; }
  }
  if (aberto) {
    relogioCena += dt;
    if (!naPrevia && relogioCena >= N180_CENA_S) mudarCena(cena + 1);
  }
}

// ------------------------------------------------------------------ desenho
// A previa: as cenas na miniatura (a que sai por baixo, a que entra por cima
// com o alfa da passagem) e a tela reduzida no cartao.
static void previa(float a) {
  GfxRect pv = { PV_X, PV_Y + (1.0f - a) * 34.0f, PV_W, PV_H };
  float s = anim_suave(transicao);
  if (gfx_mini_alvo(&mini, (int)PV_W, (int)PV_H)) {
    gfx_mini_comecar(&mini, 0, 0, PV_ESC);
    gfx_cor((GfxRect){ 0, 0, CW, CH }, 0, 0.051f, 0.055f, 0.067f, 1);
    if (s < 1.0f && cenaAntiga != cena) CENAS[cenaAntiga].desenhar(relogioCena + N180_CENA_S);
    gfx_opacidade_grupo = cenaAntiga != cena ? s : 1.0f;
    CENAS[cena].desenhar(relogioCena);
    gfx_opacidade_grupo = 1.0f;
    gfx_mini_terminar();
    gfx_mini_desenhar(&mini, pv, PV_RAIO, a);
  } else {
    gfx_cor(pv, PV_RAIO / pv.h, 0.051f, 0.055f, 0.067f, a);
  }
}

// O seletor de cenas (.pv-seg): ilha pilula embaixo da previa, a cena atual
// selecionada e o fio de progresso de cada uma. Com o foco nele, a atual vira
// a pilula cheia no acento.
static GfxRect segItem[N180_NCENAS];
static void seletor(float yOff, float a) {
  float w = 12.0f, x, y = PV_Y + PV_H - 22.0f - 52.0f + yOff, ar, ag, ab;
  int i, tinta = ajustes_tinta_foco();
  acento(&ar, &ag, &ab);
  for (i = 0; i < N180_NCENAS; i++) w += (float)txt_largura(TXT_AJ_CHIP, i18n(CENAS[i].nome)) + 36.0f + (i ? 4.0f : 0.0f);
  x = PV_X + (PV_W - w) * 0.5f;
  { GfxRect r = { x, y, w, 52 };
    gfx_opacidade_grupo = a;
    ajustes_ui_ilha(r, 26.0f, 0);
    gfx_opacidade_grupo = 1.0f; }
  x += 6.0f;
  for (i = 0; i < N180_NCENAS; i++) {
    int sel = i == cena, ac = sel && naPrevia;
    TxtLinha t = ac ? txt_linha(TXT_AJ_CHIP, CENAS[i].nome, tinta, tinta, tinta, 255) : txt(TXT_AJ_CHIP, CENAS[i].nome);
    GfxRect r = { x, y + 6, t.w + 36.0f, 40 };
    float p = sel ? (naPrevia ? 1.0f : anim_clamp(relogioCena / N180_CENA_S, 0, 1)) : i < cena ? 1.0f : 0.0f;
    float k = tinta / 255.0f;
    GfxRect pr = { r.x + 18, r.y + 33, r.w - 36, 2 };
    if (ac) pilulaAcento(r, a);
    else if (sel) neutro(r, 20, 0.14f, 0.204f, 0.212f, 0.243f, a);
    txt_desenhar_alpha(t, r.x + 18, r.y + (40 - t.h) * 0.5f - 2.0f, (sel ? 1.0f : 0.55f) * a);
    if (ac) {
      gfx_cor(pr, 0.5f, k, k, k, 0.25f * a);
      if (p * pr.w >= 1) gfx_cor((GfxRect){ pr.x, pr.y, pr.w * p, 2 }, 0, k, k, k, a);
    } else {
      gfx_cor(pr, 0.5f, 1, 1, 1, 0.18f * a);
      if (p * pr.w >= 1) gfx_cor((GfxRect){ pr.x, pr.y, pr.w * p, 2 }, 0, 1, 1, 1, 0.85f * a);
    }
    segItem[i] = r;
    x += r.w + 4.0f;
  }
}

// Uma linha da lista: o disco (no acento enquanto a cena dela esta na previa),
// o nome e a frase.
static void item(int i, float y, float vivo, float a) {
  float ar, ag, ab;
  GfxRect d = { COL_X, y + 8.8f, 44, 44 };
  acento(&ar, &ag, &ab);
  if (vid()) gfx_cor(d, 0.5f, 1, 1, 1, 0.07f * (1.0f - vivo) * a);
  else gfx_cor(d, 0.5f, 0.125f, 0.129f, 0.153f, a);
  if (vivo > 0.01f) {
    if (vid()) gfx_cor(d, 0.5f, ar, ag, ab, 0.22f * vivo * a);
    else gfx_cor(d, 0.5f, ar * 0.2f + 0.125f * 0.8f, ag * 0.2f + 0.129f * 0.8f, ab * 0.2f + 0.153f * 0.8f, vivo * a);
  }
  { GfxRect ic = { d.x + 11, d.y + 11, 22, 22 };
    icone(ITENS[i].icone, ic, 1, 0.82f * (1.0f - vivo) * a);
    if (vivo > 0.01f) iconeA(ITENS[i].icone, ic, vivo * a); }
  { TxtLinha n = txt_linha_corta(TXT_AJ_SECAO, ITENS[i].nome, 246, 245, 242, 255, COL_W - 64.0f);
    txt_desenhar_alpha(n, COL_X + 64, y + 4 + (26.4f - n.h) * 0.5f, a); }
  frase(i18n(ITENS[i].linha), COL_X + 64, y + 33.4f + (24.3f - 23.0f) * 0.5f, COL_W - 64.0f, a);
}

// Pilula do rodape (.btn 60): neutra, a principal no acento a 20% e a focada
// cheia no acento.
static float pilula(const char *rot, const char *ic, int pri, int f, float xDir, float y, float a) {
  float ar, ag, ab, w;
  TxtLinha t;
  int r8, g8, b8;
  acento(&ar, &ag, &ab);
  acento8(&r8, &g8, &b8);
  t = f ? txtT(TXT_ILHA_ITEM, rot) : pri ? txt_linha(TXT_ILHA_ITEM, rot, r8, g8, b8, 255) : txt(TXT_ILHA_ITEM, rot);
  w = 28 + (ic ? 22 + 12 : 0) + t.w + 28;
  { GfxRect r = { xDir - w, y, w, 60 };
    if (f) pilulaAcento(r, a);
    else if (pri) gfx_cor(r, 0.5f, ar, ag, ab, 0.20f * a);
    else neutro(r, 30, 0.08f, SOL_CHIP, a);
    if (ic) {
      GfxRect ir = { r.x + 28, y + 19, 22, 22 };
      if (f) { float k = ajustes_tinta_foco() / 255.0f; gfx_icone(ir, ic, k, k, k, a); }
      else if (pri) iconeA(ic, ir, a);
      else icone(ic, ir, 1, 0.88f * a);
    }
    txt_desenhar_alpha(t, r.x + 28 + (ic ? 34 : 0), y + (60 - t.h) * 0.5f, (f || pri ? 1.0f : 0.88f) * a); }
  return w;
}
// Largura da fileira de dicas, sem desenhar: em lingua longa (ru, de) as tres
// pilulas avancam sobre a dica, e ai a dica sai em vez de ficar por baixo.
static float dicasLargura(const char *const *k, const char *const *l, int n) {
  float w = 0.0f;
  int i;
  for (i = 0; i < n; i++) {
    float kw = (float)txt_largura(TXT_AJ_KBD, i18n(k[i])) + 18.0f;
    w += (kw < 34.0f ? 34.0f : kw) + 9 + (float)txt_largura(TXT_ILHA_GENERO, i18n(l[i])) + 18.0f;
  }
  return w - 18.0f;
}
static float dicas(const char *const *k, const char *const *l, int n, float x, float y, float a) {
  int i;
  for (i = 0; i < n; i++) {
    TxtLinha kt = txt(TXT_AJ_KBD, k[i]), lt = txt(TXT_ILHA_GENERO, l[i]);
    float kw = kt.w + 18.0f < 34.0f ? 34.0f : kt.w + 18.0f;
    neutro((GfxRect){ x, y, kw, 30 }, 15, 0.09f, SOL_CHIP, a);
    txt_desenhar_alpha(kt, x + (kw - kt.w) * 0.5f, y + (30 - kt.h) * 0.5f, 0.82f * a);
    txt_desenhar_alpha(lt, x + kw + 9, y + (30 - lt.h) * 0.5f, 0.5f * a);
    x += kw + 9 + lt.w + 18.0f;
  }
  return x;
}

static void ponteiroFoco(int b, int nada) { (void)nada; foco = b; naPrevia = 0; }
static void ponteiroOk(int b, int nada) {
  (void)nada; foco = b; naPrevia = 0;
  fechar(b == B_GUIA ? N180_PEDIU_GUIA : b == B_VIDRO ? N180_PEDIU_VIDRO : N180_PEDIU_NADA);
}
static void ponteiroCena(int c, int nada) { (void)nada; naPrevia = 1; mudarCena(c); }

void novidades180_desenhar(Uint32 agora) {
  float a = anim_suave(entrada), dy = (1.0f - a) * 34.0f, s = anim_suave(transicao);
  GfxRect card = { N180_X, N180_Y + dy, N180_W, N180_H };
  (void)agora;
  if (entrada < 0.002f) return;
  if (aberto) ponteiro_camada();
  // Veu da tela: 30% no vidro, 42% no solido (DESIGN.md §2).
  gfx_cor((GfxRect){ 0, 0, NV_TELA_W, NV_TELA_H }, 0, 0, 0, 0, (vid() ? 0.30f : 0.42f) * a);
  // O cartao: sombra larga, miolo a 94% (solido #121316) e a luz do canto, que
  // no vidro pega a cor da cena da previa.
  gfx_rect((GfxRect){ card.x - 46, card.y - 10, card.w + 92, card.h + 120 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f,
           0, 0, 0, 0.45f * a);
  if (vid()) {
    const float *l0 = CENAS[cenaAntiga].luz, *l1 = CENAS[cena].luz;
    float k = cenaAntiga != cena ? s : 1.0f;
    gfx_vidro_ou_solido(card, N180_RAIO / card.h, GFX_VIDRO_DENS_MODAL, a, 0.055f, 0.059f, 0.071f, 0.94f);
    gfx_luz_canto(card, N180_RAIO / card.h, card.w * 0.20f, -card.h * 0.20f, card.h * 0.62f,
                  anim_mistura(l0[0], l1[0], k), anim_mistura(l0[1], l1[1], k), anim_mistura(l0[2], l1[2], k),
                  anim_mistura(l0[3], l1[3], k) * 2.2f * a);
  } else {
    gfx_cor(card, N180_RAIO / card.h, 0.071f, 0.075f, 0.086f, a);
    gfx_luz_canto(card, N180_RAIO / card.h, card.w * 0.20f, -card.h * 0.20f, card.h * 0.62f, 1, 1, 1, 0.045f * 2.2f * a);
  }

  previa(a);
  seletor(dy, a);
  if (aberto) {
    int i;
    for (i = 0; i < N180_NCENAS; i++)
      ponteiro_alvo(segItem[i].x, segItem[i].y, segItem[i].w, segItem[i].h, ponteiroCena, ponteiroCena, i, 0);
  }

  // ----- a coluna: kicker, titulo, frase e a lista em grupos.
  { char kick[64], tit[96];
    int pend0 = txt_pendentes, i;
    float fechado = textogate_aberto(&gateLista) ? 0.0f : 1.0f;
    float ta = fechado > 0.0f ? NV_TXTGATE_AQUECER : a * textogate_passo(&gateLista, 0, SDL_GetTicks());
    float y = COL_Y + dy;
    if (boasVindas) {
      snprintf(kick, sizeof kick, "Nuvio %s", N180_VERSAO);
      snprintf(tit, sizeof tit, "%s", i18n("Boas-vindas ao Nuvio"));
    } else {
      snprintf(kick, sizeof kick, "%s", i18n("Glass UI"));
      snprintf(tit, sizeof tit, i18n("Novidades da %s"), N180_VERSAO);
    }
    caps(TXT_MINI, kick, 2.1f, COL_X, y + 2.0f, TX, 0.45f * ta);
    { TxtLinha t = txtC(TXT_NOV_TITULO, tit, COL_W);
      txt_desenhar_alpha(t, COL_X, y + 29.0f + (54.0f - t.h) * 0.5f, ta); }
    { TxtLinha t = txtC(TXT_CAPTION, boasVindas
          ? "Aqui está o que esta versão trouxe. O resto do app está explicado no Guia de uso."
          : "O app inteiro no material da ilha do relógio, em vidro ou sólido.", COL_W);
      txt_desenhar_alpha(t, COL_X, y + 95.0f + (29.0f - t.h) * 0.5f, 0.62f * ta); }
    y += 146.0f;
    for (i = 0; i < N180_NI; i++) {
      float vivo = 0.0f;
      if (ITENS[i].grupo) {
        if (i) y += 14.0f;
        caps(TXT_AJ_CAPS13, ITENS[i].grupo, 2.08f, COL_X + 64, y + 1.0f, TX, 0.38f * ta);
        y += 18.0f;
      }
      if (ITENS[i].id == CENAS[cena].id || ITENS[i].id == CENAS[cena].id2)
        vivo += cenaAntiga != cena ? s : 1.0f;
      if (cenaAntiga != cena && (ITENS[i].id == CENAS[cenaAntiga].id || ITENS[i].id == CENAS[cenaAntiga].id2))
        vivo += 1.0f - s;
      item(i, y, vivo > 1 ? 1 : vivo, ta);
      y += 61.7f;
    }
    if (fechado > 0.0f) textogate_passo(&gateLista, txt_pendentes - pend0, SDL_GetTicks()); }

  // ----- o rodape: a dica a esquerda, as tres pilulas a direita.
  { float y = N180_Y + dy + 896.0f, xd = COL_X + COL_W;
    GfxRect r[B_N];
    static const char *const ROT[B_N] = { "Agora não", "Abrir o guia", "Vidro ou sólido" };
    static const char *const IC[B_N] = { NULL, "aj_book-open", "aj_palette" };
    static const char *const K2[2] = { "← →", "↓" }, *const L2[2] = { "Trocar", "Botões" };
    static const char *const K1[1] = { "↑" }, *const L1[1] = { "Trocar a prévia" };
    int i;
    for (i = B_N - 1; i >= 0; i--) {
      float w = pilula(ROT[i], IC[i], i == B_VIDRO, !naPrevia && foco == i, xd, y, a);
      r[i] = (GfxRect){ xd - w, y, w, 60 };
      xd -= w + 12.0f;
    }
    if (COL_X + dicasLargura(naPrevia ? K2 : K1, naPrevia ? L2 : L1, naPrevia ? 2 : 1) <= xd - 12.0f)
      dicas(naPrevia ? K2 : K1, naPrevia ? L2 : L1, naPrevia ? 2 : 1, COL_X, y + 15.0f, a);
    if (aberto)
      for (i = 0; i < B_N; i++) ponteiro_alvo(r[i].x, r[i].y, r[i].w, r[i].h, ponteiroFoco, ponteiroOk, i, 0);
  }
}
