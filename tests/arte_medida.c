// Artwork draw measurement (src/arte_medida.c): opt-in default off, the
// decoded-vs-physical math with layer scale, cover crop, card contain and
// clipping, invalid input, dedup, and the hard process cap.
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
  return 0;                                         // text/material: not in cache
}

static int amostra(unsigned tex, int modo, float asp, int contem, float x0, float y0,
                   float x1, float y1, const int *clip, float esc, char *out) {
  out[0] = 0;
  return arte_medida_amostra(tex, modo, asp, contem, x0, y0, x1, y1, clip, 1920, 1080,
                             esc, 2.0f, dims, out, 512);
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

  // Cover: 2:3 poster in a 16:9 quad of 960x540 px. The quad crops the
  // height: 300 texel width visible, 300/(16/9)=168.75 texel rows visible.
  OK(amostra(8, 1, 300.0f / 450.0f, 0, 0, 0, 960, 540, NULL, 1.5f, l));
  printf("%s", l);
  OK(strstr(l, "dst=960x540 vis=960x540 rt=1920x1080 tex=300x450 fit=cover src=300x169 "
                "px_per_texel_milli=3200x3200 layer_milli=1500 px_per_unit_milli=2000") != NULL);
  OK(strstr(l, "art=1 ") != NULL && strstr(l, "mode=1 ") != NULL);
  OK(!strstr(l, "http") && !strstr(l, "hash"));

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
  OK(strstr(l, "dst=960x540 vis=100x540 ") && strstr(l, "fit=cover src=640x360 px_per_texel_milli=1500x1500"));
  OK(strstr(l, "art=2 "));
  // Card contain: 2:3 poster in a 16:9 card (ratio 2.67 > 1.25) is fitted
  // inside: displayed 360x540, whole texture visible.
  OK(amostra(8, 3, 300.0f / 450.0f, 1, 0, 0, 960, 540, NULL, 1.0f, l) == 1);
  printf("%s", l);
  OK(strstr(l, "fit=contain src=300x450 px_per_texel_milli=1200x1200"));
  // Aspect 0 = stretched mapping: independent x/y factors.
  OK(amostra(7, 4, 0.0f, 0, 0, 0, 1280, 1080, NULL, 1.0f, l) == 1);
  OK(strstr(l, "fit=stretch src=640x360 px_per_texel_milli=2000x3000"));

  // Invalid values: no line, no slot consumed.
  OK(!amostra(9, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!amostra(5, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));     // not a cache texture
  OK(!amostra(7, 1, 1.0f, 0, 10, 0, 10, 100, NULL, 1.0f, l));     // empty quad
  OK(!amostra(7, 1, 0.0f / 0.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!amostra(0, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 1.0f, l));
  OK(!arte_medida_amostra(7, 1, 1.0f, 0, 0, 0, 100, 100, NULL, 0, 1080, 1, 1, dims, l, sizeof l));

  // Hard cap: at most ARTE_MEDIDA_TETO lines per process, then the probe
  // turns itself off so gfx_rect is back to one int compare.
  int linhas = 5;
  for (int i = 0; i < 200; i++)
    linhas += amostra(7, 1, 640.0f / 360.0f, 0, 0, 0, (float)(i % 2 ? 200 : 100), 100, NULL, 1.0f, l);
  OK(linhas == ARTE_MEDIDA_TETO);
  OK(arte_medida_ligada == 0);
  OK(!amostra(8, 1, 1.0f, 0, 0, 0, 77, 77, NULL, 1.0f, l));

  if (falhas) { printf("arte_medida: %d FAIL\n", falhas); return 1; }
  printf("arte_medida: PASS\n");
  return 0;
}
