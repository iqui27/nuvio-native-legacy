// Keep app internals separate from the Library fixture's static state.
#define NV_LINUX_DESKTOP
#include "../src/app.c"
#include <assert.h>

void biblioteca_up_route_start(void) {
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(dados_gravar("ajustes.txt",
      "idioma 0\nidiomaAutoLocal 0\nhomeLayoutLocal 0\nmenuTopoLocal 1\n"));
  ajustes_dir(dados_dir());
  assert(ajustes_menu_topo());
  menu_iniciar();
  menu_definir_destino(MENU_BIBLIOTECA);
  uiPreview = 1;
  perfilSessaoAplicada = perfis_sessao_geracao();
  tela = TELA_BIBLIOTECA;
}

void biblioteca_up_route_key(const SDL_Event *e) {
  app_evento(e);
  for (int i = 0; i < 120; i++) app_atualizar(.01f, SDL_GetTicks());
}

int biblioteca_up_route_library(void) { return tela == TELA_BIBLIOTECA; }
