// Exercise the real frame pipeline: Vivid identity, pastel hue/lightness/chroma,
// contrast, neutral fallback, and no change to other dynamic themes.
#include "corviva.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar_leve(const char *n, const char *c) { (void)n; (void)c; return 1; }
#ifndef CORVIVA_SIMPLES_CLARO
#define CORVIVA_SIMPLES_CLARO 7
#endif

static int failures;
#define CHECK(c, message) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)

int main(void) {
  static const float colors[][3] = {
    {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0},
    {0, 1, 1}, {1, 0, 1}, {.8f, .3f, .1f}, {.3f, .2f, .6f}
  };
  const float ink[3] = {18 / 255.f, 19 / 255.f, 22 / 255.f};
  for (unsigned i = 0; i < sizeof colors / sizeof *colors; i++) {
    CorvivaPaleta p = {0};
    CorvivaTokens tokens;
    float vivid[3], light[3], a[3], b[3], ca, cb;
    char key[40];
    snprintf(key, sizeof key, "artepastel-%u", i);
    p.ok = 1;
    corviva_da_arte(colors[i], p.acento);
    corviva_anotar(key, &p);
    corviva_definir(key, CORVIVA_HOME);
    corviva_quadro(1, CORVIVA_SIMPLES, 0, 1);
    corviva_quadro(1, CORVIVA_SIMPLES, 0, 1);
    corviva_acento(&vivid[0], &vivid[1], &vivid[2]);
    CHECK(!memcmp(vivid, p.acento, sizeof vivid), "Vivid must be bit-identical to today's art color");
    corviva_quadro(1, CORVIVA_SIMPLES_CLARO, 0, 1);
    corviva_acento(&light[0], &light[1], &light[2]);
    corviva_srgb_para_oklab(vivid, a);
    corviva_srgb_para_oklab(light, b);
    ca = hypotf(a[1], a[2]); cb = hypotf(b[1], b[2]);
    CHECK(b[0] > a[0] + .02f, "Light must raise perceptual lightness");
    CHECK(cb < ca * .6f, "Light must lower chroma to pastel");
    CHECK(fabsf(atan2f(a[2], a[1]) - atan2f(b[2], b[1])) < .025f, "Light must keep hue");
    corviva_tokens(light, &tokens);
    CHECK(!tokens.tintaBranca && corviva_contraste(light, ink) >= 4.5f, "Pastel must retain readable dark text");
    corviva_quadro(1, CORVIVA_SIMPLES, 0, 1);
    CHECK(!memcmp(vivid, nv_acento_viva, sizeof vivid), "Returning to Vivid must restore exact color");
    corviva_quadro(1, CORVIVA_IMERSIVA, 0, 1);
    CHECK(!memcmp(vivid, nv_acento_viva, sizeof vivid), "Other themes must retain exact color");
  }
  { CorvivaPaleta p = {0};
    float vivid[3];
    corviva_anotar("neutral", &p);
    corviva_definir("neutral", CORVIVA_HOME);
    corviva_quadro(1, CORVIVA_SIMPLES, 0, 1);
    corviva_quadro(1, CORVIVA_SIMPLES, 0, 1);
    memcpy(vivid, nv_acento_viva, sizeof vivid);
    corviva_quadro(1, CORVIVA_SIMPLES_CLARO, 0, 1);
    CHECK(!memcmp(vivid, nv_acento_viva, sizeof vivid), "Neutral fallback must stay unchanged");
  }
  if (failures) return 1;
  puts("PASS: Vivid identity; pastel hue, lightness, chroma, contrast; neutral and other themes unchanged");
  return 0;
}
