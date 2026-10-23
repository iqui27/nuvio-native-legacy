// Titulo delicado da Central (#205): corpo 22 Medium e cinza com >= 4.5:1 no vidro.
#include "plrilha.h"
#include "text.h"
#include <math.h>
#include <stdio.h>

static double lum(int c) { double v = c / 255.0; return v <= 0.03928 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4); }
static double L(int r, int g, int b) { return 0.2126 * lum(r) + 0.7152 * lum(g) + 0.0722 * lum(b); }

int main(void) {
  double a = L(PLR_TIT_R, PLR_TIT_G, PLR_TIT_B), v = L(20, 22, 26);
  double c = (a + 0.05) / (v + 0.05);
  if (PLR_TIT_ESTILO != TXT_G20M) { puts("FALHA: estilo do titulo"); return 1; }
  if (c < 4.5) { printf("FALHA: contraste %.2f\n", c); return 1; }
  printf("PASS: titulo leve, contraste %.2f\n", c);
  return 0;
}
