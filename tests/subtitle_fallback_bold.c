// #637: a Regular-only Arabic fallback must still honor subtitle bold.
#define main previous_test_main
#include "text_arabe_negrito.c"
#undef main

static unsigned long ink(SDL_Surface *s) {
  unsigned long sum = 0;
  assert(s && s->format->format == SDL_PIXELFORMAT_ABGR8888);
  for (int y = 0; y < s->h; y++)
    for (int x = 0; x < s->w; x++)
      sum += ((unsigned char *)s->pixels)[y * s->pitch + x * 4 + 3];
  return sum;
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  gfx_tamanho_alvo(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  // Model the TV: Arabic resolves to Regular and no bundled Bold is available.
  arabeBoldFechar();
  arabeBoldCaminho[0] = 0;
  const char *samples[] = {"مرحبا بالعالم", "مرحبا World 123"};
  for (unsigned i = 0; i < sizeof samples / sizeof *samples; i++) {
    SDL_Surface *r = rasterLinha(TXT_LEG_100, samples[i], (SDL_Color){255,255,255,255}, TXT_FAMILIA_INTER, 0);
    SDL_Surface *b = rasterLinha(TXT_LEG_100, samples[i], (SDL_Color){255,255,255,255}, TXT_FAMILIA_INTER, TXT_ENF_NEGRITO);
    unsigned long ri = ink(r), bi = ink(b);
    fprintf(stderr, "SDL sample %u: regular ink=%lu bold ink=%lu width=%d/%d\n", i, ri, bi, r->w, b->w);
    CHECK(bi > ri);
    CHECK(b->w == larguraLinha(TXT_LEG_100, samples[i], TXT_FAMILIA_INTER, TXT_ENF_NEGRITO));
    CHECK(ink(r) == ri);
    SDL_FreeSurface(r); SDL_FreeSurface(b);
  }
  txt_encerrar(); SDL_Quit();
  return failures != 0;
}
