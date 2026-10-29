// "Termina as HH:MM" no player (issue #213: "Ends at 1", sempre "1").
// Reproduz o corte do buffer antigo (fim[32]) em russo e confere que o novo
// (RELOGIO_FIM_MAX) cabe em todos os idiomas, com hora de 24 h e de 12 h.
#include "relogiofim.h"
#include "idiomacod.h"
#include <stdlib.h>
#include <string.h>

static int lg = IDIOMA_EN;
// O formato do relogio (Ajustes > Aparencia), como no src/ajustes.c.
static int r12 = 0;
int ajustes_idioma(void) { return lg; }
int ajustes_idioma_ingles(void) { return lg == IDIOMA_EN; }
int ajustes_relogio_12h(void) { return r12; }

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

int main(void) {
  char fim[RELOGIO_FIM_MAX], velho[32];
  const char *fmt;
  // 2026-10-02 10:10:00 UTC; o filme acaba em 3h35 -> 13:45.
  time_t agora = 1790935800;
  int i, cortavam = 0;
  setenv("TZ", "UTC", 1);
  tzset();

  // INGLES, o texto do relato.
  lg = IDIOMA_EN;
  relogio_fim(fim, sizeof fim, agora, 3 * 3600 + 35 * 60);
  CONFERE(!strcmp(fim, "Ends at 13:45"), "en: \"%s\"", fim);
  relogio_fim(fim, sizeof fim, agora, -5);
  CONFERE(!strcmp(fim, "Ends at 10:10"), "en, falta negativa: \"%s\"", fim);

  // O RELATO DA TIZEN: com o relogio em 12 h ligado, a frase tem de sair em
  // 12 h tambem ("9:10 PM"), e nao no 24 h fixo do strftime.
  r12 = 0;
  relogio_fim(fim, sizeof fim, agora, 11 * 3600);
  CONFERE(!strcmp(fim, "Ends at 21:10"), "en, relogio em 24 h: \"%s\"", fim);
  relogio_fim_ilha(fim, sizeof fim, agora, 11 * 3600);
  CONFERE(!strcmp(fim, "ends at 21:10"), "en, pilula em 24 h: \"%s\"", fim);
  r12 = 1;
  relogio_fim(fim, sizeof fim, agora, 11 * 3600);
  CONFERE(!strcmp(fim, "Ends at 9:10 PM"), "en, relogio em 12 h: \"%s\"", fim);
  relogio_fim_ilha(fim, sizeof fim, agora, 11 * 3600);
  CONFERE(!strcmp(fim, "ends at 9:10 PM"), "en, pilula em 12 h: \"%s\"", fim);
  r12 = 0;

  // O DEFEITO, como era: fim[32] em russo deixava so o primeiro digito.
  lg = IDIOMA_RU;
  snprintf(velho, sizeof velho, i18n("Termina \xc3\xa0" "s %s"), "13:45");
  CONFERE(!strcmp(velho, "Заканчивается в 1"), "ru, buffer antigo: \"%s\"", velho);
  printf("buffer antigo (32) em ru: \"%s\"\n", velho);
  relogio_fim(fim, sizeof fim, agora, 3 * 3600 + 35 * 60);
  CONFERE(!strcmp(fim, "Заканчивается в 13:45"), "ru: \"%s\"", fim);

  // TODOS OS IDIOMAS, hora de 24 h e uma de 12 h: a frase inteira cabe e
  // termina na hora (nada cortado).
  for (i = 0; i < IDIOMA_N; i++) {
    const char *horas[] = { "23:59", "11:59 PM" };
    int h;
    lg = i;
    fmt = i18n("Termina \xc3\xa0" "s %s");
    for (h = 0; h < 2; h++) {
      int n = snprintf(fim, sizeof fim, fmt, horas[h]);
      CONFERE(n < (int)sizeof fim, "idioma %d (%s) nao cabe: %d bytes", i, idioma_iso(i), n);
      CONFERE(strstr(fim, horas[h]) != NULL, "idioma %d: hora cortada \"%s\"", i, fim);
      if (h == 0 && n >= 32) cortavam++;
    }
    relogio_fim(fim, sizeof fim, agora, 3 * 3600 + 35 * 60);
    CONFERE(strstr(fim, "13:45") != NULL, "idioma %d: \"%s\"", i, fim);
  }
  printf("idiomas que o fim[32] antigo cortava: %d\n", cortavam);
  CONFERE(cortavam > 0, "o teste deixou de reproduzir o corte antigo");

  if (falhas) { printf("relogiofim: %d falha(s)\n", falhas); return 1; }
  printf("relogiofim: ok\n");
  return 0;
}
