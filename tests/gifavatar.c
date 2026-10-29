// #621: real GIF decode, picker draws, and source UVs through the avatar shader.
#include "gfx.h"
#include "gif.h"
static void observeRect(GfxRect, GLuint, GfxModo, float, float, float,
                        float, float, float, float, float);
static GLuint heldFrame;
static GLuint firstFrame(const char *path, int width) {
  if (!heldFrame) heldFrame = gif_textura(path, width);
  return heldFrame;
}
#define gfx_rect observeRect
#define gif_textura firstFrame
#include "../src/perfilsel.c"
#undef gif_textura
#undef gfx_rect
#define main unusedPickerVisualMain
#include "perfilsel_visual.c"
#undef main

float gifavatar_menu_aspect(void);
static int watching, seen;
static float drawnAspect;
static void observeRect(GfxRect r, GLuint tex, GfxModo mode, float f, float x, float y,
                        float radius, float red, float green, float blue, float alpha) {
  if (watching && tex && mode == GFX_AVATAR) {
    drawnAspect = gfx_tex_aspect_atual;
    seen++;
  }
  gfx_rect(r, tex, mode, f, x, y, radius, red, green, blue, alpha);
}

static int sourceRect(GLuint uvTexture, int w, int h, float aspect) {
  const int points[4][2] = {{4, 128}, {251, 128}, {128, 4}, {128, 251}};
  int bad = 0;
  float side = w < h ? w : h;
  gfx_tex_aspect_atual = aspect;
  gfx_rect((GfxRect){0, 0, 256, 256}, uvTexture, GFX_AVATAR,
           0, 0, 0, 0, 1, 1, 1, 1);
  gfx_tex_aspect_atual = 0;
  for (int i = 0; i < 4; i++) {
    unsigned char pixel[4];
    int x = points[i][0], y = points[i][1];
    glReadPixels(x, 1079 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    float u = (w - side) / (2.0f * w) + (x + 0.5f) / 256 * side / w;
    float v = (h - side) / (2.0f * h) + (y + 0.5f) / 256 * side / h;
    if (fabsf(pixel[0] / 255.0f - u) > 0.012f ||
        fabsf(pixel[1] / 255.0f - v) > 0.012f) bad++;
  }
  printf("%s %dx%d source rect expected=(%.1f,%.1f,%.1f,%.1f), aspect=%g\n",
         bad ? "FAIL" : "PASS", w, h, (w-side)/2, (h-side)/2, side, side, aspect);
  return bad != 0;
}

int main(int argc, char **argv) {
  const char *samples = getenv("NUVIO_GIF621_DIR");
  const char *out = getenv("NUVIO_GIFAVATAR_PNG_DIR");
  const char *phase = argc > 1 ? argv[1] : "after";
  char cache[4096], wide[1024], tall[1024], png[1200];
  int bad = 0;
  if (!samples) samples = "/Volumes/ExternalSSD/tmp/rev/gif621";
  if (!out) out = "/Volumes/ExternalSSD/tmp/rev/gifavatar-png";
  snprintf(wide, sizeof wide, "%s/a1-500x207.gif", samples);
  snprintf(tall, sizeof tall, "%s/a2-280x498.gif", samples);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("GIF avatar regression", SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920, 1080);
  glViewport(0, 0, 1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  dados_iniciar(NULL);
  assert(getenv("NUVIO_TESTE_DIR") && !strcmp(dados_dir(), getenv("NUVIO_TESTE_DIR")));
  snprintf(cache, sizeof cache,
           "1\t0\t1\t0\t#1E88E5\tWide GIF 500x207\t%s\t\n"
           "2\t0\t0\t0\t#E53935\tTall GIF 280x498\t%s\t\n", wide, tall);
  escreverCache(cache, 1);
  perfis_carregar_ativo();
  ajustesDeTeste(0);
  perfilsel_iniciar();
  // Freeze on the first decoded frame for comparable before/after PNGs.
  capturaQuadros = 120;
  const char *paths[] = {wide, tall};
  const int widths[] = {500, 280}, heights[] = {207, 498};
  float aspects[2][3];
  for (int i = 0; i < 2; i++) {
    if (i) tecla(SDLK_RIGHT);
    snprintf(png, sizeof png, "%s/%s-%s.bmp", out, phase, i ? "tall" : "wide");
    captura(png);
    assert(gifTex && tex_aspecto(paths[i]) > 0);
    const ContaPerfil *p = perfis_item(i);
    for (int focused = 0; focused < 2; focused++) {
      watching = 1; seen = 0;
      disco((GfxRect){500, 400, 256, 256}, p, 1, 1, focused);
      watching = 0;
      assert(seen == 1);
      aspects[i][focused] = drawnAspect;
      assert(gfx_tex_aspect_atual == 0);
    }
    perfis_definir_ativo(i + 1);
    aspects[i][2] = gifavatar_menu_aspect();
    gif_parar(); heldFrame = 0;
  }
  // Encode source coordinates, then inspect actual production shader sampling.
  unsigned char uv[256 * 256 * 4];
  for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
    int k = (y * 256 + x) * 4;
    uv[k] = x; uv[k+1] = y; uv[k+2] = 0; uv[k+3] = 255;
  }
  GLuint uvTexture;
  glGenTextures(1, &uvTexture); glBindTexture(GL_TEXTURE_2D, uvTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, uv);
  for (int i = 0; i < 2; i++) for (int focused = 0; focused < 3; focused++) {
    printf("%s: ", focused == 2 ? "top bar" : focused ? "animated" : "static");
    bad += sourceRect(uvTexture, widths[i], heights[i], aspects[i][focused]);
  }
  bad += sourceRect(uvTexture, 256, 256, 1);
  glDeleteTextures(1, &uvTexture);
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  return bad ? 1 : 0;
}
