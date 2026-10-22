// Reuse the headless Home fixture and its stubs; test the actual copy layout
// and identity policy called by desenhaCopiaHero, without a GL window/network.
#define main hero_identity_fixture_main
#include "heroidentidade_home.c"
#undef main

static void ordered(HeroCopyLayout p, float btnH, float btnGap,
                    float captionH, float sinH, float base) {
  // Logo, information, then the action at the bottom (owner 06/10).
  assert(p.logo + p.logoHeight + btnGap <= p.caption + 0.01f);
  if (captionH > 0) assert(p.caption + captionH < p.meta);
  assert(p.meta + NV_LD_HERO_META < p.secondary);
  assert(p.secondary + NV_LD_HERO_SEC < p.synopsis);
  assert(p.synopsis + sinH + btnGap <= p.action + 0.01f);
  assert(fabsf(p.action + btnH - base) < 0.01f);
}

int main(void) {
  static CatItem c;
  snprintf(c.logoIdiomaUrl, sizeof c.logoIdiomaUrl,
           "https://image.tmdb.org/t/p/original/title.png");
  const char *variant = "https://image.tmdb.org/t/p/w500/title.png";
  assert(heroNomeLogo(&c, NULL, 0, "pt") == 1);
  assert(heroNomeLogo(&c, variant, 0, "pt") == 0);
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "und");
  assert(heroNomeLogo(&c, variant, 1, "pt") == 0);
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "pt");
  assert(heroNomeLogo(&c, variant, 0, "pt-BR") == 0);
  assert(heroNomeLogo(&c, variant, 1, "pt-BR") == 0);
  snprintf(c.logoIdioma, sizeof c.logoIdioma, "en");
  assert(heroNomeLogo(&c, variant, 0, "pt") == 1);
  assert(heroNomeLogo(&c, variant, 1, "pt") == 1);
  assert(heroNomeLogo(&c, "https://other.invalid/title.png", 1, "pt") == 0);
  assert(heroNomeLogo(&c, "https://image.tmdb.org/t/p/w500/another.png", 1, "pt") == 0);
  puts("ok identity: missing, pending, matching, unknown and confirmed foreign logos");

  const float bases[] = {NV_SHELF_TOP - NV_HERO_COPY_GAP - 24,
                        NV_PAD_BANNER_H - NV_PAD_TEXTO_BASE,
                        NV_DIN_HERO_H - NV_DIN_TEXTO_BASE};
  const float logos[] = {NV_LOGO_HERO_H, NV_PAD_LOGO_H, NV_DIN_LOGO_H};
  for (int lay = 0; lay < 3; lay++) {
    float bh = NV_HERO_BOTAO_COMPACTO_H;
    float gap = lay == 0 ? NV_HOME_HERO_BOTAO_GAP : 22;
    float sinH = 3 * NV_LD_HERO_SIN;
    float minTop = lay == 2 ? 54 : 24;
    for (int caption = 0; caption < 2; caption++) {
      float hc = caption ? NV_LD_HERO_META : 0;
      HeroCopyLayout p = heroCopyLayout(bases[lay], sinH, 1, 1, hc, logos[lay], bh, gap, minTop, 1.0f, 0);
      ordered(p, bh, gap, hc, sinH, bases[lay]);
      assert(p.logo >= minTop - 0.01f);
      assert(p.logoHeight <= logos[lay]);
      // Real synopsis arrival keeps the bottom boundary and ordered actions.
      HeroCopyLayout empty = heroCopyLayout(bases[lay], 0, 0, 0, 0, logos[lay], bh, gap, minTop, 1.0f, 0);
      assert(fabsf(empty.action + bh - bases[lay]) < 0.01f);
      assert(empty.logo + empty.logoHeight + gap <= empty.action + 0.01f);
    }
  }
  // Moderna without the button: the slot collapses and the block settles on
  // the base; the logo keeps its size and moves down with the text.
  for (int syn = 0; syn < 2; syn++)
    for (int hl = 0; hl < 2; hl++) {
      float lh = hl ? NV_LOGO_HERO_H : 90.0f, sh = syn ? 3 * NV_LD_HERO_SIN : 0;
      float b = NV_SHELF_TOP - NV_HERO_COPY_GAP - 24, bh = NV_HERO_BOTAO_COMPACTO_H;
      HeroCopyLayout on = heroCopyLayout(b, sh, 1, 1, 0, lh, bh, NV_HOME_HERO_BOTAO_GAP, 24, 1.0f, 0);
      HeroCopyLayout off = heroCopyLayout(b, sh, 1, 1, 0, lh, bh, NV_HOME_HERO_BOTAO_GAP, 24, 0.0f, 0);
      assert(off.logoHeight == on.logoHeight);
      assert(fabsf(off.synopsis + sh - b) < 0.01f);
      assert(fabsf(on.synopsis + sh + NV_HOME_HERO_BOTAO_GAP + bh - b) < 0.01f);
      assert(off.logo > on.logo);
      HeroCopyLayout mid = heroCopyLayout(b, sh, 1, 1, 0, lh, bh, NV_HOME_HERO_BOTAO_GAP, 24, 0.5f, 0);
      assert(mid.logo > on.logo && mid.logo < off.logo);
      assert(mid.action >= mid.synopsis + sh);   // the fading button never covers the text
    }
  puts("ok layout: Moderna block settles on the base when the button slot collapses");
  puts("ok layout: Moderna, Padrão, Dinâmica; captions, compact synopsis, empty information");
  return 0;
}
