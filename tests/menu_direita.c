// #750: DIREITA na barra lateral abre a janela destacada (LG webOS).
//
// O QUE O TESTE PROVA, no menu.c de verdade (sem rede, sem janela):
//   1. barra aberta (rail Padrao): CIMA/BAIXO destacam a linha; DIREITA abre
//      a janela daquela linha — o destino muda e a barra recolhe. OK faz o
//      mesmo; as duas teclas passam pelo mesmo escolher().
//   2. por cima da pagina do titulo (menu_abrir_sobre, decisao do dono de
//      03/10): DIREITA devolve o foco a pagina (a camada fica viva), e OK
//      abre a janela destacada — a pagina fecha pelo menu_escolheu().
//   3. na linha do destino ATUAL, DIREITA tambem confirma (a barra recolhe
//      e a tela permanece): nao e um "nada acontece" silencioso — o app ve
//      menu_escolheu() e decide.
//
//   bash tests/menu_direita.sh
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

static void assentar(void) {
  int i;
  for (i = 0; i < 200; i++) menu_atualizar(0.016f, 0);
}

int main(void) {
  char dir[NVT_MAX], caminho[700]; nvt_set(dir, sizeof dir, "/nuvio-menu-direita-XXXXXX");
  FILE *f;
  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w"); assert(f);
  fputs("menuGuiaLocal 1\nmenuAgendaLocal 1\n", f);   // itens escondidos, como #162
  fclose(f);
  ajustes_dir(dir);
  menu_iniciar();

  // 1. DIREITA abre a janela destacada: Inicio -> Biblioteca (pula Guia oculto).
  menu_definir_destino(MENU_INICIO);
  menu_fechar(); menu_abrir();
  assert(menu_aberto());
  tecla(SDLK_DOWN);                    // Explorar
  tecla(SDLK_DOWN);                    // Buscar (Guia escondido)
  tecla(SDLK_DOWN);                    // Biblioteca
  tecla(SDLK_RIGHT);
  assert(!menu_aberto());
  assert(menu_mudou_destino());
  assert(menu_destino() == MENU_BIBLIOTECA);
  puts("ok direita abre a janela destacada (Biblioteca)");

  // OK faz o mesmo caminho.
  menu_fechar(); menu_definir_destino(MENU_INICIO); menu_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  assert(!menu_aberto() && menu_destino() == MENU_EXPLORAR);
  puts("ok OK confirma a linha igual a DIREITA");

  // 3. Na linha do destino atual a DIREITA confirma tambem (recolhe, sem
  //    mudar a tela): o app ve menu_escolheu().
  assentar();
  menu_escolheu(); menu_mudou_destino();
  menu_fechar(); menu_definir_destino(MENU_INICIO); menu_abrir();
  tecla(SDLK_RIGHT);
  assert(!menu_aberto() && menu_escolheu() && !menu_mudou_destino());
  assert(menu_destino() == MENU_INICIO);
  puts("ok direita na linha atual confirma sem trocar a tela");

  // 2. Por cima da pagina do titulo: DIREITA devolve o foco, a camada fica;
  //    OK abre a janela (mesmo o destino atual) e a pagina fecha.
  assentar();
  menu_escolheu(); menu_mudou_destino();
  menu_fechar(); menu_definir_destino(MENU_INICIO);
  assentar();
  menu_abrir_sobre(1);
  assert(menu_aberto() && menu_sobre());
  assentar();
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  assert(!menu_aberto() && menu_destino() == MENU_INICIO && !menu_escolheu());
  assert(menu_sobre());                       // a pagina continua viva por baixo
  assentar();
  assert(!menu_sobre());
  menu_abrir_sobre(1);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);       // Explorar
  assert(!menu_aberto() && menu_escolheu() && menu_mudou_destino());
  assert(menu_destino() == MENU_EXPLORAR);
  puts("ok por cima da pagina: direita devolve o foco, OK abre a janela");

  unlink(caminho);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t);
    snprintf(t, sizeof t, "%s/envio-151.txt", dir); unlink(t); }
  rmdir(dir);
  puts("menu_direita: ok");
  return 0;
}
