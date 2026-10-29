// #660: four GIF owners, static focus detours, and picker reopening.
#include "gfx.h"
#include "gif.h"
#include "../src/perfilsel.c"
#define main unusedPickerVisualMain
#include "perfilsel_visual.c"
#undef main
int gifperfis_ready(void);
int gifperfis_started(void);

static void draw(const ContaPerfil *p) {
  disco((GfxRect){500, 400, 256, 256}, p, 1, 1, 1);
}
static void waitFrame(void) {
  Uint32 start = SDL_GetTicks();
  while (!gifperfis_ready() && SDL_GetTicks() - start < 2000) SDL_Delay(1);
  assert(gifperfis_ready());
}
static void cycle(const ContaPerfil *p) {
  unsigned char first[4], next[4];
  draw(p);
  if (!gifperfis_started()) waitFrame();
  draw(p); // First draw after decode must upload immediately.
  assert(gifTex && gifperfis_started());
  glBindTexture(GL_TEXTURE_2D, gifTex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, first);
  assert(first[0] == 255 && first[2] == 0);
  waitFrame(); SDL_Delay(25);
  draw(p);
  glBindTexture(GL_TEXTURE_2D, gifTex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, next);
  assert(next[0] == 0 && next[2] == 255);
}
int main(void) {
  // Two 1x1 frames: red then blue, each held for 20 ms.
  static const unsigned char image[] = {
    'G','I','F','8','9','a',1,0,1,0,0x80,0,0,255,0,0,0,0,255,
    0x21,0xf9,4,0,2,0,0,0,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,1,0,
    0x21,0xf9,4,0,2,0,0,0,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x4c,1,0,0x3b
  };
  ContaPerfil avatars[5] = {0};
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  win = SDL_CreateWindow("GIF focus regression", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win);
  assert(gl && gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(16); dados_iniciar(NULL); ajustesDeTeste(0);
  for (int i = 0; i < 4; i++) {
    snprintf(avatars[i].avatarUrl, sizeof avatars[i].avatarUrl, "%s/avatar%d.gif", dados_dir(), i);
    FILE *f = fopen(avatars[i].avatarUrl, "wb"); assert(f);
    assert(fwrite(image, 1, sizeof image, f) == sizeof image); fclose(f);
  }
  for (int pass = 0; pass < 3; pass++) for (int i = 0; i < 4; i++) {
    cycle(&avatars[i]);
    draw(&avatars[4]); // Leave for a profile without a GIF, then return.
    cycle(&avatars[i]);
  }
  perfilsel_iniciar();
  cycle(&avatars[3]); // Reopening on the same owner must restart too.
  gif_parar(); tex_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: four GIF avatars restart and upload on the first draw after decode");
  return 0;
}
