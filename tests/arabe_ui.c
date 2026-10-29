// Arabic lookup regression. Also runs against the pre-change idioma.c.
#include "idioma.h"
#include "idiomaauto.h"
#include "bidi.h"
#include <stdio.h>
#include <string.h>

static int language;
int ajustes_idioma(void) { return language; }
int ajustes_idioma_ingles(void) { return language == IDIOMA_EN; }

int main(void) {
  static const struct { const char *key, *value; } cases[] = {
    { "Ajustes", "الإعدادات" }, { "Buscar", "البحث" },
    { "Idioma do app", "لغة التطبيق" }, { "Aparência", "المظهر" },
    { "Reprodução", "التشغيل" }, { "Biblioteca", "المكتبة" },
    { "Árabe", "العربية" }
  };
  char visual[128];
  int errors = 0;
  language = IDIOMA_AR;
  for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
    if (strcmp(i18n(cases[i].key), cases[i].value)) {
      fprintf(stderr, "FAIL Arabic lookup: %s -> %s\n", cases[i].key, i18n(cases[i].key));
      errors++;
    }
  }
  if (errors) return 1;
  if (idiomaauto_mapear("ar-SA") != IDIOMA_AR ||
      idiomaauto_mapear("ar") != IDIOMA_AR ||
      idiomaauto_mapear("ara") != IDIOMA_AR ||
      strcmp(idioma_iso(IDIOMA_AR), "ar")) return 1;
  // سلام: visual order is meem + joined lam-alef + initial seen.
  if (bidi_visual_utf8("سلام", visual, sizeof visual) != 1 ||
      strcmp(visual, "\uFEE1\uFEFC\uFEB3")) return 1;
  puts("Arabic UI lookup, detection and joined forms: PASS");
  return 0;
}
