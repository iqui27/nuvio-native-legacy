// Decodifica um WebP real (capa de colecao do CDN do Xperience) pela libwebp
// do sistema, sem SDL_image.
#include "../src/webp.h"
#include <assert.h>
#include <stdio.h>
int main(int argc, char **argv) {
  SDL_Surface *s = webp_carregar(argc > 1 ? argv[1] : "tests/amostra.webp");
  assert(s && s->w > 0 && s->h > 0 && s->format->format == SDL_PIXELFORMAT_ABGR8888);
  printf("ok  webp %dx%d\n", s->w, s->h);
  // REDUZIDO NO DECODER: pedido a 320 sai 320 de largura, com a altura na
  // proporcao, e diz o tamanho do arquivo (1477x980 na amostra).
  { int ow = 0, oh = 0;
    SDL_Surface *r = webp_carregar_larg(argc > 1 ? argv[1] : "tests/amostra.webp", 320, &ow, &oh);
    assert(r && r->w == 320 && ow == s->w && oh == s->h);
    assert(r->h == (oh * 320 + ow / 2) / ow);
    { unsigned char *p = (unsigned char *)r->pixels + (r->h / 2) * r->pitch + (r->w / 2) * 4;
      printf("ok  webp reduzido %dx%d (arquivo %dx%d) pixel central rgba=%d,%d,%d,%d\n",
             r->w, r->h, ow, oh, p[0], p[1], p[2], p[3]);
      assert(p[3] == 255); }
    SDL_FreeSurface(r);
    // Limite maior que o arquivo: sai inteiro.
    r = webp_carregar_larg("tests/amostra.webp", 4000, &ow, &oh);
    assert(r && r->w == ow && r->h == oh);
    SDL_FreeSurface(r); }
  assert(webp_carregar("tests/webp.c") == NULL);   // nao e WebP: NULL, sem alarde
  SDL_FreeSurface(s);
  puts("webp: tudo ok");
  return 0;
}
