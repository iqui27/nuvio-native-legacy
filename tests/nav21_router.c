#include "nav21.h"
#include "../src/app.c"
void nav21_library(void);
void nav21_route_library_open(void);
int nav21_route_library_focus(void);
void nav21_route_settings_open(void);
int nav21_route_settings_focus(void);
void nav21_router(void) {
  nav21_library();
  nav21_route_library_open();tela=TELA_BIBLIOTECA;menu_definir_destino(MENU_BIBLIOTECA);
  int saved=nav21_route_library_focus();
  SpotPedido p={0};p.tipo=SPOT_AJUSTE;p.indice=0;
  // Use the same request handler as the Spotlight result consumer.
  spotAbrirPedido(p);
  voltarTela();
  nav21_expect("Router/Spotlight","Library, Spotlight, Settings, Back: Library",tela,TELA_BIBLIOTECA);
  nav21_expect("Router/Spotlight","Library, Spotlight, Settings, Back: focus",nav21_route_library_focus(),saved);
  nav21_expect("Router/Spotlight","Library, Spotlight, Settings, Back: sidebar",menu_destino(),MENU_BIBLIOTECA);
  nav21_route_settings_open();tela=TELA_AJUSTES;menu_definir_destino(MENU_AJUSTES);
  saved=nav21_route_settings_focus();
  guiaAtender();
  nav21_expect("Router/guide","Settings guide, Library: destination",tela,TELA_BIBLIOTECA);
  voltarTela();
  nav21_expect("Router/guide","Settings guide, Library, Back: Settings",tela,TELA_AJUSTES);
  nav21_expect("Router/guide","Settings guide, Library, Back: focus",nav21_route_settings_focus(),saved);
  nav21_expect("Router/guide","Settings guide, Library, Back: sidebar",menu_destino(),MENU_AJUSTES);

}
