// Vidro neutro (dono, 14/10/2026: "o glass UI no frost ta tendo aberracao de
// cor, ficando meio verde"). O fosco lia um assado de OUTRA fonte (a luz da
// paleta do titulo em cena), nao o que esta atras do painel: com um titulo
// verde, o vidro sobre um fundo cinza saia verde. Aqui o fundo e cinza neutro
// e branco, a luz da paleta e verde, e a media da area de vidro tem de sair
// neutra: |G - (R+B)/2| <= 2 em 8 bits. Tambem confere que o vidro desfoca o
// que esta ATRAS dele (uma borda preto/branco vira degrade dentro do painel).
//   bash tests/vidro_neutro.sh [pasta-de-capturas]
#include "gfx.h"
#include "corviva.h"
#include "ajustes.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;
static int falhas;

static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}
static void media(int x, int y, int w, int h, double m[3]) {
  unsigned char *p = malloc((size_t)w * h * 4);
  int i;
  assert(p);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(x, LH - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
  m[0] = m[1] = m[2] = 0;
  for (i = 0; i < w * h; i++) { m[0] += p[4*i]; m[1] += p[4*i+1]; m[2] += p[4*i+2]; }
  for (i = 0; i < 3; i++) m[i] /= (double)(w * h);
  free(p);
}
static void salvar(const char *dir, const char *nome) {
  SDL_Surface *s; unsigned char *p, *t; int y; char cam[700];
  if (!dir) return;
  s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t); snprintf(cam, sizeof cam, "%s/%s", dir, nome);
  assert(SDL_SaveBMP(s, cam) == 0); SDL_FreeSurface(s); printf("captura: %s\n", cam);
}
// O quadro do Frost com "Vidro fosco": a luz da paleta do titulo (verde)
// preparada como antes era assada para o fosco, a forca devolvida a 0, e o
// fundo de tela cheia pintado por cima.
static void quadro(float fundo) {
  static const float verde[4][3] = { {0.10f,0.55f,0.12f}, {0.20f,0.70f,0.18f},
                                     {0.05f,0.45f,0.10f}, {0.15f,0.60f,0.15f} };
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glDisable(GL_SCISSOR_TEST);
  memcpy(nv_ambiente_viva, verde, sizeof verde);
  nv_ambiente_forca = 1.0f; nv_tempo_viva = 0.0f;
  gfx_ambiente_preparar();
  nv_ambiente_forca = 0.0f;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, fundo, fundo, fundo, 1.0f);
}
static void neutro(const char *nome, double m[3]) {
  double d = m[1] - (m[0] + m[2]) * 0.5;
  char t[200];
  snprintf(t, sizeof t, "%s neutro: media %.1f,%.1f,%.1f  G-(R+B)/2 = %+.2f", nome, m[0], m[1], m[2], d);
  confere(fabs(d) <= 2.0, t);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : NULL;
  GfxRect painel = { 1240, 140, 560, 760 }, folha = { 160, 140, 560, 760 };
  double m[3], gray[3] = {0};
  int k;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("vidro", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco());

  // Dois quadros por fundo: o segundo reaproveita o backdrop do primeiro.
  for (k = 0; k < 4; k++) {
    float fundo = k < 2 ? 0.5f : 1.0f;
    const char *nf = k < 2 ? "cinza" : "branco";
    char n[80];
    quadro(fundo);
    gfx_vidro_painel(painel, 36.0f / painel.h, 0.80f, 1.0f);
    gfx_vidro_folha(folha, 36.0f / folha.h, 1.0f);
    snprintf(n, sizeof n, "painel/%s/q%d", nf, k & 1);
    media((int)painel.x + 40, (int)painel.y + 40, (int)painel.w - 80, (int)painel.h - 80, m); neutro(n, m);
    if (k == 1) memcpy(gray, m, sizeof gray);
    if (k == 2) confere(m[1] > gray[1] + 20.0, "hard cut updates the frost on the same frame");
    snprintf(n, sizeof n, "folha/%s/q%d", nf, k & 1);
    media((int)folha.x + 40, (int)folha.y + 40, (int)folha.w - 80, (int)folha.h - 80, m); neutro(n, m);
    if (k == 0) salvar(dir, "vidro-cinza.bmp");
    if (k == 2) salvar(dir, "vidro-branco.bmp");
  }

  // Settle a small region, then open a distant panel after foreground drew.
  // Both the miss and the following regional frame must use backdrop only.
  {
    GfxRect small = { 100, 40, 300, 90 }, expanded = { 1100, 250, 600, 600 };
    double miss[3], settled[3];
    for (int i = 0; i < 35; i++) {
      if (i == 34) gfx_vidro_sujar();
      quadro(.5f);
      gfx_vidro_painel(small, .3f, .8f, 1);
      if (i == 34) confere(gfx_vidro_capturas == 1 && gfx_vidro_fill > 0 && gfx_vidro_fill < .1,
                           "neutral test reaches regional blur before the miss");
    }
    for (int i = 0; i < 2; i++) {
      quadro(.5f);
      gfx_vidro_painel(small, .3f, .8f, 1);
      gfx_cor((GfxRect){1000, 200, 800, 800}, 0, 0, 1, 0, 1);
      gfx_vidro_painel(expanded, .06f, .8f, 1);
      media(1250, 400, 200, 200, i ? settled : miss);
    }
    neutro("regional miss excludes foreground", miss);
    confere(fabs(miss[1] - settled[1]) <= 1.0, "region expansion has no source pop");
  }

  // O vidro desfoca o que esta atras: metade esquerda preta, direita branca,
  // a borda em x = 960 no meio do painel. Dentro do painel a luz sobe em
  // rampa dos dois lados da borda (degrade), nao num degrau so em x = 960.
  {
    GfxRect p = { 760, 300, 400, 480 };
    double v[4][3];
    int i, sobe = 1;
    quadro(0.0f);
    // Full-screen backdrop draw clipped to its white half. A half-size COR
    // draw is foreground and would close the backdrop before painting white.
    gfx_recorte(960, 0, 960, 1080);
    gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, 1, 1, 1, 1.0f);
    gfx_sem_recorte();
    gfx_vidro_painel(p, 36.0f / p.h, 0.30f, 1.0f);
    for (i = 0; i < 4; i++) media(944 + 10 * i, 500, 2, 40, v[i]);
    for (i = 0; i < 3; i++) sobe &= v[i + 1][1] > v[i][1] + 2.0;
    printf("borda sob o vidro: %.1f %.1f | %.1f %.1f\n", v[0][1], v[1][1], v[2][1], v[3][1]);
    confere(sobe, "o vidro desfoca o backdrop real (a borda vira degrade)");
    salvar(dir, "vidro-borda.bmp");
  }
  // Top bar, glass on + frost OFF: still see-through. Left half black, right
  // half white behind a bar spanning x = 960; opaque draws both halves equal.
  {
    GfxRect b = { 560, 40, 800, 96 };
    double e[3], d[3];
    setenv("NUVIO_SHOT_VIDRO_FOSCO", "0", 1); ajustes_teste_vidro_env();
    assert(ajustes_vidro() && !ajustes_vidro_fosco());
    quadro(0.0f);
    gfx_cor((GfxRect){ 960, 0, 960, 1080 }, 0, 1, 1, 1, 1.0f);
    gfx_chrome(b, 0.5f, 1.0f);
    media(700, 70, 40, 30, e); media(1180, 70, 40, 30, d);
    printf("barra fosco off: esq %.1f dir %.1f\n", e[1], d[1]);
    confere(d[1] > e[1] + 12.0, "barra do topo, vidro sem fosco, deixa o fundo passar");
    confere(e[1] < 60.0 && d[1] < 200.0, "barra sem fosco segue escura o bastante para o texto");
    salvar(dir, "barra-fosco-off.bmp");
  }
  printf("vidro_neutro: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
