// Real Settings and Live TV error surfaces; isolated data, no network workers.
#include "ajustes.h"
#include "aovivo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int ajustes_teste_conexoes(void);

static void save(const char *dir, const char *name) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, 1920, 1080, 24, 0xff, 0xff00, 0xff0000, 0);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, 1920, 1080, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  unsigned char *p = s->pixels, *row = malloc((size_t)s->pitch);
  for (int y = 0; y < 540; y++) {
    memcpy(row, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (1079 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (1079 - y) * s->pitch, row, (size_t)s->pitch);
  }
  char path[1024]; snprintf(path, sizeof path, "%s/%s.png", dir, name);
  assert(IMG_SavePNG(s, path) == 0); SDL_FreeSurface(s); free(row);
  printf("PNG: %s\n", path);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Connection limit proof", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w); SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl);
  GLuint fbo, tex;
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(32);
  gfx_icones_dir("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar(); ajustes_definir_vidro(0);
  ajustes_teste_conexoes();
  for (int surface = 0; surface < 2; surface++) {
    for (int i = 0; i < 60; i++) {
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro();
      glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, 1920, 1080);
      glClearColor(.031f, .035f, .043f, 1); glClear(GL_COLOR_BUFFER_BIT);
      if (!surface) { ajustes_atualizar(1.f / 60, SDL_GetTicks()); ajustes_desenhar(SDL_GetTicks()); }
      else aovivo_erro_desenhar("Xtream", "", i18n("Esta linha está em uso em outra tela"),
          i18n("Feche o Xtream em outro aparelho e tente de novo."), 1);
    }
    save(argv[1], surface ? "line-in-use" : "limited-connections");
  }
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
}
