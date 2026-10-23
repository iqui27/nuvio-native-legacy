// #162: itens escondidos nos Ajustes somem da barra lateral e as setas pulam
// por cima deles. Guia e Agenda desligados pelo ajustes.txt, como o arranque le.
#include "ajustes.h"
#include "nvtmp.h"
#include "menu.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  SDL_memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  menu_evento(&e);
}

// Abre a barra, desce `n` vezes e confirma: devolve o destino escolhido.
static int descer(int n) {
  int i;
  menu_fechar();
  menu_definir_destino(MENU_INICIO);
  menu_abrir();
  for (i = 0; i < n; i++) tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  return menu_destino();
}

int main(void) {
  char dir[NVT_MAX], caminho[700]; nvt_set(dir, sizeof dir, "/nuvio-menu-ocultos-XXXXXX");
  FILE *f;
  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  fputs("menuGuiaLocal 1\nmenuAgendaLocal 1\n", f);
  fclose(f);
  ajustes_dir(dir);
  assert(ajustes_menu_explorar() && !ajustes_menu_guia());
  assert(!ajustes_menu_agenda() && ajustes_menu_perfil());

  menu_iniciar();
  assert(descer(1) == MENU_EXPLORAR);
  assert(descer(2) == MENU_BUSCAR);       // pulou o Guia
  assert(descer(3) == MENU_BIBLIOTECA);
  assert(descer(4) == MENU_PERFIL);       // pulou a Agenda
  assert(descer(5) == MENU_AJUSTES);
  // Subindo a partir dos Ajustes tambem pula.
  menu_fechar(); menu_definir_destino(MENU_AJUSTES); menu_abrir();
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_RIGHT);
  assert(menu_destino() == MENU_BIBLIOTECA);
  // Com o destino em vigor escondido, abrir a barra cai no Inicio.
  menu_fechar(); menu_definir_destino(MENU_GUIA); menu_abrir();
  tecla(SDLK_RIGHT);
  assert(menu_destino() == MENU_INICIO);

  // MENU OVER A LAYER (the title page, owner 03/10): Right/Back only hand the
  // focus back (no choice, the layer stays); OK on any row is a choice the
  // app uses to close the layer, even on the current destination.
  menu_escolheu(); menu_mudou_destino();
  menu_fechar(); menu_definir_destino(MENU_INICIO);
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  menu_abrir_sobre(1);
  assert(menu_aberto() && menu_sobre());
  for (int i = 0; i < 30; i++) menu_atualizar(0.016f, 0);
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  assert(!menu_aberto() && menu_destino() == MENU_INICIO && !menu_escolheu());
  assert(menu_sobre());                       // still drawn while collapsing
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  assert(!menu_sobre());
  menu_abrir_sobre(1); tecla(SDLK_ESCAPE);
  assert(!menu_aberto() && !menu_escolheu());
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  menu_abrir_sobre(1); tecla(SDLK_RETURN);   // current destination
  assert(!menu_aberto() && menu_escolheu() && !menu_mudou_destino());
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  menu_abrir_sobre(0); tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(menu_escolheu() && menu_mudou_destino() && menu_destino() == MENU_EXPLORAR);
  // A plain open keeps Right = choose.
  for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
  menu_abrir(); assert(!menu_sobre()); tecla(SDLK_RIGHT);
  assert(menu_escolheu());

  unlink(caminho);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t);
    snprintf(t, sizeof t, "%s/envio-151.txt", dir); unlink(t); }
  rmdir(dir);
  puts("menu_ocultos: ok");
  return 0;
}
