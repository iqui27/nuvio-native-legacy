/* Inspect the shared visibility/order paths without sending input events. */
#include "../src/menu.c"
#include "dados.h"
#include <assert.h>

void sports_sidebar_menu_test(void) {
  const char *dir = getenv("NUVIO_DADOS");
  assert(dir && !layout_modo_mobile());
  for (int hidden = 0; hidden < 2; hidden++) {
    char settings[180];
    snprintf(settings, sizeof settings,
      "idioma 0\nidiomaAutoLocal 0\nmenuEsportesLocal %d\nmenuGuiaLocal 0\n", hidden);
    dados_gravar("ajustes.txt", settings); ajustes_dir(dir); menu_iniciar();
    assert(ajustes_menu_esportes() == !hidden);
    assert(!mostra(MENU_ESPORTES) && mostra(MENU_GUIA));
    int order[NV_MENU_FOCOS_TV];
    int n = tvOrdem(order), guide = 0;
    for (int i = 0; i < n; i++) { assert(order[i] != MENU_ESPORTES); guide |= order[i] == MENU_GUIA; }
    assert(guide);
    n = topoLista(order); guide = 0;
    for (int i = 0; i < n; i++) { assert(order[i] != MENU_ESPORTES); guide |= order[i] == MENU_GUIA; }
    assert(guide);
  }
}
