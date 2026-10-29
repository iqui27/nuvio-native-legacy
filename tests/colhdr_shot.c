// MOCK DO CABECALHO DAS COLECOES (job 205-colhdr). Pedido do dono: "o header
// das colecoes e a barra de progresso precisa de um rethink, ta tudo meio cru".
// Tres direcoes para o bloco identidade + progresso + troca de exibicao, como
// UMA peca alinhada a ilha do relogio (y 36, borda direita a 64 px):
//   A  editorial quieto: tipo grande, anel de pontos, segmentado de vidro.
//   B  ilha de vidro: o cabecalho e uma ilha no material do relogio, com o
//      progresso dentro; a troca de exibicao encosta no relogio.
//   C  pela arte: retrato / pilha de capas, progresso em capinhas sob a arte.
// Cada uma em zero / parcial / completo / foco no seletor. So desenho com as
// primitivas do app (gfx_vidro_painel, gfx_vidro_pilula_cheia, gfx_luz_canto,
// fontes e icones embarcados); NAO toca src/vertudo.c.
//
// NAO ENTRA NA SUITE (*_mock.sh fora do testa-tudo): precisa de janela GL.
#include "ilha.h"
#include "ajustes.h"
#include "corviva.h"
#include "fundo.h"
#include "gfx.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X0 104.0f                   // ajustes_conteudo_x() da TV
#define DIR (1920.0f - NV_ILHA_MARGEM_D)

typedef struct {
  const char *eyebrow, *nome, *meta, *arte, *foto, *capas[3];
  int pessoa, vistos, total, lay;
} Caso;

static const char *LAY[] = { "Grid", "Timeline", "Cover Flow", "Ranking", "Showcase" };
static const char *LAY_ICO[] = { "aj_layout-dashboard", "aj_rows-3", "aj_film", "aj_list-ordered",
                                 "aj_monitor-play" };
#define NLAY 5

static SDL_Window *win;
static float ilhaX, ilhaY, ilhaW, ilhaH;

typedef struct { const char *cam; GLuint t; float asp; } Tex;
static Tex texs[32];
static int nTex;
// `quad`: recorte quadrado do alto (rosto) para o retrato redondo.
static GLuint carregar(const char *cam, float *asp, int quad) {
  SDL_Surface *s, *t;
  int i;
  for (i = 0; i < nTex; i++)
    if (!strcmp(texs[i].cam, cam)) { *asp = texs[i].asp; return texs[i].t; }
  s = IMG_Load(cam);
  assert(s);
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  assert(t && nTex < 32);
  if (quad && t->h > t->w) {
    SDL_Surface *q = SDL_CreateRGBSurfaceWithFormat(0, t->w, t->w, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_Rect o = { 0, (t->h - t->w) / 5, t->w, t->w };
    SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(t, &o, q, NULL);
    SDL_FreeSurface(t);
    t = q;
  }
  texs[nTex].cam = strdup(cam);
  texs[nTex].asp = (float)t->w / t->h;
  glGenTextures(1, &texs[nTex].t);
  glBindTexture(GL_TEXTURE_2D, texs[nTex].t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
  *asp = texs[nTex].asp;
  return texs[nTex++].t;
}
// Arte recortada (nunca esticada) com cantos de `raio` (fracao do menor lado).
static void arte(GfxRect r, const char *cam, float raio, float a) {
  float asp;
  GLuint t = carregar(cam, &asp, r.w == r.h);
  gfx_tex_aspect_atual = asp;
  gfx_rect(r, t, GFX_CARD, 0, 0, 0, raio, 1, 1, 1, a);
  gfx_tex_aspect_atual = 0;
}
static void sombra(GfxRect r, float a) {
  gfx_rect((GfxRect){ r.x - 16, r.y - 6, r.w + 32, r.h + 34 }, 0, GFX_SOMBRA, 1, 0, 0, 0.5f, 0, 0, 0, a);
}
// Uma ilha no material do relogio: sombra curta, vidro, luz fraca no canto.
static void ilhaVidro(GfxRect r, float raioPx) {
  float raio = raioPx / r.h;
  sombra(r, 0.34f);
  gfx_vidro_painel(r, raio, 0.80f, 1);
  gfx_luz_canto(r, raio, r.w * 0.25f, -r.h * 0.9f, r.w * 0.85f, 1, 1, 1, 0.07f);
}
static TxtLinha linha(TxtEstilo e, const char *s, int c, int a) { return txt_linha(e, s, c, c, c, a); }
static void acento(float *r, float *g, float *b) { ajustes_acento(r, g, b); }

// O fundo da colecao como o app pinta (fundo_desenhar_modo), e a grade logo
// abaixo do cabecalho (VT_TOPO = 332), para o bloco ser julgado em contexto.
static void cena(const Caso *c) {
  int i;
  fundo_desenhar_modo(FUNDO_ARTE, (GfxRect){ 0, 0, 1920, 1080 }, 0.0f, c->arte, 1.0f);
  gfx_rect((GfxRect){ 0, 0, 1920, 360 }, 0, GFX_VEU_TOPO, 0, 0, 0, 0, 0, 0, 0, 0.55f);
  for (i = 0; i < 7; i++) {
    char cam[64];
    GfxRect p = { X0 + i * 248.0f, 332.0f, 220, 330 };
    snprintf(cam, sizeof cam, "deploy/app/art/poster/%02d.jpg", (i * 5 + c->total) % 40);
    arte(p, cam, NV_RAIO_CARD, i < c->vistos ? 1.0f : 0.92f);
  }
}

// Texto da contagem, a mesma regra nas tres direcoes: zero convida, nunca
// mostra barra vazia; completo comemora em uma palavra.
static void contagem(const Caso *c, char *forte, size_t nf, char *apoio, size_t na) {
  const char *tipo = c->pessoa ? "films" : "titles";
  if (!c->vistos) { snprintf(forte, nf, "%d %s", c->total, tipo); snprintf(apoio, na, "Start anywhere"); }
  else if (c->vistos < c->total) { snprintf(forte, nf, "%d of %d watched", c->vistos, c->total);
                                   snprintf(apoio, na, "%d to go", c->total - c->vistos); }
  else { snprintf(forte, nf, "All %d watched", c->total); snprintf(apoio, na, "Complete"); }
}

// Titulo em ate 2 linhas; devolve a altura usada.
static float titulo(TxtEstilo e, const char *s, float x, float y, float larg, float lead) {
  return txt_bloco_corta(e, s, 242, 243, 247, x, y, larg, lead, 1.0f, 2);
}
// Nome grande: 76 numa linha; se nao cabe, 56 em ate duas (a altura do bloco
// fica quase a mesma e a grade continua em VT_TOPO).
static float nomeGrande(const char *s, float x, float y, float larg) {
  if (linha(TXT_TITULO1, s, 242, 255).w <= larg) return titulo(TXT_TITULO1, s, x, y, larg, 82);
  return titulo(TXT_TITULO2, s, x, y + 4, larg, 62) + 4;
}

// Dica do seletor (so no foco): setas + verbo, discreta, alinhada a `xDir`.
static void dica(float xDir, float y, float a) {
  TxtLinha t = linha(TXT_ILHA_SUB, "switch layout", 225, 255);
  float x = xDir - t.w;
  txt_desenhar_alpha(t, x, y, 0.72f * a);
  gfx_icone((GfxRect){ x - 22, y + (t.h - 18) * 0.5f, 18, 18 }, "pl_chevron-right", 1, 1, 1, 0.72f * a);
  gfx_icone((GfxRect){ x - 40, y + (t.h - 18) * 0.5f, 18, 18 }, "pl_chevron-left", 1, 1, 1, 0.72f * a);
}

// --- A: EDITORIAL QUIETO ------------------------------------------------------
static void anelPontos(const Caso *c, float cx, float cy, float R) {
  float ar, ag, ab, s = c->total > 24 ? 5.0f : 7.0f;
  int i, todo = c->vistos >= c->total;
  acento(&ar, &ag, &ab);
  if (!c->vistos) {   // zero: nada de anel apagado, so a ficha com o total
    char n[8]; snprintf(n, sizeof n, "%d", c->total);
    TxtLinha t = linha(TXT_ILHA_NOME, n, 242, 255);
    gfx_cor((GfxRect){ cx - R - 4, cy - R - 4, (R + 4) * 2, (R + 4) * 2 }, 0.5f, 1, 1, 1, 0.10f);
    gfx_anel((GfxRect){ cx - R - 4, cy - R - 4, (R + 4) * 2, (R + 4) * 2 }, 0.5f, 1.5f, 1, 1, 1, 0.16f);
    txt_desenhar(t, cx - t.w * 0.5f, cy - t.h * 0.5f);
    return;
  }
  if (todo) gfx_rect((GfxRect){ cx - R * 2.2f, cy - R * 2.2f, R * 4.4f, R * 4.4f }, 0, GFX_SOMBRA, 1, 0, 0,
                     0.5f, ar, ag, ab, 0.22f);
  for (i = 0; i < c->total; i++) {
    float t = -1.5707963f + 6.2831853f * i / c->total;
    GfxRect d = { cx + R * cosf(t) - s * 0.5f, cy + R * sinf(t) - s * 0.5f, s, s };
    if (todo) gfx_cor(d, 0.5f, ar, ag, ab, 1);
    else gfx_cor(d, 0.5f, 1, 1, 1, i < c->vistos ? 0.96f : 0.20f);
  }
  if (todo) gfx_icone((GfxRect){ cx - 14, cy - 14, 28, 28 }, "aj_check", ar, ag, ab, 1);
  else { char n[8]; snprintf(n, sizeof n, "%d", c->vistos);
         TxtLinha t = linha(TXT_ILHA_NOME, n, 242, 255);
         txt_desenhar(t, cx - t.w * 0.5f, cy - t.h * 0.5f); }
}
static void segmentado(const Caso *c, float xDir, float cy, float foco) {
  const float H = NV_CTRL_H, S = 56.0f, P = 4.0f;
  TxtLinha nome = linha(TXT_AJ_SEG, LAY[c->lay], gfx_vidro_tinta(foco), 255);
  float selW = 50.0f + nome.w + 20.0f, w = P * 2 + S * (NLAY - 1) + selW, x = xDir - w, sx;
  GfxRect trilho = { x, cy - H * 0.5f, w, H };
  int i;
  sombra(trilho, 0.30f);
  gfx_vidro_painel(trilho, 0.5f, 0.62f, 1);
  sx = x + P;
  for (i = 0; i < NLAY; i++) {
    int sel = i == c->lay;
    float sw = sel ? selW : S;
    GfxRect s = { sx, trilho.y + P, sw, H - P * 2 };
    float k = sel ? 1.0f : 0.58f, ic = sel && foco > 0.5f ? gfx_vidro_tinta(foco) / 255.0f : 1.0f;
    if (sel) {
      if (foco > 0) gfx_vidro_pilula_cheia(s, 0.5f, foco, 1);
      else gfx_cor(s, 0.5f, 1, 1, 1, 0.14f);
      txt_desenhar(nome, s.x + 50, s.y + (s.h - nome.h) * 0.5f);
    }
    gfx_icone((GfxRect){ s.x + 17, s.y + (s.h - 22) * 0.5f, 22, 22 }, LAY_ICO[i], ic, ic, ic, k);
    sx += sw;
  }
  if (foco > 0) dica(x - 20, cy - 12, foco);   // na mesma linha: a grade fica livre
}
static void dirA(const Caso *c, float foco) {
  char forte[64], apoio[64];
  float y, h, cy, ar, ag, ab;
  TxtLinha eb = linha(TXT_HERO_META, c->eyebrow, 197, 255);
  txt_desenhar(eb, X0, ilhaY + (ilhaH - eb.h) * 0.5f);   // linha do relogio
  h = nomeGrande(c->nome, X0, 84, 900);
  y = 84 + h + 22;
  cy = y + 30;
  anelPontos(c, X0 + 30, cy, 26);
  contagem(c, forte, sizeof forte, apoio, sizeof apoio);
  { int todo = c->vistos >= c->total;
    TxtLinha f = linha(TXT_ILHA_NOME, forte, 242, 255), m;
    char meta[160];
    snprintf(meta, sizeof meta, "%s  ·  %s", apoio, c->meta);
    m = linha(TXT_DET_META2, meta, 186, 255);
    acento(&ar, &ag, &ab);
    txt_desenhar(f, X0 + 84, cy - f.h - 1);
    if (todo) txt_desenhar_tom(m, X0 + 84, cy + 3, ar, ag, ab, 1);
    else txt_desenhar(m, X0 + 84, cy + 3); }
  segmentado(c, DIR, cy, foco);
}

// --- B: ILHA DE VIDRO ---------------------------------------------------------
static void ticks(const Caso *c, float x, float y, float larg) {
  float ar, ag, ab, g = 4.0f, w = fminf(18.0f, (larg - g * (c->total - 1)) / c->total);
  int i, todo = c->vistos >= c->total;
  acento(&ar, &ag, &ab);
  for (i = 0; i < c->total; i++) {
    GfxRect r = { x + i * (w + g), y, w, 6 };
    if (todo) gfx_cor(r, 0.5f, ar, ag, ab, 1);
    else gfx_cor(r, 0.5f, 1, 1, 1, i < c->vistos ? 0.95f : 0.18f);
  }
}
static void dirB(const Caso *c, float foco) {
  const float PAD = 24.0f, AV = 104.0f, TX = 520.0f;
  char forte[64], apoio[64];
  float th = txt_bloco_corta(TXT_TITULO3, c->nome, 0, 0, 0, 0, 0, TX, 54, 0, 2);   // alpha 0 so mede
  TxtLinha eb = linha(TXT_ILHA_SECAO, c->eyebrow, 170, 255);
  float hTxt = eb.h + 6 + th + 18 + 6 + 10 + 22;
  float H = fmaxf(AV, hTxt) + PAD * 2, x = X0 - PAD, y = NV_ILHA_Y;
  float tw = fminf(TX, fmaxf(linha(TXT_TITULO3, c->nome, 242, 255).w, 360.0f));   // a ilha abraca o texto
  GfxRect R = { x, y, PAD + AV + 22 + tw + PAD + 8, H };
  float ty = y + PAD + (H - PAD * 2 - hTxt) * 0.5f, tx = x + PAD + AV + 22, ar, ag, ab;
  int todo = c->vistos >= c->total;
  ilhaVidro(R, 32.0f);
  // Retrato redondo ou capa: o conteudo dentro da ilha, concentrico com ela.
  { GfxRect a = { x + PAD, y + (H - AV) * 0.5f, AV, AV };
    if (c->pessoa) arte(a, c->foto, 0.5f, 1);
    else { a.x += AV * 0.17f; a.w = AV * 0.66f; arte(a, c->capas[0], 0.12f, 1); }
    acento(&ar, &ag, &ab);
    if (todo) gfx_anel_fora(a, c->pessoa ? 0.5f : 0.12f, 3, 2.5f, ar, ag, ab, 1); }
  txt_desenhar(eb, tx, ty);
  titulo(TXT_TITULO3, c->nome, tx, ty + eb.h + 6, TX, 54);
  ty += eb.h + 6 + th + 18;
  if (c->vistos) ticks(c, tx, ty, 300);   // zero: sem regua vazia, so o convite
  contagem(c, forte, sizeof forte, apoio, sizeof apoio);
  if (!c->vistos) { size_t k = strlen(forte); snprintf(forte + k, sizeof forte - k, "  ·  %s", apoio); }
  { TxtLinha f = linha(TXT_ILHA_SUB, forte, 236, 255);
    float fx = c->vistos ? tx + fminf(300.0f, c->total * 22.0f - 4.0f) + 14 : tx;
    if (todo) { gfx_icone((GfxRect){ fx, ty + 3 - 11, 22, 22 }, "aj_badge-check", ar, ag, ab, 1); fx += 28; }
    txt_desenhar(f, fx, ty + 3 - f.h * 0.5f); }
  // A troca de exibicao encosta no relogio: duas ilhas, mesma linha, mesmo
  // material. No foco ela cresce e mostra os vizinhos (a dica e o proprio gesto).
  { TxtLinha n = linha(TXT_AJ_SEG, LAY[c->lay], gfx_vidro_tinta(foco), 255);
    float pw = 48 + n.w + 22, w = pw, px;
    TxtLinha ant = linha(TXT_AJ_SEG, LAY[(c->lay + NLAY - 1) % NLAY], 235, 255);
    TxtLinha prx = linha(TXT_AJ_SEG, LAY[(c->lay + 1) % NLAY], 235, 255);
    if (foco > 0) w += (ant.w + prx.w + 2 * 40 + 16) * foco;
    GfxRect S = { ilhaX - 12 - w, NV_ILHA_Y, w, NV_ILHA_H };
    ilhaVidro(S, NV_ILHA_H * 0.5f);
    px = S.x + 4 + (foco > 0 ? (ant.w + 40) * foco : 0);
    if (foco > 0) {
      gfx_icone((GfxRect){ S.x + 12, S.y + 17, 18, 18 }, "pl_chevron-left", 1, 1, 1, 0.55f * foco);
      txt_desenhar_alpha(ant, S.x + 32, S.y + (S.h - ant.h) * 0.5f, 0.55f * foco);
      txt_desenhar_alpha(prx, px + pw + 8 + 8, S.y + (S.h - prx.h) * 0.5f, 0.55f * foco);
      gfx_icone((GfxRect){ S.x + S.w - 30, S.y + 17, 18, 18 }, "pl_chevron-right", 1, 1, 1, 0.55f * foco);
      gfx_vidro_pilula_cheia((GfxRect){ px, S.y + 4, pw, S.h - 8 }, 0.5f, foco, 1);
    }
    { float ic = gfx_vidro_tinta(foco) / 255.0f;
      gfx_icone((GfxRect){ px + 16, S.y + 15, 22, 22 }, LAY_ICO[c->lay], ic, ic, ic, 1); }
    txt_desenhar(n, px + 46, S.y + (S.h - n.h) * 0.5f); }
}

// --- C: PELA ARTE -------------------------------------------------------------
static float artePilha(const Caso *c, float x, float y, float *w) {
  int i;
  if (c->pessoa) {
    GfxRect r = { x, y, 184, 184 };
    sombra(r, 0.4f);
    arte(r, c->foto, 0.5f, 1);
    gfx_anel(r, 0.5f, 1.5f, 1, 1, 1, 0.22f);
    *w = 184;
    return 184;
  }
  // Pilha de capas no idioma do Fluxo de capas: retas, escala e veu.
  for (i = 2; i >= 0; i--) {
    float k = 1.0f - 0.12f * i, cw = 124 * k, ch = 186 * k;
    GfxRect r = { x + i * 34.0f, y + (186 - ch) * 0.5f, cw, ch };
    sombra(r, 0.35f);
    arte(r, c->capas[i], NV_RAIO_CARD, 1);
    if (i) gfx_cor(r, NV_RAIO_CARD, 0, 0, 0, 0.30f * i);
  }
  *w = 124 + 2 * 34;
  return 186;
}
// Uma capinha por titulo (2:3), cheia = visto. Quebra na largura da arte.
static float capinhas(const Caso *c, float x, float y, float larg) {
  const float W = 10, H = 15, G = 6;
  int por = (int)((larg + G) / (W + G)), i, todo = c->vistos >= c->total;
  float ar, ag, ab;
  acento(&ar, &ag, &ab);
  for (i = 0; i < c->total; i++) {
    GfxRect r = { x + (i % por) * (W + G), y + (i / por) * (H + G), W, H };
    if (todo) gfx_cor(r, 0.3f, ar, ag, ab, 1);
    else if (i < c->vistos) gfx_cor(r, 0.3f, 1, 1, 1, 0.95f);
    else gfx_anel(r, 0.3f, 1.5f, 1, 1, 1, 0.30f);
  }
  return ((c->total + por - 1) / por) * (H + G) - G;
}
static void dirC(const Caso *c, float foco) {
  char forte[64], apoio[64];
  float aw, ah = artePilha(c, X0, 40, &aw), tx = X0 + aw + 44, ar, ag, ab, h, y;
  int todo = c->vistos >= c->total;
  TxtLinha eb = linha(TXT_HERO_META, c->eyebrow, 197, 255);
  acento(&ar, &ag, &ab);
  if (c->vistos) capinhas(c, X0, 40 + ah + 18, aw);   // aparecem no primeiro visto
  txt_desenhar(eb, tx, ilhaY + (ilhaH - eb.h) * 0.5f);
  h = nomeGrande(c->nome, tx, 84, 900);
  y = 84 + h + 10;
  contagem(c, forte, sizeof forte, apoio, sizeof apoio);
  { char s[200]; TxtLinha m;
    if (c->vistos) snprintf(s, sizeof s, "%s  ·  %s", forte, c->meta);
    else snprintf(s, sizeof s, "%s  ·  %s  ·  %s", forte, c->meta, apoio);
    m = linha(TXT_DET_META2, s, 196, 255);
    if (todo) { gfx_icone((GfxRect){ tx, y + (m.h - 24) * 0.5f, 24, 24 }, "aj_badge-check", ar, ag, ab, 1);
                txt_desenhar(m, tx + 32, y); }
    else txt_desenhar(m, tx, y);
    y += m.h + 14; }
  // Seletor: uma pilula de vidro com setas. As setas SAO a dica; no foco ela
  // vira a pilula branca do app e o nome do vizinho aparece a direita.
  { TxtLinha n = linha(TXT_AJ_SEG, LAY[c->lay], gfx_vidro_tinta(foco), 255);
    GfxRect p = { tx, y, 34 + 30 + n.w + 34 + 18, NV_CTRL_H };
    float ic = gfx_vidro_tinta(foco) / 255.0f;
    sombra(p, 0.28f);
    gfx_vidro_painel(p, 0.5f, 0.62f, 1);
    if (foco > 0) gfx_vidro_pilula_cheia(p, 0.5f, foco, 1);
    gfx_icone((GfxRect){ p.x + 12, p.y + 19, 18, 18 }, "pl_chevron-left", ic, ic, ic, foco > 0 ? 1 : 0.5f);
    gfx_icone((GfxRect){ p.x + 36, p.y + 17, 22, 22 }, LAY_ICO[c->lay], ic, ic, ic, 1);
    txt_desenhar(n, p.x + 66, p.y + (p.h - n.h) * 0.5f);
    gfx_icone((GfxRect){ p.x + p.w - 30, p.y + 19, 18, 18 }, "pl_chevron-right", ic, ic, ic, foco > 0 ? 1 : 0.5f);
    if (foco > 0) {
      char s[64]; snprintf(s, sizeof s, "Next: %s", LAY[(c->lay + 1) % NLAY]);
      TxtLinha d = linha(TXT_ILHA_APOIO, s, 225, 255);
      txt_desenhar_alpha(d, p.x + p.w + 16, p.y + (p.h - d.h) * 0.5f, 0.72f * foco);
    } }
}

static void captura(const char *cam) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

static void quadro(char d, const Caso *c, float foco) {
  Uint32 agora = SDL_GetTicks();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(6);
  gfx_novo_quadro();
  glClearColor(0.051f, 0.051f, 0.051f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  corviva_definir(c->arte, 1);   // o acento "Da arte" sai do fundo, como no app
  corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), 0, 1);
  cena(c);
  ilha_relogio_visivel(1);
  ilha_ancorar(DIR, NV_ILHA_Y, 1);
  if (d == 'A') dirA(c, foco);
  else if (d == 'B') dirB(c, foco);
  else dirC(c, foco);
  ilha_desenhar(agora);
  ilha_rect(&ilhaX, &ilhaY, &ilhaW, &ilhaH);
  SDL_Delay(16);   // o relogio anima por tempo de parede
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-colhdr";
  const char *dados = getenv("NUVIO_DADOS");
  static const Caso casos[4] = {
    { "FILMOGRAPHY", "Alejandro González Iñárritu", "Director  ·  2000–2022", "deploy/app/art/26.jpg",
      "deploy/app/art/elenco/06_0.jpg", { 0 }, 1, 0, 9, 1 },
    { "STUDIO", "Studio Ghibli", "Animation  ·  1984–2023", "deploy/app/art/19.jpg", NULL,
      { "deploy/app/art/poster/07.jpg", "deploy/app/art/poster/12.jpg", "deploy/app/art/poster/21.jpg" },
      0, 7, 23, 0 },
    { "FILMOGRAPHY", "Denis Villeneuve", "Director  ·  1998–2024", "deploy/app/art/32.jpg",
      "deploy/app/art/elenco/03_0.jpg", { 0 }, 1, 11, 11, 2 },
    { "COLLECTION", "The Lord of the Rings and The Hobbit Collection", "Fantasy  ·  2001–2014", "deploy/app/art/37.jpg", NULL,
      { "deploy/app/art/poster/03.jpg", "deploy/app/art/poster/15.jpg", "deploy/app/art/poster/28.jpg" },
      0, 4, 6, 1 },
  };
  static const char *estado[4] = { "zero", "parcial", "completo", "foco" };
  char cam[700];
  SDL_GLContext gl;
  FILE *f;
  int d, k, i;
  assert(dados && *dados);
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados);
  f = fopen(cam, "w");
  assert(f);
  fprintf(f, "idioma 1\n");   // tema: o de fabrica da 2.0 (Da arte), como na TV do dono
  fclose(f);
  ajustes_dir(dados);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: colhdr mock", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  for (k = 0; k < 4; k++) {
    // A paleta da arte, como o decode faz no app (o fundo e a luz leem dela).
    SDL_Surface *b = IMG_Load(casos[k].arte), *t;
    CorvivaPaleta p;
    assert(b);
    t = SDL_ConvertSurfaceFormat(b, SDL_PIXELFORMAT_ABGR8888, 0);
    if (t && corviva_extrair(t->pixels, t->w, t->h, t->pitch, &p)) corviva_anotar(casos[k].arte, &p);
    if (t) SDL_FreeSurface(t);
    SDL_FreeSurface(b);
  }
  for (i = 0; i < 60; i++) { quadro('A', &casos[0], 0); SDL_GL_SwapWindow(win); }   // fontes e relogio prontos
  for (d = 0; d < 3; d++)
    for (k = 0; k < 4; k++) {
      for (i = 0; i < 70; i++) {
        quadro("ABC"[d], &casos[k], k == 3 ? 1.0f : 0.0f);
        if (i == 69) {
          snprintf(cam, sizeof cam, "%s/%c-%s.png", saida, "ABC"[d], estado[k]);
          captura(cam);
        }
        SDL_GL_SwapWindow(win);
      }
    }
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  SDL_Quit();
  puts("PASS: colhdr mock");
  return 0;
}
