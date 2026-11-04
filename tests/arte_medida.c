// Artwork draw measurement (src/arte_medida.c): opt-in default off, the
// decoded-vs-physical math with layer scale, cover crop, card contain and
// clipping, shader aspect vs non-uniform target scale, sub-rect strips
// excluded, bounded cache lookups on misses, invalid input, dedup, and the
// hard process cap.
#include "arte_medida.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas, buscas;
#define OK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

// Fake cache: texture name -> decoded size, counting lookups.
static int dims(unsigned tex, int *w, int *h) {
  buscas++;
  if (tex == 7) { *w = 640; *h = 360; return 1; }   // 16:9 card art
  if (tex == 8) { *w = 300; *h = 450; return 1; }   // 2:3 poster
  if (tex == 9) { *w = 0; *h = 360; return 1; }     // invalid decoded size
  if (tex >= 20 && tex < 40) { *w = 100; *h = 100; return 1; }
  return 0;                                         // text/material: not in cache
}

static const float inteiro[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

// asp: the shader uAspect (layout w/h). sub: the uSub window.
static int amostraA(unsigned tex, int modo, float tasp, int contem, float asp, const float *sub,
                    float x0, float y0, float x1, float y1, const int *clip, float esc, char *out) {
  out[0] = 0;
  return arte_medida_amostra(tex, modo, tasp, contem, asp, sub, x0, y0, x1, y1, clip, 1920, 1080,
                             esc, 2.0f, dims, out, 512);
}
// Uniform target scale: the shader aspect equals the physical quad aspect.
static int amostra(unsigned tex, int modo, float tasp, int contem, float x0, float y0,
                   float x1, float y1, const int *clip, float esc, char *out) {
  return amostraA(tex, modo, tasp, contem, (x1 - x0) / (y1 - y0), inteiro, x0, y0, x1, y1, clip, esc, out);
}

int main(void) {
  char l[512];
  unsetenv("NV_ART_DRAW_MEASURE");
  OK(arte_medida_resolver() == 0);
  OK(arte_medida_ligada == 0);
  setenv("NV_ART_DRAW_MEASURE", "0", 1);
  OK(arte_medida_resolver() == 0);
  setenv("NV_ART_DRAW_MEASURE", "1", 1);
  OK(arte_medida_resolver() == 1);

  // gfx_rect_furos strip (uSub not 0,0,1,1): excluded before any lookup and
  // without taking an ordinal, so the first full draw below is still art=1.
  const float faixa[4] = { 0.0f, 0.0f, 0.5f, 1.0f };
  buscas = 0;
  OK(!amostraA(8, 1, 300.0f / 450.0f, 0, 960.0f / 540.0f, faixa, 0, 0, 960, 540, NULL, 1.5f, l) && !l[0]);
  OK(buscas == 0);

  // Cover: 2:3 poster in a 16:9 quad of 960x540 px. The quad crops the
  // height: 300 texel width visible, 300/(16/9)=168.75 texel rows visible.
  OK(amostra(8, 1, 300.0f / 450.0f, 0, 0, 0, 960, 540, NULL, 1.5f, l));
  printf("%s", l);
  OK(strstr(l, "dst=960x540 vis=960x540 rt=1920x1080 tex=300x450 fit=1 src=300x169 "
                "px_per_texel_milli=3200x3200 layer_milli=1500 px_per_unit_milli=2000") != NULL);
  OK(strstr(l, "art=1 ") != NULL && strstr(l, "mode=1 ") != NULL);
  OK(!strstr(l, "http") && !strstr(l, "hash"));
  OK(!strstr(l, "cover") && !strstr(l, "contain") && !strstr(l, "stretch"));

  // Same resource, same size: deduped, no cache lookup, no line.
  buscas = 0;
  OK(!amostra(8, 1, 300.0f / 450.0f, 0, 0, 0, 960, 540, NULL, 1.5f, l) && !l[0]);
  OK(buscas == 0);
  // Small change (<25%) also deduped; >=25% emits again with the same ordinal.
  OK(!amostra(8, 1, 300.0f / 450.0f, 0, 0, 0, 1100, 540, NULL, 1.5f, l));
  OK(amostra(8, 1, 300.0f / 450.0f, 0, 0, 0, 480, 270, NULL, 1.5f, l));
  OK(strstr(l, "art=1 ") && strstr(l, "dst=480x270 "));

  // Contain allowed but the aspect matches (ratio 1): stays cover. The quad
  // is clipped by the target (x 1800..2760 -> ..1920) and a scissor (..1900).
  int clip[4] = { 0, 0, 1900, 1080 };
  OK(amostra(7, 2, 640.0f / 360.0f, 1, 1800, 0, 2760, 540, clip, 1.0f, l) == 1);
  printf("%s", l);
  OK(strstr(l, "dst=960x540 vis=100x540 ") && strstr(l, "fit=1 src=640x360 px_per_texel_milli=1500x1500"));
  OK(strstr(l, "art=2 "));
  // Card contain: 2:3 poster in a 16:9 card (ratio 2.67 > 1.25) is fitted
  // inside: displayed 360x540, whole texture visible.
  OK(amostra(8, 3, 300.0f / 450.0f, 1, 0, 0, 960, 540, NULL, 1.0f, l) == 1);
  printf("%s", l);
  OK(strstr(l, "fit=2 src=300x450 px_per_texel_milli=1200x1200"));
  // Aspect 0 = stretched mapping: independent x/y factors.
  OK(amostra(7, 4, 0.0f, 0, 0, 0, 1280, 1080, NULL, 1.0f, l) == 1);
  OK(strstr(l, "fit=0 src=640x360 px_per_texel_milli=2000x3000"));

  // Non-uniform target (ex=2, ey=1): a 960x540 layout card (uAspect 16:9)
  // covers 1920x540 px. The shader sees ratio 1 with 16:9 art: no crop and no
  // contain. The physical quad aspect (3.56) must not flip that.
  OK(amostraA(7, 5, 640.0f / 360.0f, 1, 960.0f / 540.0f, inteiro, 0, 0, 1920, 540, NULL, 1.0f, l) == 1);
  printf("%s", l);
  OK(strstr(l, "dst=1920x540 vis=1920x540 rt=1920x1080 tex=640x360 fit=1 src=640x360 "
                "px_per_texel_milli=3000x1500 ") != NULL);
  // Same target, 2:3 poster as cover (no card rule): shader ratio 2.67 crops
  // the height to 300/(16/9)=169 rows; whole image 1920 x 540*2.67 px.
  OK(amostraA(8, 6, 300.0f / 450.0f, 0, 960.0f / 540.0f, inteiro, 0, 0, 1920, 270, NULL, 1.0f, l) == 1);
  OK(strstr(l, "fit=1 src=300x169 px_per_texel_milli=6400x1600 ") != NULL);

  // Invalid values: no line, no slot consumed.
  OK(!amostra(9, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!amostra(5, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));     // not a cache texture
  OK(!amostra(7, 1, 1.0f, 0, 10, 0, 10, 100, NULL, 1.0f, l));     // empty quad
  OK(!amostra(7, 1, 0.0f / 0.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!amostraA(7, 1, 1.0f, 0, 0.0f / 0.0f, inteiro, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!amostra(0, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!arte_medida_amostra(7, 1, 1.0f, 0, 1.0f, inteiro, 0, 0, 100, 100, NULL, 0, 1080, 1, 1, dims, l, sizeof l));

  // Persistent misses: each failing name is looked up once, every frame after
  // that costs no lookup (the invalid-size name 9 counts as a miss too).
  buscas = 0;
  for (int i = 0; i < 1000; i++) {
    amostra(5, 1, 1.0f, 0, 0, 0, 100 + i % 300, 100, NULL, 1.0f, l);
    amostra(9, 1, 1.0f, 0, 0, 0, 100 + i % 300, 100, NULL, 1.0f, l);
  }
  OK(buscas == 0);   // both already missed above
  // Many unique unrecognized names: total lookups stay under the process
  // budget, then the probe turns itself off (fast path again).
  buscas = 0;
  for (unsigned t = 1000; t < 100000; t++) amostra(t, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l);
  OK(buscas > 0 && buscas <= ARTE_MEDIDA_BUSCAS);
  OK(arte_medida_ligada == 0);

  // Hard cap: at most ARTE_MEDIDA_TETO lines per process, then the probe
  // turns itself off so gfx_rect is back to one int compare.
  OK(arte_medida_resolver() == 1);
  int linhas = 0;
  buscas = 0;
  for (int i = 0; i < 200; i++)
    linhas += amostra(20 + i % 20, 1, 1.0f, 0, 0, 0, (float)(i / 20 % 2 ? 200 : 100), 100, NULL, 1.0f, l);
  OK(linhas == ARTE_MEDIDA_TETO);
  OK(buscas <= ARTE_MEDIDA_BUSCAS);
  OK(arte_medida_ligada == 0);
  OK(!amostra(8, 1, 1.0f, 0, 0, 0, 77, 77, NULL, 1.0f, l));

  if (falhas) { printf("arte_medida: %d FAIL\n", falhas); return 1; }
  printf("arte_medida: PASS\n");
  return 0;
}
