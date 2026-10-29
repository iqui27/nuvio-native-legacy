// Exercise the real Settings row with reported panel sizes, no TV required.
#include <SDL2/SDL.h>
static int panelW = 3840, panelH = 2160;
static int reportedDesktop(int display, SDL_DisplayMode *mode) {
  (void)display; mode->w = panelW; mode->h = panelH; return 0;
}
#define SDL_GetDesktopDisplayMode reportedDesktop
#include "../src/ajustes.c"
#undef SDL_GetDesktopDisplayMode
#include <assert.h>

// Round-trip the device choice, then change resolution and reload as at restart.
void ui720_saved(int width, int height, int app, int resolution) {
  panelW = width; panelH = height;
  valor[AJ_AMPLIAR_UI] = app;
  valor[AJ_RESOLUCAO] = RES_720;
  assert(gravar());
  valor[AJ_RESOLUCAO] = resolution;
  assert(gravar());
  valor[AJ_AMPLIAR_UI] = !app;
  valor[AJ_RESOLUCAO] = RES_720;
  assert(dados_gravar(RES_ARQ_MIGRADA, "1\n"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(valor[AJ_AMPLIAR_UI] == app && valor[AJ_RESOLUCAO] == resolution);
}

void ui720_settings(int width, int height, int app, int weak, int sourceHeight) {
  panelW = width; panelH = height;
  valor[AJ_AMPLIAR_UI] = app;
  valor[AJ_RESOLUCAO] = sourceHeight == 720 ? RES_720 : RES_1080;
  assert(ajOpcaoDisponivel(AJ_AMPLIAR_UI) == (width >= 3840 && height >= 2160));
  assert(inativa(AJ_AMPLIAR_UI) == weak);
  assert(ajustes_ampliar_app() == (app && width >= 3840 && height >= 2160));
  assert(somenteDesteAparelho(AJ_AMPLIAR_UI));
  if (!app) assert(OPCOES[AJ_AMPLIAR_UI].valores[0] && valor[AJ_AMPLIAR_UI] == 0);
  assert(!ajustePortavel(AJ_AMPLIAR_UI));
  assert(!strcmp(textoValor(AJ_AMPLIAR_UI), app ? "Pelo app (4K)" : "Pela TV"));
  // #205: in Automatic the choice has no effect — the row shows the effective
  // "Pela TV" and is unavailable, and the stored value is untouched. Back at
  // 1080p/720p the stored choice is what shows.
  int savedRes = valor[AJ_RESOLUCAO];
  valor[AJ_RESOLUCAO] = RES_AUTO;
  assert(inativa(AJ_AMPLIAR_UI));
  assert(!strcmp(textoValor(AJ_AMPLIAR_UI), "Pela TV"));
  assert(valor[AJ_AMPLIAR_UI] == app);
  valor[AJ_RESOLUCAO] = RES_4K;
  assert(inativa(AJ_AMPLIAR_UI));
  assert(!strcmp(textoValor(AJ_AMPLIAR_UI), "Pela TV"));
  valor[AJ_RESOLUCAO] = savedRes;
  assert(!strcmp(textoValor(AJ_AMPLIAR_UI), app ? "Pelo app (4K)" : "Pela TV"));
  assert(inativa(AJ_AMPLIAR_UI) == weak);
  int row = -1;
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_AMPLIAR_UI) row = i;
  assert(row >= 0);
  assert(ajustes_teste_focar_opcao(width >= 3840 && height >= 2160 && !weak ? AJ_AMPLIAR_UI : AJ_RESOLUCAO));
  assert(visivel(row) == (width >= 3840 && height >= 2160));
  ajustes_teste_layout(1);
  for (int n = 0; n < 90; n++) ajustes_atualizar(1.f / 60, 10000);
}
