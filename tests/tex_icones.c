// Formatter icons through the shared texture cache (tex_obter_icone).
// The sheet showed no icons for a while because a package icon waited like
// network art: no slot while nMax/3 items were in flight, FIFO in the decode
// queue, and 3 uploads per frame for the whole app. This fills the in-flight
// cap with decoded posters that are never uploaded (tex_bombear(0)), then
// asks for 24 icons: they must all be ready within a few frames, while a
// plain tex_obter_larg request for the same kind of file still gets no slot.
#include <SDL.h>
#include <SDL_image.h>
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "tex_cache.h"
#include "gfx.h"

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#ifdef NV_LINUX_DESKTOP
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
  SDL_Window *win = SDL_CreateWindow("texicones", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(tex_iniciar(48));                 // in-flight cap: 48/3 = 16
  char posters[16][64], icones[25][300];
  for (int i = 0; i < 16; i++) snprintf(posters[i], sizeof posters[i], "deploy/app/art/%02d.jpg", i);
  int n = 0;
  DIR *d = opendir("deploy/app/art/icones/formatador"); assert(d);
  for (struct dirent *e; (e = readdir(d)) && n < 25;)
    if (!strncmp(e->d_name, "fl_", 3) && strstr(e->d_name, ".png"))
      snprintf(icones[n++], sizeof icones[0], "deploy/app/art/icones/formatador/%s", e->d_name);
  closedir(d); assert(n == 25);

  // The app has drawn art before the sheet opens: the GL upload path is warm.
  for (int f = 0; f < 200 && !tex_obter_larg("deploy/app/art/39.jpg", 212); f++) {
    tex_bombear(3); tex_novo_quadro(); SDL_Delay(5);
  }
  assert(tex_obter_larg("deploy/app/art/39.jpg", 212));
  // 16 posters requested and decoded, never uploaded: the cache is at the cap.
  for (int f = 0; f < 200; f++) {
    for (int i = 0; i < 16; i++) assert(!tex_obter_larg(posters[i], 212));
    tex_novo_quadro(); SDL_Delay(10);
  }
  // Network-style request: no slot while 16 are in flight.
  for (int f = 0; f < 20; f++) {
    assert(!tex_obter_larg(icones[24], 22));
    tex_bombear(0); tex_novo_quadro(); SDL_Delay(5);
  }
  int quadros = -1;
  for (int f = 0; f < 200 && quadros < 0; f++) {
    int prontos = 0;
    for (int i = 0; i < 24; i++) prontos += tex_obter_icone(icones[i], 22) != 0;
    for (int i = 0; i < 16; i++) (void)tex_obter_larg(posters[i], 212);
    if (prontos == 24) quadros = f;
    if (f % 50 == 49) printf("frame %d: %d icons ready\n", f, prontos);
    tex_bombear(0); tex_novo_quadro(); SDL_Delay(5);
  }
  printf("24 icons ready after %d frames with the in-flight cap full\n", quadros);
  assert(quadros >= 0 && quadros <= 20);
  for (int i = 0; i < 16; i++) assert(!tex_obter_larg(posters[i], 212));  // art still waits
  printf("PASS: package icons skip the network in-flight cap, the decode queue and the per-frame upload cap\n");
  tex_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  return 0;
}
