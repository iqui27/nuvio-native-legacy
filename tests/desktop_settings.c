// Desktop hides TV-only Settings rows through ajOpcaoDisponivel; the rest stay.
#include "../src/layout.c"
#include "../src/ajustes.c"
#include <assert.h>
int main(void) {
  static const int mantidas[] = { AJ_QUALIDADE, AJ_IDIOMA, AJ_GPU_EFEITOS, AJ_QUALIDADE_IMG,
                                  AJ_LEG_LINGUA, AJ_VIDRO, AJ_TEX_MB, AJ_DIAGNOSTICO };
  size_t i;
  for (i = 0; i < sizeof AJ_DESKTOP_OCULTAS / sizeof *AJ_DESKTOP_OCULTAS; i++)
    assert(!ajOpcaoDisponivel(AJ_DESKTOP_OCULTAS[i]));
  for (i = 0; i < sizeof mantidas / sizeof *mantidas; i++) assert(ajOpcaoDisponivel(mantidas[i]));
  assert(ajustes_esmaecer() == 0);   // no TV screensaver on a computer
  printf("PASS: %zu TV-only settings hidden on desktop, core settings kept\n",
         sizeof AJ_DESKTOP_OCULTAS / sizeof *AJ_DESKTOP_OCULTAS);
}
