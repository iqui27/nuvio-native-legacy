// Split hero focused: logo block ends on the side posters' text line, label
// fades out, row runs to the screen edge. Headless: includes src/home.c.
#define main hero_identity_fixture_main
#include "heroidentidade_home.c"
#undef main

float gfx_escala(void) { return 1.0f; }
float gfx_escala_txt(void) { return 1.0f; }
float gfx_escala_tela_txt(void) { return 1.0f; }

int main(void) {
  assert(divididoDireita() >= NV_TELA_W);
  assert(divididoRotuloAlfa(0.0f) == 1.0f && divididoRotuloAlfa(1.0f) == 0.0f);
  const float cardY = 300.0f, yh = 28.0f, ovH = 72.0f, mh = 34.0f;
  float by = cardY + divArteH() + 18.0f, bh = fmaxf(34.0f, yh);
  float fim = divididoFimY(cardY, yh);
  assert(fabsf(fim - (by + (bh - yh) * 0.5f + yh)) < 0.01f);   // side meta text bottom
  const float lead = 36.0f, th = 24.0f;                        // text height != leading
  DivBloco b = divididoBloco(fim, ovH, lead, th, mh);
  assert(fabsf(b.ov + (ovH - lead) + th - fim) < 0.01f);       // last rendered line ends on that line
  assert(divididoBloco(fim, 0, lead, th, mh).ov == fim);       // no overview: no offset
  assert(b.meta + mh < b.ov && b.logoB < b.meta);
  const float ap[] = {5.0f, 2.0f, 1.0f};
  for (int i = 0; i < 3; i++) {
    GfxRect l = divididoLogoRect(104.0f, b.logoB, ap[i]);
    assert(l.x == 104.0f && fabsf(l.y + l.h - b.logoB) < 0.01f);
    assert(l.w <= NV_DIV_LOGO_W + 0.01f && l.h <= NV_DIV_LOGO_H + 0.01f);
    assert(fabsf(l.w / l.h - ap[i]) < 0.001f);
  }
  puts("ok divididosplit: bottom-aligned logo block, label fade, row to screen edge");
  return 0;
}
