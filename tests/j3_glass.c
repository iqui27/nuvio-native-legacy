// Borderless islands keep their blurred source while entering and leaving.
#include "gfx.h"
#include "ajustes.h"
#include "tela21.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern void ajustes_teste_vidro_env(void);

int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Island frost", 0, 0, 64, 64,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  GLuint texture, fbo;
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  ajustes_definir_vidro(1);
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro_fosco());
  for (int frame = 0; frame < 4; frame++) gfx_novo_quadro();
  const float alpha[] = { .4f, .9966f, 1.f };
  for (int i = 0; i < 3; i++) {
    gfx_novo_quadro();
    glClearColor(.7f, .5f, .3f, 1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_cor((GfxRect){0, 0, 960, 1080}, 0, .1f, .2f, .3f, 1);
    gfx_opacidade_grupo = alpha[i];
    t21_ilha((GfxRect){380, 140, 1180, 834}, 36, 1);
    printf("island alpha %.4f: blurred=%d flat=%d\n", alpha[i],
           gfx_vidro_com_fonte, gfx_vidro_chapados);
    assert(gfx_vidro_com_fonte == 1);
    // A nested pill must reuse flat compositing, not capture its parent.
    gfx_vidro_painel((GfxRect){432, 634, 260, 56}, .5f, .55f, 1);
    assert(gfx_vidro_com_fonte == 1 && gfx_vidro_chapados == 1);
    assert(glGetError() == GL_NO_ERROR);
    gfx_opacidade_grupo = 1;
  }
  puts("Island entry/settled frost and nested glass: PASS");
  return 0;
}
