// tex_reduzir: media de area, nao vizinho mais proximo.
//
//   bash tests/reduzir.sh
//
// O que se conferia a olho no #54 ("hero lavado e serrilhado") vira numero:
// um xadrez de 1 pixel reduzido tem de virar cinza uniforme; o vizinho mais
// proximo do SDL_BlitScaled devolve 0 ou 255 conforme a coluna que sobrou.
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tex_cache.h"

static int falhas = 0;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHA: " __VA_ARGS__); printf("\n"); } } while (0)

static const unsigned char *px(SDL_Surface *s, int x, int y) {
  return (const unsigned char *)s->pixels + (size_t)y * s->pitch + (size_t)x * 4;
}

int main(void) {
  // 1. Xadrez 1920x1080 em ABGR8888 -> 1280x720: todo pixel perto de 127.
  SDL_Surface *a = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_ABGR8888);
  for (int y = 0; y < a->h; y++)
    for (int x = 0; x < a->w; x++) {
      unsigned char *q = (unsigned char *)a->pixels + (size_t)y * a->pitch + (size_t)x * 4;
      unsigned char v = ((x + y) & 1) ? 255 : 0;
      q[0] = q[1] = q[2] = v; q[3] = 255;
    }
  SDL_Surface *r = tex_reduzir(a, 1280, 720);
  CONFERE(r && r->w == 1280 && r->h == 720, "xadrez: tamanho");
  if (r) {
    int fora = 0;
    for (int y = 0; y < r->h; y++)
      for (int x = 0; x < r->w; x++) {
        const unsigned char *q = px(r, x, y);
        if (q[0] < 60 || q[0] > 195 || q[3] != 255) fora++;
      }
    CONFERE(fora == 0, "xadrez: %d pixels longe do cinza (vizinho mais proximo?)", fora);
    SDL_FreeSurface(r);
  }
  // O que o SDL_BlitScaled faria, para deixar registrado o contraste.
  { SDL_Surface *m = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_ABGR8888);
    SDL_SetSurfaceBlendMode(a, SDL_BLENDMODE_NONE);
    SDL_BlitScaled(a, NULL, m, NULL);
    int extremos = 0;
    for (int x = 0; x < m->w; x++) { const unsigned char *q = px(m, x, 5); if (q[0] == 0 || q[0] == 255) extremos++; }
    printf("SDL_BlitScaled: %d de %d pixels da linha 5 sao 0 ou 255 (vizinho mais proximo)\n", extremos, m->w);
    SDL_FreeSurface(m); }
  SDL_FreeSurface(a);

  // 2. Fonte RGB24 (como sai do decoder JPEG), degrade horizontal 3000 -> 640:
  //    saida monotona e proxima do degrade.
  SDL_Surface *b = SDL_CreateRGBSurfaceWithFormat(0, 3000, 30, 24, SDL_PIXELFORMAT_RGB24);
  for (int y = 0; y < b->h; y++)
    for (int x = 0; x < b->w; x++) {
      unsigned char *q = (unsigned char *)b->pixels + (size_t)y * b->pitch + (size_t)x * 3;
      q[0] = (unsigned char)(x * 255 / 2999); q[1] = 40; q[2] = 200;
    }
  r = tex_reduzir(b, 640, 6);
  CONFERE(r && r->w == 640 && r->h == 6, "rgb24: tamanho");
  if (r) {
    int quebras = 0, pior = 0;
    for (int x = 1; x < r->w; x++) {
      const unsigned char *p0 = px(r, x - 1, 2), *p1 = px(r, x, 2);
      if (p1[0] < p0[0]) quebras++;
      int esperado = x * 255 / 639; int d = abs((int)p1[0] - esperado); if (d > pior) pior = d;
      if (p1[1] != 40 || p1[2] != 200 || p1[3] != 255) quebras += 1000;
    }
    CONFERE(quebras == 0, "rgb24: %d quebras de monotonia/canal", quebras);
    CONFERE(pior <= 2, "rgb24: desvio maximo %d do degrade", pior);
    SDL_FreeSurface(r);
  }
  SDL_FreeSurface(b);

  // 3. Alpha pesado: celula com metade transparente-preta e metade
  //    branca-opaca sai BRANCA com alpha 127, nao cinza.
  SDL_Surface *c = SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_ABGR8888);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) {
      unsigned char *q = (unsigned char *)c->pixels + (size_t)y * c->pitch + (size_t)x * 4;
      if (x < 2) { q[0] = q[1] = q[2] = 0; q[3] = 0; } else { q[0] = q[1] = q[2] = 255; q[3] = 255; }
    }
  r = tex_reduzir(c, 1, 1);
  if (r) {
    const unsigned char *q = px(r, 0, 0);
    CONFERE(q[0] == 255 && q[1] == 255 && q[2] == 255 && (q[3] == 127 || q[3] == 128),
            "alpha: R%d G%d B%d A%d (esperado 255 255 255 127/128)", q[0], q[1], q[2], q[3]);
    SDL_FreeSurface(r);
  } else CONFERE(0, "alpha: NULL");
  SDL_FreeSurface(c);

  // 4. Indexada (PNG com paleta): converte inteira e reduz.
  SDL_Surface *d = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 8, SDL_PIXELFORMAT_INDEX8);
  { SDL_Color pal[2] = { {10, 20, 30, 255}, {110, 120, 130, 255} };
    SDL_SetPaletteColors(d->format->palette, pal, 0, 2);
    memset(d->pixels, 0, (size_t)d->pitch * 8);
    for (int y = 0; y < 8; y++) ((unsigned char *)d->pixels)[(size_t)y * d->pitch + 4] = 1; }
  r = tex_reduzir(d, 2, 2);
  if (r) {
    const unsigned char *q = px(r, 0, 0), *q2 = px(r, 1, 0);
    CONFERE(q[0] == 10 && q[1] == 20 && q[2] == 30, "indexada esquerda: %d %d %d", q[0], q[1], q[2]);
    // direita: 3 de 4 colunas cor 0, 1 de 4 cor 1 -> 10+25=35, 20+25=45, 30+25=55
    CONFERE(q2[0] == 35 && q2[1] == 45 && q2[2] == 55, "indexada direita: %d %d %d", q2[0], q2[1], q2[2]);
    SDL_FreeSurface(r);
  } else CONFERE(0, "indexada: NULL");
  SDL_FreeSurface(d);

  printf(falhas ? "reduzir: %d falhas\n" : "reduzir: ok\n", falhas);
  return falhas ? 1 : 0;
}
