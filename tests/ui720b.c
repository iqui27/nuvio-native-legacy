// Physical 4K panel behind a 1080p SDL desktop: exercise real Settings.
#include <SDL2/SDL.h>
static int desktopW = 1920, desktopH = 1080;
static int desktop(int display, SDL_DisplayMode *mode) {
  (void)display; mode->w = desktopW; mode->h = desktopH; return 0;
}
#include "../src/gpunivel.h"
static int gpuSupported = 1;
static int supported(void) { return gpuSupported; }
#define gpun_upscale_suportado supported
#define SDL_GetDesktopDisplayMode desktop
#include "../src/ajustes.c"
#undef SDL_GetDesktopDisplayMode
#undef gpun_upscale_suportado
#include <assert.h>

int main(int argc, char **argv) {
  const char *physical = argc > 1 ? argv[1] : "3840x2160";
  int eligible = !strcmp(physical, "3840x2160") || !strcmp(physical, "7680x4320");
  if (!strcmp(physical, "desktop4k")) {
    desktopW = 3840; desktopH = 2160; eligible = 1;
  } else setenv("NUVIO_PANEL_SIZE", physical, 1);
  setenv("NUVIO_PANEL_SOURCE", "mock physical panel", 1);
  valor[AJ_AMPLIAR_UI] = 1;
  valor[AJ_IDIOMA] = 1;
  montarTela();
  assert(ajOpcaoDisponivel(AJ_AMPLIAR_UI) == eligible);
  int row = -1;
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_AMPLIAR_UI) row = i;
  assert(row >= 0 && visivel(row) == eligible);
  assert(ajustes_ampliar_app() == eligible);
  valor[AJ_RESOLUCAO] = RES_720;
  assert(ajOpcaoDisponivel(AJ_AMPLIAR_UI) == eligible);
  assert(visivel(row) == eligible);
  assert(!inativa(AJ_AMPLIAR_UI));
  assert(ajustes_720p() && ajustes_ampliar_app() == eligible);
  gpuSupported = 0;
  assert(inativa(AJ_AMPLIAR_UI));
  assert(ajustes_720p() && ajustes_ampliar_app() == eligible);
  gpuSupported = 1;
  valor[AJ_AMPLIAR_UI] = 0;
  assert(ajustes_ampliar_app() == 0);
  ajustes_ui_saida(1920, 1080);
  assert(strstr(ajudaOpcao(AJ_AMPLIAR_UI), "1080p"));
  ajustes_ui_saida(3840, 2160);
  assert(!strcmp(ajudaOpcao(AJ_AMPLIAR_UI), ajudaOpcaoBase(AJ_AMPLIAR_UI)));
  valor[AJ_AMPLIAR_UI] = 1;
  perfilSeguro = 1;
  assert(ajustes_ampliar_app() == 0);
  printf("PASS physical=%s / SDL=%dx%d: visibility, opt-in, safe mode\n", physical, desktopW, desktopH);
  return 0;
}
