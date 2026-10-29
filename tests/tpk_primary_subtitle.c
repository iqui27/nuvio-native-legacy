/* Offline .tpk primary renderer: real parsing, layout, fonts and GL output. */
#define NV_TPK
#include "../src/player.c"
#include <assert.h>

void test_primary_native_wait(int waiting);
static unsigned char pixels[1920 * 1080 * 4];
static int visible(void) {
  glFinish();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  int n = 0;
  for (size_t i = 0; i < sizeof pixels; i += 4)
    if (pixels[i] > 100 && pixels[i + 1] > 100 && pixels[i + 2] > 100) n++;
  return n;
}
int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("subtitle regression", 0, 0, 1920, 1080,
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  glViewport(0, 0, 1920, 1080);
  entrada = 1; posSeg = 2; comVideo = 1;
  legEstilo = (VideoLegendaEstilo){120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER};
  const char *docs[] = {
    "1\n00:00:01,000 --> 00:00:03,000\nPrimary plain subtitle\n\n",
    "1\n00:00:01,000 --> 00:00:03,000\n<i>Primary</i> <b>tagged subtitle</b>\n\n",
    "WEBVTT\n\n00:01.000 --> 00:03.000\nPrimary VTT subtitle\n\n"
  };
  for (int embedded = 0; embedded <= 1; embedded++)
  for (unsigned i = 0; i < sizeof docs / sizeof *docs; i++) {
    test_primary_native_wait(embedded);
    legenda_definir_corpo(docs[i]);
    txt_novo_quadro();
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    float top = -1;
    desenharLegendaPrincipal(&top);
    int n = visible();
    printf("primary %s document %u: %d visible pixels\n", embedded ? "embedded" : "addon", i, n);
    assert(n > 100);
  }
  puts("tpk primary subtitles: PASS");
}
