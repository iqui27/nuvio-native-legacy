#include "nav21.h"
#define sessao_logada nav21_logged_in
#define desc_sessao_aberta nav21_discovery_open
#include "../src/app.c"
int nav21_logged_in(void){return 1;}
int nav21_discovery_open(void){return 1;}
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

  // Exercise the real Settings exit request and router update, including the
  // following frames that used to consume Profile's stale exit request.
  ajustes_encerrar();menu_fechar();perfilSessaoAplicada=perfis_sessao_geracao();
  tela=TELA_PERFIL;menu_definir_destino(MENU_PERFIL);perfil_abrir();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_UP;perfil_evento(&e);
  nav21_expect("Router/Profile","Profile, Up: achievements focused",perfil_foco_conquistas(),1);
  spotAbrirPedido(p);
  nav21_expect("Router/Profile","Profile, Spotlight Settings: Profile suspended",perfil_aberto(),0);
  e.key.keysym.sym=SDLK_ESCAPE;app_evento(&e);
  Uint32 now=SDL_GetTicks();
  for(int i=0;i<60;i++)app_atualizar(.025f,now+i*25);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back, 1500ms: Profile",tela,TELA_PERFIL);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back: open",perfil_aberto(),1);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back: focus",perfil_foco_conquistas(),1);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back: sidebar",menu_destino(),MENU_PERFIL);
  e.key.keysym.sym=SDLK_RETURN;app_evento(&e);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back, OK: accepts input",perfil_pediu_conquistas(),1);
  e.key.keysym.sym=SDLK_ESCAPE;app_evento(&e);app_atualizar(.025f,now+1525);
  nav21_expect("Router/Profile","Profile, Spotlight Settings, Back, Back: Home",tela,TELA_HOME);
  nav21_expect("Router/Profile","Profile, Back: closed",perfil_aberto(),0);
}
