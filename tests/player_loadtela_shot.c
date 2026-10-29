// A3 LOADING SCREEN (dono, 16/10): three moments of the detail -> opening
// morph (mid-morph, end of morph, loading). Out of the suite (GL window):
//
//   bash tests/player_loadtela_shot.sh /Volumes/ExternalSSD/tmp/loadtela/depois
//
// A second arg "canto" shoots the B3 style (logo bottom-left, ring under it).

// Builds against an older tree too (before/after): player_morph_de is weak.
#include "catalogo.h"
#include "player.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "artehero.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void player_morph_de(GfxRect logo, GfxRect play) __attribute__((weak));
static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;

static void salvar(const char *dir, const char *nome) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char caminho[700];
  int y; unsigned char *p = s->pixels, *t = malloc((size_t)s->pitch);
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  snprintf(caminho, sizeof caminho, "%s/%s", dir, nome);
  assert(IMG_SavePNG(s, caminho) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", caminho);
}

static void quadro(void) {
  Uint32 agora = SDL_GetTicks();
  SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
  player_atualizar(1.f / 60, agora);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  player_desenhar(agora);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/loadtela/depois";
  static const struct { Uint32 ms; const char *nome; } M[3] = {
    { 350, "1-morph.png" }, { 800, "2-fim-morph.png" }, { 2500, "3-carregando.png" } };
  CatItem c;
  Uint32 t0;
  int i, k;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: loadtela", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\n");
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (argc > 2 && !strcmp(argv[2], "canto")) assert(ajustes_shot_valor("aberturaEstiloLocal", 1));

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
  snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
  snprintf(c.logo, sizeof c.logo, "deploy/app/art/logo/04.png");
  cat_definir(&c, 1);
  // Art and logo already decoded, as on the title page right before Play.
  for (k = 0; k < 400; k++) {
    const char *lg = artehero_logo_sessao(&c);
    int ok = tex_obter_hero(c.backdrop) && lg && tex_obter_larg_qualquer(lg, 640);
    tex_bombear(8); SDL_Delay(5);
    if (ok) break;
  }
  // The title page's logo and Play pill (detail.c hero layout, 1920x1080).
  if (player_morph_de) player_morph_de((GfxRect){ 96, 360, 500, 200 }, (GfxRect){ 96, 640, 321, 94 });
  player_abrir(0, NULL);
  player_shot_carregando(1);
  t0 = SDL_GetTicks();
  for (i = 0; i < 3; i++) {
    do { quadro(); SDL_Delay(4); } while (SDL_GetTicks() - t0 < M[i].ms);
    salvar(saida, M[i].nome);
  }
  puts("player_loadtela_shot: ok");
  return 0;
}
