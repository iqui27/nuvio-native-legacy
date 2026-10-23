/* libass regression guard, not a reproduction of webOS issue #484.
 * Reuse the font/GL fixture and count actual submissions without a GL context. */
#define main ass_pisca_fixture_main
#define gfx_rect fixtureRect
#include "ass_pisca.c"
#undef gfx_rect
#undef main
#include <assert.h>

static int submitted;
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float px, float py,
              float raio, float cr, float cg, float cb, float alpha) {
  fixtureRect(r, tex, modo, foco, px, py, raio, cr, cg, cb, alpha);
  assert(tex && alpha > .99f);
  submitted++;
}

int main(void) {
  const char *doc = CAB
    "Dialogue: 0,0:00:01.00,0:00:03.00,Default,,0,0,0,,Early\n"
    "Dialogue: 0,0:05:00.00,0:05:02.00,Default,,0,0,0,,Five minutes\n"
    "Dialogue: 0,0:25:00.00,0:25:02.00,Default,,0,0,0,,Twenty five minutes\n";
  size_t len;
  char *font = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &len);
  assert(font);
  assrender_geracao(484);
  assert(assrender_adicionar_fonte("InterDisplay-Regular.ttf", font, len));
  free(font);
  assert(assrender_carregar(doc, strlen(doc), 484));
  for (int second = 0; second <= 1501; second++) {
    submitted = 0;
    assrender_desenhar(second, 0, 1, 0, 0, 1920, 1080);
    /* Allow the real worker to consume the request. A long logical gap does
     * not mean the render thread or worker is put to sleep. */
    usleep(1000);
    if (second == 2 || second == 301 || second == 1501) {
      for (int retry = 0; retry < 100; retry++) {
        submitted = 0;
        assrender_desenhar(second, 0, 1, 0, 0, 1920, 1080);
        if (submitted) break;
        usleep(1000);
      }
      printf("libass at %ds: %d submitted images\n", second, submitted);
      assert(submitted > 0);
    }
  }
  assrender_geracao(485);
  assrender_limpar();
  puts("PASS: libass external cue regression guard (not #484 reproduction)");
}
