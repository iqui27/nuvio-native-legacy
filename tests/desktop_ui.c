// Exercise the real responsive canvas and shared sidebar routing without GL.
#include "../src/layout.c"
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

int ajustes_home_layout(void) { return HOME_LAYOUT_DINAMICA; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int esportes_modalidades_n(void) { return 1; }
int esportes_habilitado(int sport) { (void)sport; return 1; }
int ajustes_menu_topo(void) { return 0; }
int ajustes_rail_recolhida(void) { return 0; }
float gfx_escala_ui_base(void) { return 1; }
float gfx_escala_txt(void) { return 1; }
int home_streaming_barra(const int **p) {
  static const int folders[] = {4, 5, 6};
  if (p) *p = folders;
  return 3;
}

static void key(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  menu_evento(&e);
}
static char sidebarSaved[4];
int dados_gravar(const char *name, const char *value) { (void)name; snprintf(sidebarSaved,sizeof sidebarSaved,"%s",value); return 1; }
char *dados_ler(const char *name) { (void)name; return strdup(sidebarSaved); }
int main(void) {
  layout_tela_definir(2560, 1600);
  layout_janela_definir(1280, 800);
  assert(layout_desktop_ativo() && !layout_modo_mobile());
  assert(nv_layout_w == 1920 && nv_layout_h == 1200);
  layout_modo_definir(1); // Saved phone preference cannot override window size.
  assert(layout_desktop_ativo());
  assert(menuEscala() == 1);
  menu_servidores(1);
  int order[NV_MENU_FOCOS_TV];
  int n = tvOrdem(order);
  assert(n == 12);
  assert(order[4] == MENU_LISTAS && order[5] == MENU_CONQUISTAS);
  assert(order[6] == MENU_JUNTOS && order[8] == MENU_STREAMING);
  assert(order[0] == MENU_BUSCAR && order[1] == MENU_INICIO);
  assert(order[n - 2] == MENU_AJUSTES && order[n - 1] == MENU_RODAPE);
  assert(tvPastas(NULL) == 0); // Services open only through the Streaming entry.
  float ys[NV_MENU_FOCOS_TV];
  desktopLayout(ys);
  assert(ys[MENU_INICIO] == 164.5f && ys[MENU_AJUSTES] == 1068 && ys[MENU_RODAPE] == 1124);
  menu_desktop_alternar(); assert(desktopCollapsed && sidebarSaved[0]=='1');
  desktopCollapsed=0; desktopRestaurar(); assert(desktopCollapsed && nv_desktop_sidebar==0);
  menu_desktop_hover(3); assert(desktopOverlay && desktopCollapsed);
  menu_desktop_hover(500); assert(!desktopOverlay && desktopCollapsed);
  menu_desktop_revelar(); assert(desktopOverlay && desktopCollapsed);
  menu_desktop_expandir(); assert(!desktopCollapsed && sidebarSaved[0]=='0');
  menu_abrir(); key(SDLK_UP);
  assert(linha == MENU_BUSCAR && menu_desktop_foco()==MENU_BUSCAR);
  assert(menu_destino()==MENU_INICIO);
  key(SDLK_RIGHT);
  assert(menu_desktop_foco()==-1);
  assert(menu_destino() == MENU_BUSCAR && !menu_aberto() && menu_visivel());
  menu_abrir(); key(SDLK_AC_BACK);
  assert(!menu_aberto() && menu_destino() == MENU_BUSCAR);
  desktopEscolher(MENU_STREAMING,0); assert(tvPastas(NULL)==3);
  n=tvOrdem(order);assert(order[9]==MENU_ST0 && order[10]==MENU_ST0+1 && order[12]==MENU_FONTES);
  desktopEscolher(MENU_ST0 + 1, 0);
  assert(menu_pediu_colecao() == 5 && !menu_aberto());
  desktopEscolher(MENU_RODAPE, 0);
  assert(menu_pediu_trocar() && !menu_aberto());
  desktopBusca(0, 0);
  assert(menu_destino() == MENU_BUSCAR && !menu_aberto());
  menu_ir(MENU_CONQUISTAS); assert(menu_mudou_destino());
  menu_ir(MENU_CONQUISTAS); assert(menu_mudou_destino());
  menu_ir(MENU_JUNTOS); assert(menu_mudou_destino());
  menu_ir(MENU_JUNTOS); assert(menu_mudou_destino());
  menu_ir(MENU_LISTAS); assert(menu_mudou_destino());
  menu_ir(MENU_LISTAS); assert(menu_mudou_destino());
  menu_ir(MENU_ESPORTES); assert(menu_destino()==MENU_ESPORTES && menu_mudou_destino());
  menu_fechar(); assert(menu_destino()==MENU_ESPORTES);
  menu_ir(MENU_BIBLIOTECA); assert(menu_destino()==MENU_BIBLIOTECA && menu_mudou_destino());
  // Opening a child overlay does not choose Home; closing/reopening navigation
  // must retain the page that owns it.
  menu_abrir_sobre(0); menu_fechar(); assert(menu_destino()==MENU_BIBLIOTECA);

  layout_janela_definir(1000, 500);
  assert(layout_desktop_ativo() && nv_layout_w == 2400);
  menu_abrir(); linha = MENU_ST0 + 2;
  desktopAtualizar(); desktopLayout(ys);
  assert(ys[linha] + 56 - tvRolar <= nv_layout_h - 142);
  linha = MENU_INICIO; desktopAtualizar();
  assert(tvRolar == 0);
  menu_fechar();
  layout_tela_definir(1600, 1800);
  layout_janela_definir(800, 900);
  assert(!layout_desktop_ativo() && layout_modo_mobile());
  assert(nv_layout_w == 1080 && nv_layout_h == 1215);
  desktopStreaming=0; assert(tvPastas(NULL) == 0); // Existing narrow drawer stays collapsed.
  layout_tela_definir(1800, 1600);
  layout_janela_definir(900, 800);
  assert(layout_modo_mobile() && nv_layout_w == 1215 && nv_layout_h == 1080);
  layout_tela_definir(2560, 1600);
  layout_janela_definir(1280, 800);
  assert(layout_desktop_ativo() && nv_layout_w == 1920 && nv_layout_h == 1200);
  layout_janela_definir(0, 0);
  assert(nv_layout_w == 1920 && nv_layout_h == 1200);
  puts("PASS: desktop/narrow/Retina resize, persistent navigation, streaming, profile, search, keyboard");
  return 0;
}
