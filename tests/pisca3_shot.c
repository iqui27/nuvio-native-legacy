// "A tela ainda ta piscando" (owner, LG C9): layout Apple TV (Dinamica) with
// Fundo = Frost and frosted glass. Renders consecutive frames of Home and the
// title page while nothing should change (idle, after a focus move, after the
// glass governor drops to smoked glass) and hashes the hero region and the
// whole frame per frame. After every swap the back buffer is filled with
// garbage, as GLES leaves it undefined (EGL_BUFFER_DESTROYED). Flicker = a
// hash that keeps changing in a settled window. PNGs of the first differing
// frames go to argv[1].
#include "ajustes.h"
#include "addons.h"
#include "artehero.h"
#include "catalogo.h"
#include "corviva.h"
#include "dados.h"
#include "detail.h"
#include "gfx.h"
#include "fundo.h"
#include "home.h"
#include "ilha.h"
#include "layout.h"
#include "menu.h"
#include "perfilsel.h"
#include "perfis.h"
#include "tex_cache.h"
#include "text.h"
#include "trailer.h"
#include "gl_compat.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <EGL/egl.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define W 1920
#define H 1080
static EGLDisplay egl;
static EGLSurface eglTela;
static const char *out;
static Uint32 relogio = 10000;
static unsigned char *px;
static unsigned char *ant;
static int failures;
static unsigned char *q1, *q2;   // the two frames before this one
static int nquadro, flashes, sujas, saltos;   // sujas: textures given garbage
static const char *cena = "settle";   // last window started (transitions follow it)
static int escolha;   // "Who's watching?" up: the Home assembles hidden behind it (app.c)
static int uploadFault, uploadFaults;
static GLenum uploadError;

Uint32 pisca3_ticks(void) { return relogio; }   // tests/pisca3_gles.h

// Mean |diff| over every 7th pixel (RGBA).
static double dif(const unsigned char *a, const unsigned char *b) {
  size_t p; double s = 0; int n = 0;
  for (p = 0; p < (size_t)W * H * 4; p += 28, n++)
    s += abs(a[p] - b[p]) + abs(a[p + 1] - b[p + 1]) + abs(a[p + 2] - b[p + 2]) + abs(a[p + 3] - b[p + 3]);
  return s / n;
}

// Changed box vs the previous frame, layout coords (top-left origin).
// Returns 1 when the change is visible (not 1-LSB easing noise).
static int caixa(const char *nome, int i, int falar) {
  int x, y, x0 = W, y0 = H, x1 = -1, y1 = -1, dmax = 0, n = 0;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    size_t p = ((size_t)y * W + x) * 4;
    int d = abs(px[p] - ant[p]) + abs(px[p + 1] - ant[p + 1]) + abs(px[p + 2] - ant[p + 2]) + abs(px[p + 3] - ant[p + 3]);
    if (d > dmax) dmax = d;
    if (d) {
      n++;
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
  }
  if (falar) printf("[pisca3] %s f%03d changed box x %d..%d y %d..%d px=%d maxdiff=%d\n", nome, i, x0, x1, H - 1 - y1, H - 1 - y0, n, dmax);
  return dmax > 8 && n > 100;
}

static void frame(void) {
  Uint32 now = relogio += 16;
  tex_bombear(8);
  home_oculta(escolha);
  home_atualizar(1.0f / 60, now);
  home_oculta(0);
  if (escolha) perfilsel_atualizar(1.0f / 60, now);
  home_trailer_passo(!detail_aberto(), 1.0f / 60, now);   // as app.c
  if (!detail_aberto() && home_pediu_abrir()) {
    HomeItem it;
    if (home_item_focado(&it)) detail_abrir(&it);
  }
  detail_atualizar(1.0f / 60, now);
  corviva_quadro(1.0f / 60, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  menu_atualizar(1.0f / 60, now);
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_ambiente(1.0f);
  if (escolha) perfilsel_desenhar(now);
  else {
    if (!detail_cobre_tela()) home_desenhar(now);
    detail_desenhar(now);
    if (!detail_aberto()) menu_desenhar(now);
  }
  ilha_desenhar(now);
  gfx_ambiente_descarregar();
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  // FLASH: the previous frame stands out from both neighbours, which agree.
  if (nquadro >= 2) {
    double a = dif(q2, q1), b = dif(q1, px), c = dif(q2, px);
    if (a > 2.0 && b > 2.0 && c < 0.25 * (a < b ? a : b)) {
      flashes++;
      printf("[pisca3] FLASH %s frame %d: prev-mid %.2f mid-cur %.2f prev-cur %.2f\n", cena, nquadro - 1, a, b, c);
    }
  }
  memcpy(q2, q1, (size_t)W * H * 4); memcpy(q1, px, (size_t)W * H * 4); nquadro++;
  eglSwapBuffers(egl, eglTela);
  // GLES: the back buffer is undefined after the swap. Garbage it.
  glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

// webOS composites the GL surface over the video plane by its ALPHA: any
// pixel with alpha < 1 shows what is behind (black, or the trailer).
static int furos(void) {
  size_t p;
  int n = 0;
  for (p = 3; p < (size_t)W * H * 4; p += 4) n += px[p] < 250;
  return n;
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
  frame();
  e.type = SDL_KEYUP;
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
}

// FNV over GL rows [y0, y1) (origin bottom), every 3rd pixel.
static unsigned hash(int y0, int y1) {
  unsigned h = 2166136261u;
  size_t p;
  for (p = (size_t)y0 * W * 4; p < (size_t)y1 * W * 4; p += 12)
    h = (h ^ px[p] ^ px[p + 1] << 8 ^ px[p + 2] << 16 ^ (unsigned)px[p + 3] << 24) * 16777619u;
  return h;
}

static void pngDe(const unsigned char *b, const char *nome) {
  char path[1024];
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(s);
  for (y = 0; y < H; y++) memcpy((char *)s->pixels + y * s->pitch, b + (size_t)(H - 1 - y) * W * 4, W * 4);
  snprintf(path, sizeof path, "%s/%s.png", out, nome);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
}
static void png(const char *nome) { pngDe(px, nome); }

// `n` frames that should all be identical: count frames whose hero or whole
// frame hash differs from the previous one, and A-B-A alternations.
static void parado(const char *nome, int n) {
  unsigned heroAnt = 0, telaAnt = 0, tela2 = 0;
  int i, mudou = 0, alterna = 0, gravou = 0, a0 = gfx_n_assados, furoMax = 0, f0 = gfx_n_fundo_assados;
  char nm[256];
  cena = nome;
  for (i = 0; i < n; i++) {
    frame();
    unsigned hero = hash(H - 640, H), tela = hash(0, H);
    { int f = furos(); if (f > furoMax) furoMax = f; }
    if (i && (hero != heroAnt || tela != telaAnt) && caixa(nome, i, mudou < 6)) {
      mudou++;
      if (i > 1 && tela == tela2) alterna++;
      if (gravou < 2) { snprintf(nm, sizeof nm, "%s-f%03d", nome, i); png(nm); gravou++; }
    }
    memcpy(ant, px, (size_t)W * H * 4);
    tela2 = telaAnt; heroAnt = hero; telaAnt = tela;
  }
  snprintf(nm, sizeof nm, "%s-fim", nome); png(nm);
  printf("[pisca3] %-14s frames=%d changed=%d alternating=%d translucent-px=%d glass-src=%d flat=%d caps=%d\n",
         nome, n, mudou, alterna, furoMax, gfx_vidro_com_fonte, gfx_vidro_chapados, gfx_vidro_capturas);
  printf("[pisca3] %-14s light-bakes=%d bg-bakes=%d forca=%.3f\n", nome, gfx_n_assados - a0, gfx_n_fundo_assados - f0, nv_ambiente_forca);
  printf("%s: %s settled\n", mudou ? "FAIL" : "PASS", nome);
  failures += mudou != 0;
}

// Frames until the whole frame stays identical for 60 frames (at most `max`):
// late network text must not land inside a settled window. Flicker never
// settles, and the window after it still fails.
static void assentar(int max) {
  unsigned h0 = 0;
  int i, iguais = 0;
  for (i = 0; i < max && iguais < 60; i++) {
    unsigned h;
    frame();
    h = hash(0, H);
    iguais = h == h0 ? iguais + 1 : 0;
    h0 = h;
  }
}

// A run that may change (fades): print per-frame mean diff and hole pixels,
// so a jump (one frame far above its neighbours) stands out.
static void rampa(const char *nome, int n) {
  int i;
  double d, dAnt = 0;
  cena = nome;
  for (i = 0; i < n; i++) {
    frame();
    d = dif(ant, px);
    printf("[pisca3] %s f%03d dif=%.2f holes=%d\n", nome, i, d, furos());
    // JUMP: a fade step far above the step before it (a fade only slows down).
    if (i && d > 8.0 && d > 6.0 * dAnt) {
      char nm[96];
      saltos++;
      printf("[pisca3] JUMP %s f%03d: %.2f after %.2f\n", nome, i, d, dAnt);
      snprintf(nm, sizeof nm, "%s-f%03d-jump", nome, i); png(nm);
      memcpy(px, ant, (size_t)W * H * 4);
      snprintf(nm, sizeof nm, "%s-f%03d-before", nome, i); png(nm);
      memcpy(px, q1, (size_t)W * H * 4);
    }
    dAnt = d;
    memcpy(ant, px, (size_t)W * H * 4);
  }
}

// HOME ARTWORK (owner, C9, no trailer): "pisca ao trocar de titulo, nas
// artwork, e quando abre a sidebar". The screen is cut in TW x TH tiles; a
// tile BLINKS when its hash, settled for two frames, comes back 2..4 frames
// later after a visibly different frame in between. The glass counters of each frame (frosted, flat) blink the same
// way when a glass goes flat for a frame or two and frosts back. PNG
// sequences of every blink go to argv[1]/home-arte/.
#define TW 96
#define TH 72
#define NTX (W / TW)
#define NTY (H / TH)
#define NHIST 6
static unsigned char *hist[NHIST];
static unsigned th[NHIST][NTX * NTY];
static int vg[NHIST][2], nh, piscas, vpiscas, seqs;
static unsigned tileHash(const unsigned char *b, int tx, int ty) {
  unsigned h = 2166136261u;
  int x, y;
  for (y = ty * TH; y < (ty + 1) * TH; y++)
    for (x = tx * TW; x < (tx + 1) * TW; x += 2) {
      const unsigned char *q = b + ((size_t)y * W + x) * 4;
      h = (h ^ q[0] ^ q[1] << 8 ^ q[2] << 16 ^ (unsigned)q[3] << 24) * 16777619u;
    }
  return h;
}
static int tileDif(const unsigned char *a, const unsigned char *b, int tx, int ty) {
  int x, y, m = 0;
  for (y = ty * TH; y < (ty + 1) * TH; y++)
    for (x = tx * TW; x < (tx + 1) * TW; x++) {
      size_t p = ((size_t)y * W + x) * 4;
      int d = abs(a[p] - b[p]) + abs(a[p + 1] - b[p + 1]) + abs(a[p + 2] - b[p + 2]);
      if (d > m) m = d;
    }
  return m;
}
static void seq(const char *nome, int k) {
  char nm[256];
  int j;
  if (seqs >= 6) return;
  seqs++;
  for (j = k; j >= 0; j--) {
    snprintf(nm, sizeof nm, "home-arte/%s-f%03d-%d", nome, nh - 1 - j, k - j);
    pngDe(hist[(nh - 1 - j) % NHIST], nm);
  }
}
static void arteQuadro(const char *nome) {
  int c = nh % NHIST, t, k, j, achou = 0;
  frame();
  memcpy(hist[c], px, (size_t)W * H * 4);
  vg[c][0] = gfx_vidro_com_fonte; vg[c][1] = gfx_vidro_chapados;
  for (t = 0; t < NTX * NTY; t++) th[c][t] = tileHash(px, t % NTX, NTY - 1 - t / NTX);
  nh++;
  for (k = 2; k < NHIST - 1 && k + 1 < nh; k++) {
    int a = (nh - 1 - k) % NHIST, a0 = (nh - 2 - k) % NHIST;
    for (t = 0; t < NTX * NTY && !achou; t++) {
      int tx = t % NTX, ty = NTY - 1 - t / NTX, mid = 0;
      // Settled before (A A), something else, then A again: a slide or a
      // fade never comes back.
      if (th[c][t] != th[a][t] || th[a0][t] != th[a][t]) continue;
      for (j = 1; j < k; j++) {
        int m = (nh - 1 - j) % NHIST;
        if (th[m][t] != th[c][t] && tileDif(hist[m], hist[c], tx, ty) > 24) mid = 1;
      }
      if (!mid) continue;
      achou = 1; piscas++;
      printf("[pisca3] BLINK %s f%03d tile x %d..%d y %d..%d back after %d frames\n", nome, nh - 1,
             tx * TW, tx * TW + TW - 1, (NTY - 1 - ty) * TH, (NTY - ty) * TH - 1, k);
      seq(nome, k);
    }
    if (vg[c][0] == vg[a][0] && vg[c][1] == vg[a][1] && vg[a0][0] == vg[a][0] && vg[a0][1] == vg[a][1] && !achou) {
      int mid = 0;
      for (j = 1; j < k; j++) {
        int m = (nh - 1 - j) % NHIST;
        mid |= vg[m][0] != vg[c][0] || vg[m][1] != vg[c][1];
      }
      if (mid) {
        achou = 1; vpiscas++;
        printf("[pisca3] GLASS-BLINK %s f%03d frosted=%d flat=%d back after %d frames\n", nome, nh - 1,
               vg[c][0], vg[c][1], k);
        seq(nome, k);
      }
    }
  }
  if (achou) for (j = 0; j < NHIST; j++) memcpy(th[j], th[c], sizeof th[c]);   // one report per blink
}
static void arte(const char *nome, SDL_Keycode k, int n) {
  int i;
  char nm[256];
  for (i = 0; i < n; i++) {
    SDL_Event e;
    memset(&e, 0, sizeof e);
    e.key.keysym.sym = k;
    if (k && i < 2) { e.type = i ? SDL_KEYUP : SDL_KEYDOWN; home_evento(&e); }   // as tecla()
    arteQuadro(nome);
    if (i < 12 && !(i & 3)) { snprintf(nm, sizeof nm, "home-arte/%s-%03d", nome, i); png(nm); }
  }
  printf("[pisca3] %-14s frosted=%d flat=%d caps=%d blinks=%d glass-blinks=%d\n", nome,
         gfx_vidro_com_fonte, gfx_vidro_chapados, gfx_vidro_capturas, piscas, vpiscas);
}
// PROMOTION (C9 log: "[tex-trace] dedup role=hero estado=promocao", then
// "upload-total ... mip=0"): the hero asks a card's art at hero size, the
// shared texture is decoded again larger. A card resting on screen must look
// the same before and after.
// Edge energy of `b` (sum of |dx| + |dy| on green) inside the box where the
// frame changed. Without its pyramid a 1280 texture drawn at 317 aliases:
// same picture, much more edge energy. A different mip level of the same
// pyramid moves it under 10% (ANGLE: x1.32 without, x0.92 with).
static int bx0, by0, bx1, by1;
static void caixaMudou(void) {
  int x, y;
  bx0 = W; by0 = H; bx1 = by1 = -1;
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    size_t p = ((size_t)y * W + x) * 4;
    if (memcmp(px + p, ant + p, 3)) {
      if (x < bx0) bx0 = x;
      if (x > bx1) bx1 = x;
      if (y < by0) by0 = y;
      if (y > by1) by1 = y;
    }
  }
}
static double bordas(const unsigned char *b) {
  long soma = 0;
  int x, y;
  for (y = by0; y < by1; y++) for (x = bx0; x < bx1; x++) {
    size_t p = ((size_t)y * W + x) * 4 + 1;
    soma += abs(b[p] - b[p + 4]) + abs(b[p] - b[p + (size_t)W * 4]);
  }
  return soma;
}

static void promo(const char *arte) {
  int i;
  double pior = 1;
  for (i = 0; i < 90; i++) frame();
  memcpy(ant, px, (size_t)W * H * 4);
  png("home-arte/arte-promo-before");
  for (i = 0; i < 60; i++) {
    double r;
    (void)tex_obter_hero(arte);
    frame();
    caixaMudou();
    if (bx1 < 0) continue;
    r = bordas(px) / (bordas(ant) + 1);
    if (fabs(r - 1) > fabs(pior - 1)) { pior = r; (void)caixa("arte-promo", i, 1); png("home-arte/arte-promo-after"); }
  }
  printf("[pisca3] arte-promo edge energy x%.3f\n%s: a resting card keeps its look when its art is promoted\n",
         pior, fabs(pior - 1) > 0.15 ? "FAIL" : "PASS");
  failures += fabs(pior - 1) > 0.15;
}

static void homeArte(const char *promoArte) {
  char nm[64];
  int i;
  for (i = 0; i < NHIST; i++) { hist[i] = malloc((size_t)W * H * 4); assert(hist[i]); }
  snprintf(nm, sizeof nm, "%s/home-arte", out); mkdir(nm, 0755);
  tecla(SDLK_DOWN); promo(promoArte); tecla(SDLK_UP);   // a row card the hero never showed
  for (i = 0; i < 120; i++) frame();
  for (i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "arte-right%d", i); arte(nm, SDLK_RIGHT, 90); }
  arte("arte-down", SDLK_DOWN, 120);
  for (i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "arte-row%d", i); arte(nm, SDLK_RIGHT, 90); }
  menu_abrir();  arte("arte-sidebar", 0, 90);
  menu_fechar(); arte("arte-sidebar-off", 0, 90);
  for (i = 0; i < 4; i++) { snprintf(nm, sizeof nm, "arte-back%d", i); arte(nm, SDLK_LEFT, 60); }
  arte("arte-up", SDLK_UP, 120);
  printf("[pisca3] blinks=%d glass-blinks=%d\n%s: home artwork does not blink\n", piscas, vpiscas,
         piscas || vpiscas ? "FAIL" : "PASS");
  failures += piscas || vpiscas;
}

// Freeze the real picker at its current animation state. Compare every pixel,
// so missing/streaked artwork lasting any number of frames cannot pass.
static void pickerPixels(void) {
  gfx_novo_quadro();
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  perfilsel_desenhar(relogio);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
}

static void pickerPressure(void) {
  char paths[513][1024];
  int slots, i, j, ready, bad = 0;
  long evictions = tex_despejos_total, hot = tex_despejos_quentes_total;
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 255, 0, 255, 255));
  tex_orcamento_info(NULL, NULL, NULL, &slots);
  assert(slots > 12 && slots <= 512);
  // Home/background preloads compete with the wall in the same bounded cache.
  // Keep the picker hot while filling it; older preloads become cold victims.
  for (i = 0; i <= slots; i++) {
    snprintf(paths[i], sizeof paths[i], "%s/pressure-%03d.bmp", getenv("NUVIO_DADOS"), i);
    assert(SDL_SaveBMP(s, paths[i]) == 0);
    ready = 0;
    for (j = 0; j < 1000 && !ready; j++) {
      tex_novo_quadro();
      perfilsel_atualizar(0, relogio);
      ready = tex_obter_larg(paths[i], 128) != 0;
      tex_bombear(8);
      SDL_Delay(1);
    }
    assert(ready);
  }
  SDL_FreeSurface(s);
  // Drain pending picker promotions before taking the clean reference.
  for (i = 0; i < 120; i++) {
    tex_bombear(8);
    tex_novo_quadro();
    perfilsel_atualizar(0, relogio);
    SDL_Delay(1);
  }
  pickerPixels();
  png("perfil/pressure-clean");
  memcpy(ant, px, (size_t)W * H * 4);
  // All resident preloads are now hot, with the wall oldest in LRU order.
  // A new request must wait, rather than deleting artwork already handed out.
  for (i = 0; i <= slots; i++)
    if (tex_aspecto(paths[i]) > 0) assert(tex_obter_larg(paths[i], 128));
  assert(tex_aspecto(paths[0]) == 0);
  assert(tex_obter_larg(paths[0], 128) == 0);
  for (i = 0; i < 4; i++) {
    pickerPixels();
    bad += memcmp(ant, px, (size_t)W * H * 4) != 0;
  }
  png("perfil/pressure-loaded");
  printf("%s: picker artwork survives full hot cache (bad frames=%d hot evictions=%ld cold evictions=%ld)\n",
         bad || tex_despejos_quentes_total != hot ? "FAIL" : "PASS", bad,
         tex_despejos_quentes_total - hot,
         tex_despejos_total - evictions - (tex_despejos_quentes_total - hot));
  failures += bad || tex_despejos_quentes_total != hot;
  failures += tex_despejos_total == evictions; // The reproduction must exercise eviction.

  // Explicitly discard a visible poster, then require identical artwork again.
  assert(tex_esquecer("deploy/app/art/poster/00.jpg"));
  tex_novo_quadro();
  perfilsel_atualizar(0, relogio);
  pickerPixels();
  assert(memcmp(ant, px, (size_t)W * H * 4)); // The discarded art was visible.
  ready = 0;
  for (i = 0; i < 1000; i++) {
    tex_bombear(8);
    tex_novo_quadro();
    perfilsel_atualizar(0, relogio);
    pickerPixels();
    if (!memcmp(ant, px, (size_t)W * H * 4)) { ready = 1; break; }
    SDL_Delay(1);
  }
  png("perfil/pressure-recovered");
  printf("%s: picker restores original artwork after eviction\n", ready ? "PASS" : "FAIL");
  failures += !ready;
  // Deferred requests must eventually load when the preload set cools down.
  ready = 0;
  for (i = 0; i < 1000 && !ready; i++) {
    tex_novo_quadro();
    perfilsel_atualizar(0, relogio);
    ready = tex_obter_larg(paths[0], 128) != 0;
    tex_bombear(8);
    SDL_Delay(1);
  }
  printf("%s: deferred artwork loads after cold eviction\n", ready ? "PASS" : "FAIL");
  failures += !ready;
  failures += glGetError() != GL_NO_ERROR;
}

// PROFILE PICKER (owner, C9, 10-17): "No profile picker tb ta piscando a
// artwork". Fundo Filmes (the default): the poster wall behind "Who's
// watching?", the Home assembling hidden behind it as app.c does.
static void escolhaPerfil(void) {
  char nm[64];
  int i, antes;
  snprintf(nm, sizeof nm, "%s/perfil", out); mkdir(nm, 0755);
  assert(dados_gravar("perfis.txt", "1\t0\t1\t0\t#FB8C00\tHenrique\tdeploy/app/art/poster/00.jpg\t\n"
                                    "2\t1\t0\t0\t#2F3BFF\tJose\t\t\n"));
  assert(dados_gravar("perfil.txt", "1\n"));
  perfis_carregar_ativo();
  assert(perfis_n() == 2);
  perfilsel_iniciar();
  escolha = 1;
  antes = flashes;
  for (i = 0; i < 300; i++) {
    frame();
    if (i % 15 == 0) { snprintf(nm, sizeof nm, "perfil/f%03d", i); png(nm); }
    { int h = furos(); if (h) printf("[pisca3] perfil f%03d holes=%d\n", i, h);
      failures += h != 0; }
  }
  pickerPressure();
  escolha = 0;
  printf("%s: profile picker wall without a single-frame flash (%d)\n", flashes > antes ? "FAIL" : "PASS", flashes - antes);
  failures += flashes > antes;
}

// Fake native trailer: plays, settles, then the webOS pipeline stalls
// (video_pronto drops while the uMS reloads) and resumes, then a 4-frame blip.
static void trailerCiclo(const char *onde, int dono) {
  char nm[64];
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  trailer_teste_falso(1);
  frame(); frame();                           // home: the hero claims the trailer slot
  trailer_abrir("fake://trailer", tela, 0, 0);
  trailer_marcar_dono(dono, "tt9000000");
  snprintf(nm, sizeof nm, "%s-trailer-in", onde); rampa(nm, 120);
  snprintf(nm, sizeof nm, "%s-trailer", onde); parado(nm, 60);
  trailer_teste_falso(0); snprintf(nm, sizeof nm, "%s-stall", onde); rampa(nm, 90);
  trailer_teste_falso(1); snprintf(nm, sizeof nm, "%s-resume", onde); rampa(nm, 90);
  trailer_teste_falso(0); snprintf(nm, sizeof nm, "%s-blip", onde); rampa(nm, 4);
  trailer_teste_falso(1); snprintf(nm, sizeof nm, "%s-blip-back", onde); rampa(nm, 60);
  trailer_fechar(); trailer_teste_falso(0);   // still supported, as on the TV
}

// An unpublished striped upload must not invalidate settled Frost/glass.
static void uploadFrame(GLuint poster) {
  gfx_novo_quadro();
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  fundo_desenhar_modo(FUNDO_FROST, (GfxRect){0, 0, W, H}, 0, NULL, 1);
  gfx_rect((GfxRect){200, 200, 288, 432}, poster, GFX_CARD, 0, 0, 0, 0, 1, 1, 1, 1);
  gfx_rect((GfxRect){650, 420, 960, 540}, poster, GFX_CARD, 0, 0, 0, 0, 1, 1, 1, 1);
  if (ajustes_vidro()) gfx_vidro((GfxRect){1000, 100, 400, 200}, .1f, GFX_VIDRO_PAINEL, .1f, .8f, 1);
  gfx_ambiente_descarregar();
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  eglSwapBuffers(egl, eglTela);
}
static void uploadPressure(void) {
  char path[1024];
  GLuint poster = 0, incoming = 0;
  SDL_Surface *s;
  int i, stripes = 0, captures = 0, changed = 0;
  float saved = nv_ambiente_forca;
  nv_ambiente_forca = 0;
  for (i = 0; i < 1000 && !poster; i++) {
    poster = tex_obter_larg_qualquer("deploy/app/art/poster/00.jpg", 288);
    tex_bombear(1); SDL_Delay(1);
  }
  assert(poster);
  for (i = 0; i < 5; i++) uploadFrame(poster);
  memcpy(ant, px, (size_t)W * H * 4); png("upload-settled");
  snprintf(path, sizeof path, "%s/upload.bmp", getenv("NUVIO_DADOS"));
  s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s); SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 230, 20, 180, 255));
  assert(SDL_SaveBMP(s, path) == 0); SDL_FreeSurface(s);
  for (i = 0; i < 1000 && !incoming; i++) {
    tex_novo_quadro();
    assert(tex_obter_larg_qualquer("deploy/app/art/poster/00.jpg", 288) == poster);
    incoming = tex_obter_hero(path);
    tex_upl_bytes = 0; tex_bombear(1);
    uploadFrame(poster);
    if (tex_upl_bytes && !incoming) {
      printf("[pisca3] private upload f%03d bytes=%ld captures=%d\n", i, tex_upl_bytes, gfx_vidro_capturas);
      stripes++;
      captures += gfx_vidro_capturas;
    }
    changed += memcmp(ant, px, (size_t)W * H * 4) != 0;
    SDL_Delay(1);
  }
  png("upload-finished");
  printf("%s: private upload stripes=%d glass recaptures=%d changed frames=%d\n",
         incoming && stripes >= 2 && !captures && !changed ? "PASS" : "FAIL", stripes, captures, changed);
  failures += !incoming || stripes < 2 || captures || changed;
  nv_ambiente_forca = saved;
}

// Failed GPU uploads must never replace a visible lower-resolution poster.
static void uploadFailure(void) {
  int mode;
  float saved = nv_ambiente_forca;
  nv_ambiente_forca = 0;
  for (mode = 1; mode <= 4; mode++) {
    char path[1024], name[64];
    GLuint poster = 0;
    int i, bad = 0, injected = uploadFaults, captures = 0;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    assert(s);
    SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 70, 180, 110, 255));
    snprintf(path, sizeof path, "%s/promotion-%d.bmp", getenv("NUVIO_DADOS"), mode);
    assert(SDL_SaveBMP(s, path) == 0); SDL_FreeSurface(s);
    for (i = 0; i < 1000 && !poster; i++) {
      poster = tex_obter_larg_qualquer(path, 200);
      tex_bombear(1); SDL_Delay(1);
    }
    assert(poster);
    for (i = 0; i < 5; i++) uploadFrame(poster);
    memcpy(ant, px, (size_t)W * H * 4);
    snprintf(name, sizeof name, "upload-fault-%d-before", mode); png(name);
    uploadFault = mode;
    for (i = 0; i < 80; i++) {
      GLuint visible;
      tex_novo_quadro();
      (void)tex_obter_hero(path);
      visible = tex_obter_larg_qualquer(path, 200);
      tex_bombear(1);
      visible = tex_obter_larg_qualquer(path, 200);
      uploadFrame(visible);
      captures += gfx_vidro_capturas;
      if (visible != poster || memcmp(ant, px, (size_t)W * H * 4)) {
        if (!bad) {
          printf("[pisca3] failed promotion mode=%d first changed frame=f%03d\n", mode, i);
          snprintf(name, sizeof name, "upload-fault-%d-f%03d", mode, i); png(name);
        }
        bad++;
      }
      SDL_Delay(1);
    }
    uploadFault = 0;
    snprintf(name, sizeof name, "upload-fault-%d-after", mode); png(name);
    printf("%s: failed promotion mode=%d injected=%d changed frames=%d recaptures=%d\n",
           uploadFaults > injected && !bad && !captures ? "PASS" : "FAIL", mode, uploadFaults - injected, bad, captures);
    failures += uploadFaults == injected || bad || captures;
    // The renderer must consume the upload error before touching the old art.
    failures += glGetError() != GL_NO_ERROR;
  }
  // Without an older texture, the failed item must remain unavailable.
  for (mode = 1; mode <= 4; mode++) {
    char path[1024];
    int i, bad = 0, injected = uploadFaults;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    assert(s); SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 70, 180, 110, 255));
    snprintf(path, sizeof path, "%s/first-%d.bmp", getenv("NUVIO_DADOS"), mode);
    assert(SDL_SaveBMP(s, path) == 0); SDL_FreeSurface(s);
    uploadFault = mode;
    for (i = 0; i < 80; i++) {
      GLuint visible;
      tex_novo_quadro();
      visible = mode == 3 ? tex_obter_larg_qualquer(path, 200) : tex_obter_hero(path);
      bad += visible != 0;
      tex_bombear(1); SDL_Delay(1);
    }
    uploadFault = 0;
    printf("%s: failed first upload mode=%d injected=%d published=%d\n",
           uploadFaults > injected && !bad ? "PASS" : "FAIL", mode, uploadFaults - injected, bad);
    failures += uploadFaults == injected || bad;
    failures += glGetError() != GL_NO_ERROR;
  }
  nv_ambiente_forca = saved;
}

// Poll real decode/upload work without advancing the injected retry clock.
static GLuint retryPoll(const char *path) {
  GLuint visible = 0;
  for (int i = 0; i < 100; i++) {
    tex_novo_quadro();
    visible = tex_obter_larg_qualquer(path, 200);
    tex_bombear(1); SDL_Delay(1);
  }
  return visible;
}

static void uploadRetry(void) {
  static const Uint32 waits[] = {2000, 10000, 60000};
  for (int recovery = 0; recovery < 2; recovery++) {
    char path[1024];
    int injected = uploadFaults, bad = 0;
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 200, 300, 32, SDL_PIXELFORMAT_RGBA32);
    assert(s); SDL_FillRect(s, NULL, SDL_MapRGBA(s->format, 70, 180, 110, 255));
    snprintf(path, sizeof path, "%s/retry-%d.bmp", getenv("NUVIO_DADOS"), recovery);
    assert(SDL_SaveBMP(s, path) == 0); SDL_FreeSurface(s);
    uploadFault = 3;
    bad += retryPoll(path) != 0 || !tex_falhou(path) || uploadFaults - injected != 1;
    for (int attempt = 0; attempt < (recovery ? 2 : 3); attempt++) {
      relogio += waits[attempt] - 1;
      bad += retryPoll(path) != 0 || !tex_falhou(path) || uploadFaults - injected != attempt + 1;
      relogio++;
      if (recovery && attempt == 1) uploadFault = 0;
      GLuint visible = retryPoll(path);
      if (recovery && attempt == 1)
        bad += !visible || tex_falhou(path) || uploadFaults - injected != 2;
      else
        bad += visible != 0 || !tex_falhou(path) || uploadFaults - injected != attempt + 2;
    }
    relogio += 120000;
    GLuint visible = retryPoll(path);
    bad += recovery ? !visible || uploadFaults - injected != 2
                    : visible != 0 || !tex_falhou(path) || uploadFaults - injected != 4;
    uploadFault = 0;
    printf("%s: GPU retry %s failures=%d violations=%d\n", bad ? "FAIL" : "PASS",
           recovery ? "recovery" : "limit/backoff", uploadFaults - injected, bad);
    failures += bad != 0;
    failures += glGetError() != GL_NO_ERROR;
  }
}

// Fixed accent and immersive off: changing scene colors cannot change Frost.
static void frostCache(void) {
  float regions[4][3], saved = nv_ambiente_forca;
  memcpy(regions, nv_ambiente_viva, sizeof regions);
  assert(ajustes_shot_valor("selected_theme", 0));
  assert(ajustes_cor_viva() == 0);
  nv_ambiente_forca = 0;
  for (int glass = 1; glass >= 0; glass--) {
    ajustes_definir_vidro(glass);
    for (int i = 0; i < 5; i++) uploadFrame(0);
    memcpy(ant, px, (size_t)W * H * 4);
    int baked = gfx_n_fundo_assados, changed = 0, captures = 0;
    for (int i = 0; i < 30; i++) {
      for (int region = 0; region < 4; region++)
        for (int c = 0; c < 3; c++) nv_ambiente_viva[region][c] = (float)(i + region + c) / 40;
      nv_tempo_viva += 1.0f / 60;
      uploadFrame(0);
      changed += memcmp(ant, px, (size_t)W * H * 4) != 0;
      captures += gfx_vidro_capturas;
    }
    int bad = changed || captures || gfx_n_fundo_assados != baked;
    printf("%s: fixed-accent Frost glass=%d bakes=%d changed=%d captures=%d\n",
           bad ? "FAIL" : "PASS", glass, gfx_n_fundo_assados - baked, changed, captures);
    failures += bad;
  }
  ajustes_definir_vidro(1);
  assert(ajustes_shot_valor("selected_theme", 12));
  memcpy(nv_ambiente_viva, regions, sizeof regions);
  nv_ambiente_forca = saved;
}

// Inspect the Frost itself, before UI overlays can hide a discontinuity.
// Every sampled pixel must stay opaque and move at most two luminance levels
// per frame while the immersive strength moves by 1/120.
static void frostTransicoes(void) {
  float saved = nv_ambiente_forca;
  float regions[4][3];
  GfxRect tela = {0, 0, W, H};
  int direction, i, x, y, bad = 0;
  double worst = 0;
  memcpy(regions, nv_ambiente_viva, sizeof regions);
  for (i = 0; i < 4; i++) {
    nv_ambiente_viva[i][0] = 0.44f;
    nv_ambiente_viva[i][1] = 0.22f;
    nv_ambiente_viva[i][2] = 0.10f;
  }
  for (direction = 0; direction < 2; direction++) for (i = 0; i <= 120; i++) {
    char nm[80];
    double jump = 0;
    nv_ambiente_forca = (direction ? 120 - i : i) / 120.0f;
    gfx_novo_quadro(); gfx_ambiente_preparar();
    glClearColor(1, 0, 1, 0); glClear(GL_COLOR_BUFFER_BIT);
    fundo_desenhar_modo(FUNDO_FROST, tela, 0, NULL, 1);
    gfx_ambiente_descarregar();
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    if (i) for (y = 24; y < H; y += 48) for (x = 24; x < W; x += 48) {
      size_t p = ((size_t)y * W + x) * 4;
      double d = fabs(0.2126 * ((int)px[p] - ant[p]) +
                      0.7152 * ((int)px[p+1] - ant[p+1]) +
                      0.0722 * ((int)px[p+2] - ant[p+2]));
      if (d > jump) jump = d;
      assert(px[p+3] == 255);
    }
    if (!direction && (i == 0 || i == 120)) {
      snprintf(nm, sizeof nm, "frost-settled-%d", i); png(nm);
    }
    if (jump > worst) worst = jump;
    printf("[pisca3] frost-immersive-%d f%03d strength=%.4f jump=%.3f bakes=%d\n",
           direction, i, nv_ambiente_forca, jump, gfx_n_fundo_assados);
    if (jump > 2) {
      printf("FAIL: frost-immersive-%d f%03d luminance jump %.3f\n", direction, i, jump);
      snprintf(nm, sizeof nm, "frost-immersive-%d-f%03d-before", direction, i); pngDe(ant, nm);
      snprintf(nm, sizeof nm, "frost-immersive-%d-f%03d-after", direction, i); png(nm);
      bad++;
    }
    memcpy(ant, px, (size_t)W * H * 4);
    eglSwapBuffers(egl, eglTela);
  }
  printf("%s: Frost immersive transitions max luminance step %.3f\n", bad ? "FAIL" : "PASS", worst);
  failures += bad != 0;
  nv_ambiente_forca = saved;
  memcpy(nv_ambiente_viva, regions, sizeof regions);
}

int main(int argc, char **argv) {
  static CatItem items[10];
  CatFileira row;
  char path[1024];
  int i;
  out = argc > 1 ? argv[1] : "/tmp/pisca3";
  px = malloc((size_t)W * H * 4); ant = malloc((size_t)W * H * 4);
  q1 = calloc((size_t)W * H, 4); q2 = calloc((size_t)W * H, 4);
  assert(px && ant && q1 && q2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  { // GLES2 (ANGLE), as the TV. Alpha 8 as main.c: alpha is the video-plane window.
    static const EGLint ca[] = { EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                 EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                                 EGL_NONE };
    static const EGLint sa[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
    static const EGLint xa[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLint n;
    EGLConfig c;
    EGLContext x;
    egl = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(egl != EGL_NO_DISPLAY && eglInitialize(egl, NULL, NULL));
    assert(eglChooseConfig(egl, ca, &c, 1, &n) && n == 1);
    eglTela = eglCreatePbufferSurface(egl, c, sa); assert(eglTela != EGL_NO_SURFACE);
    x = eglCreateContext(egl, c, EGL_NO_CONTEXT, xa); assert(x != EGL_NO_CONTEXT);
    assert(eglMakeCurrent(egl, eglTela, eglTela, x));
    printf("[pisca3] GL: %s | %s\n", (const char *)glGetString(GL_VERSION), (const char *)glGetString(GL_RENDERER));
  }
  glViewport(0, 0, W, H); gfx_tamanho_alvo(W, H);
  { GLint ab = 0; glGetIntegerv(GL_ALPHA_BITS, &ab); printf("[pisca3] alpha bits %d\n", ab); }
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  // Apple TV layout, Fundo = Frost, glass on, no clock (it ticks).
  fputs("homeLayoutLocal 2\nvidroLocal 0\nfundoLocal 2\nidioma 0\ntrailerHero 0\nrelogioTelaLocal 0\nselected_theme 12\n", f);
  fclose(f);
  // V_LIGA: 0 = on. trailer-1310.txt: skip the one-time "trailers off" migration.
  snprintf(path, sizeof path, "%s/trailer-1310.txt", getenv("NUVIO_DADOS"));
  f = fopen(path, "w"); assert(f); fputs("1\n", f); fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_home_layout() == HOME_LAYOUT_DINAMICA);
  assert(ajustes_cor_viva() == CORVIVA_SIMPLES);   // C9 log: cor-viva=1
  tex_iniciar(64); gfx_borrao_iniciar(480, 270); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art")); menu_iniciar(); ilha_relogio_visivel(0);
  { AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
    assert(addons_definir_lista(&addon, 1)); }
  memset(&row, 0, sizeof row);
  snprintf(row.chave, sizeof row.chave, "popular_movie");
  snprintf(row.titulo, sizeof row.titulo, "Popular");
  snprintf(row.tipo, sizeof row.tipo, "movie");
  row.n = 10;
  for (i = 0; i < 10; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Offline movie %d", i + 1);
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "An offline story beneath the glass chrome.");
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(items[i].backdropCatalogo, sizeof items[i].backdropCatalogo, "%s", items[i].backdrop);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i);
    snprintf(items[i].logo, sizeof items[i].logo, "deploy/app/art/logo/%02d.png", i);
  }
  cat_definir_tudo(items, 10, &row, 1);
  if (argc > 2 && !strcmp(argv[2], "upload")) {
    uploadPressure();
    uploadFailure();
    uploadRetry();
    frostCache();
    tex_encerrar(); SDL_Quit();
    return failures ? 1 : 0;
  }
  if (argc > 2 && !strcmp(argv[2], "frost")) {
    frostTransicoes();
    printf("[pisca3] gl=%#x\n", glGetError());
    tex_encerrar(); SDL_Quit();
    return failures ? 1 : 0;
  }
  frostTransicoes();
  uploadPressure();
  uploadFailure();
  escolhaPerfil();
  if (argc > 2 && !strcmp(argv[2], "perfil")) return failures ? 1 : 0;

  for (i = 0; i < 240; i++) frame();          // settle: art loads, springs rest
  parado("home-idle", 90);
  homeArte(items[3].backdrop);
  trailerCiclo("home", TRAILER_DONO_HOME);
  for (i = 0; i < 120; i++) frame();
  tecla(SDLK_DOWN);
  for (i = 0; i < 150; i++) frame();
  parado("home-moved", 60);
  tecla(SDLK_RIGHT);
  for (i = 0; i < 90; i++) frame();
  { HomeItem it;                              // open the title (carousel, Frost page)
    int k;
    for (k = 0; k < 3 && !home_item_focado(&it); k++) {   // catalogue revision moved: refocus
      tecla(SDLK_LEFT); for (i = 0; i < 60; i++) frame();
      tecla(SDLK_RIGHT); for (i = 0; i < 60; i++) frame();
    }
    assert(home_item_focado(&it)); detail_abrir(&it); }
  for (i = 0; i < 300; i++) frame();
  assentar(900);                              // network meta (Cinemeta, IMDb) lands late
  assert(detail_aberto());
  parado("detail-idle", 90);
  trailerCiclo("detail", TRAILER_DONO_DETALHE);   // C9 log: "[trailer] apple tt1430132 ... HLS"
  for (i = 0; i < 120; i++) frame();
  tecla(SDLK_DOWN);                           // into the page: the card expands over Frost
  for (i = 0; i < 200; i++) frame();
  parado("page-idle", 90);
  tecla(SDLK_DOWN);
  for (i = 0; i < 600; i++) frame();          // the offline extras text lands late
  parado("page-down", 90);
  for (i = 0; i < 4 * 150; i++) gfx_vidro_governar(100.0, 16.7);   // C9: the governor trips
  for (i = 0; i < 30; i++) frame();
  parado("detail-smoked", 90);
  printf("[pisca3] jumps=%d\n%s: trailer fades without a jump\n", saltos, saltos ? "FAIL" : "PASS");
  failures += saltos != 0;
  printf("[pisca3] garbage-textures=%d\n", sujas);
  printf("[pisca3] flashes=%d\n%s: no single-frame flash\n", flashes, flashes ? "FAIL" : "PASS");
  failures += flashes != 0;
  printf("[pisca3] degraded=%d gl=%#x\n", gfx_vidro_degradado(), glGetError());
  tex_encerrar(); SDL_Quit(); free(px);
  return failures ? 1 : 0;
}

// GLES: a new texture's contents are undefined. Garbage, as a Mali can give.
// Uploads that never complete allocation, stripe, or mip generation surface
// as GL_OUT_OF_MEMORY on webOS; the caller decides whether the texture can be
// published.
#undef glTexImage2D
#undef glTexSubImage2D
#undef glGenerateMipmap
#undef glDrawArrays
#undef glGetError
extern GLenum glGetError(void);
extern void glGenerateMipmap(GLenum);
extern void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void *);

static int failUpload(int mode, GLsizei w) {
  if (uploadFault != mode || (mode != 3 && w < 1024)) return 0;
  uploadError = GL_OUT_OF_MEMORY; uploadFaults++;
  return 1;
}
GLenum pisca3_error(void) {
  GLenum e = uploadError;
  uploadError = GL_NO_ERROR;
  return e ? e : glGetError();
}
extern void glDrawArrays(GLenum, GLint, GLsizei);
void pisca3_drawarrays(GLenum mode, GLint first, GLsizei count) {
  if (uploadFault == 4) {
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] == 1 && viewport[3] == 1 && failUpload(4, 1024)) return;
  }
  glDrawArrays(mode, first, count);
}
void pisca3_mipmap(GLenum target) {
  if (!failUpload(3, 0)) glGenerateMipmap(target);
}
void pisca3_texsubimage(GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h,
                        GLenum f, GLenum ty, const void *p) {
  if (!failUpload(2, w)) glTexSubImage2D(t, l, x, y, w, h, f, ty, p);
}
extern void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
void pisca3_teximage(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty,
                     const void *p) {
  unsigned char *lixo;
  size_t n, k;
  if (failUpload(1, w)) return;
  glTexImage2D(t, l, i, w, h, b, f, ty, p);
  if (p || ty != GL_UNSIGNED_BYTE || (f != GL_RGB && f != GL_RGBA) || w <= 0 || h <= 0) return;
  n = (size_t)w * h * (f == GL_RGB ? 3 : 4);
  lixo = malloc(n);
  if (!lixo) return;
  for (k = 0; k < n; k++) lixo[k] = (unsigned char)(k * 2654435761u >> 13) | 0x80;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(t, l, 0, 0, w, h, f, ty, lixo);
  sujas++;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  free(lixo);
}
