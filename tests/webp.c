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
  // AVATAR PEQUENO NAS VARIANTES QUE O CDN ENTREGA (log de campo LG 1.4.1-1.4.3,
  // "decode falhou ... magica=52494646"). Fixtures de 64x64 geradas com cwebp e
  // img2webp: circulo vermelho com borda transparente no quadro 1, azul
  // opaco no quadro 2. O animado so tem o quadro 1: centro vermelho, canto
  // transparente — inclusive quando o quadro vem recortado (60x60 em 2,2).
  { static const char *arqs[] = { "vp8l", "alfa", "anim_lossy", "anim_vp8l" };
    unsigned i;
    for (i = 0; i < sizeof arqs / sizeof *arqs; i++) {
      char cam[96]; int ow = 0, oh = 0;
      SDL_Surface *a;
      unsigned char *c, *k;
      snprintf(cam, sizeof cam, "tests/fixtures/webp/%s.webp", arqs[i]);
      a = webp_carregar_larg(cam, 320, &ow, &oh);
      if (!a) { printf("FALHOU  %s nao decodificou\n", cam); return 1; }
      assert(a->w == 64 && a->h == 64 && ow == 64 && oh == 64);
      c = (unsigned char *)a->pixels + 32 * a->pitch + 32 * 4;
      k = (unsigned char *)a->pixels + 1 * a->pitch + 1 * 4;
      printf("ok  %-10s 64x64 centro rgba=%d,%d,%d,%d canto alfa=%d\n", arqs[i], c[0], c[1], c[2], c[3], k[3]);
      assert(c[0] > 200 && c[2] < 60 && c[3] == 255);   // vermelho: quadro 1, nao o 2 azul
      assert(k[3] == 0);
      SDL_FreeSurface(a);
    } }
  assert(webp_carregar("tests/webp.c") == NULL);   // nao e WebP: NULL, sem alarde
  SDL_FreeSurface(s);
  puts("webp: tudo ok");
  return 0;
}
