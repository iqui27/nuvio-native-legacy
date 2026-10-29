// Vidro em tudo (dono, 15/10/2026: "varios componentes nao estao em glass,
// estao diferentes um do outro e nem acompanhando o ajuste de opacidade").
// Cada material de superficie compartilhado e desenhado sozinho sobre cinza
// 0,5, com e sem "Vidro fosco", na "Opacidade do vidro" de 60 % e de 92 %:
//  - todos mudam com a opacidade (media do miolo difere >= 3 niveis);
//  - os paineis (material, folha, ou_solido) saem no MESMO tom (<= 2 niveis);
//  - sobre pagina ESCURA (0,05) 60 % e 92 % tambem diferem (>= 8 niveis): o tom
//    segue a opacidade, nao so o alfa;
//  - todos levam a MESMA luz de borda (gfx_vidro_luz): a faixa de cima clareia
//    sobre a de baixo (>= 2 niveis, vidro chapado sobre escuro, 78 %).
// Os pontos de chamada ficam em tests/vidro_todo.sh (grep).
//   bash tests/vidro_todo.sh
#include "gfx.h"
#include "ajustes.h"
#include "plrui.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static GLuint fbo, fboTex;
static const int LW = 1920, LH = 1080;
static const GfxRect R = { 660, 340, 600, 400 };
static int falhas;

static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}
static double mediaEm(int x, int y, int w, int h) {
  int i;
  unsigned char *p = malloc((size_t)w * h * 4);
  double m = 0;
  assert(p);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glReadPixels(x, LH - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, p);
  for (i = 0; i < w * h; i++) m += (p[4*i] + p[4*i+1] + p[4*i+2]) / 3.0;
  free(p);
  return m / (w * h);
}
static double media(void) {
  return mediaEm((int)R.x + 60, (int)(R.y + R.h * 0.5f), (int)R.w - 120, (int)(R.h * 0.5f) - 40);
}
// Faixa de cima menos a de baixo, na mesma coluna (onde a luz de borda entra).
static double luzBorda(void) {
  int x = (int)(R.x + R.w * 0.3f), w = (int)(R.w * 0.2f);
  return mediaEm(x, (int)R.y + 12, w, 28) - mediaEm(x, (int)(R.y + R.h) - 40, w, 28);
}
static void opacidade(const char *o, const char *fosco) {
  setenv("NUVIO_SHOT_VIDRO_OPAC", o, 1); setenv("NUVIO_SHOT_VIDRO_FOSCO", fosco, 1);
  ajustes_teste_vidro_env();
}
enum { MATERIAL, FOLHA, OU_SOLIDO, PLRUI_MATERIAL, PAINEL, PAINEL_ACENTO, BOTAO_REPOUSO, DISCO_OSD, CHIP, N };
static const char *NOME[N] = { "gfx_vidro_material", "gfx_vidro_folha", "gfx_vidro_ou_solido",
  "plrui_material", "gfx_vidro_painel", "gfx_vidro_painel_acento", "plrui_botao_repouso", "plrui_disco_osd", "gfx_vidro_chip" };
static float pag = 0.5f;   // cinza da pagina atras
static double desenhar(int s) {
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  gfx_novo_quadro();
  glClearColor(pag, pag, pag, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, pag, pag, pag, 1.0f);
  switch (s) {
    case MATERIAL:       gfx_vidro_material(R, 28.0f / R.h, GFX_VIDRO_DENS, 1); break;
    case FOLHA:          gfx_vidro_folha(R, 28.0f / R.h, 1); break;
    case OU_SOLIDO:      gfx_vidro_ou_solido(R, 28.0f / R.h, GFX_VIDRO_DENS, 1, .05f, .05f, .06f, .94f); break;
    case PLRUI_MATERIAL: plrui_material(R, 28.0f, 0, 1); break;
    case PAINEL:         gfx_vidro_painel(R, 0.5f, 0.55f, 1); break;
    case PAINEL_ACENTO:  gfx_vidro_painel_acento(R, 0.5f, 0.55f, 1); break;
    case BOTAO_REPOUSO:  plrui_botao_repouso(R, 1); break;
    case DISCO_OSD:      plrui_disco_osd(R, 1); break;
    case CHIP:           gfx_vidro_chip(R, 0.5f, 0.55f, 1); break;
  }
  return media();
}

int main(void) {
  double v[2][3][N], e[2][N], luz[N];
  int s, f, o;
  char t[200];
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

  for (f = 0; f < 2; f++)
    for (o = 0; o < 3; o++) {
      opacidade(o == 0 ? "60" : o == 1 ? "78" : "92", f ? "1" : "0");
      assert(ajustes_vidro());
      for (s = 0; s < N; s++) { desenhar(s); v[f][o][s] = desenhar(s); }   // 2o quadro: backdrop reusado
    }
  pag = 0.05f;
  for (o = 0; o < 2; o++) {
    opacidade(o ? "92" : "60", "1");
    for (s = 0; s < N; s++) { desenhar(s); e[o][s] = desenhar(s); }
  }
  opacidade("78", "0");
  for (s = 0; s < N; s++) { desenhar(s); luz[s] = luzBorda(); }
  for (s = 0; s < N; s++) {
    snprintf(t, sizeof t, "%s segue a opacidade sobre pagina escura: 60%% %.1f / 92%% %.1f", NOME[s],
             e[0][s], e[1][s]);
    confere(fabs(e[0][s] - e[1][s]) >= 8.0, t);
    snprintf(t, sizeof t, "%s com a luz de borda: cima - baixo %.1f", NOME[s], luz[s]);
    confere(luz[s] >= 2.0, t);
  }
  for (f = 0; f < 2; f++) {
    for (s = 0; s < N; s++) {
      snprintf(t, sizeof t, "%s segue a opacidade (fosco %d): 60%% %.1f / 92%% %.1f", NOME[s], f,
               v[f][0][s], v[f][2][s]);
      confere(fabs(v[f][0][s] - v[f][2][s]) >= 3.0, t);
    }
    for (s = FOLHA; s <= OU_SOLIDO; s++) {
      snprintf(t, sizeof t, "%s no tom do material (fosco %d): %.1f vs %.1f", NOME[s], f,
               v[f][1][s], v[f][1][MATERIAL]);
      confere(fabs(v[f][1][s] - v[f][1][MATERIAL]) <= 2.0, t);
    }
  }
  printf("vidro_todo: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
