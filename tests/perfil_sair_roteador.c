// Revisao 2.0.3 sobre o #371: ESQUERDA na borda do Perfil so PEDE a barra
// (sair = 1) e deixa `aberto` = 1, porque o roteador abre a barra POR CIMA da
// pagina. Mas com a barra proibida (mini player / PiP: sidebar_permitida()
// falsa) o roteador vai para a Home com trocarTela(TELA_HOME) e ninguem fechava
// o Perfil: perfil_atualizar/socialvis_atualizar seguiam rodando todo quadro
// numa tela que ja nao existe. O mesmo valia para sair do Perfil pela barra.
//
// Inclui o app.c de verdade e exercita trocarTela, que e por onde TODA saida
// do Perfil passa.
#define SDL_MAIN_HANDLED
#include "../src/app.c"
#include "dados.h"
#include <assert.h>

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  perfil_evento(&e);
}

int main(void) {
  dados_iniciar("deploy/app/art");
  ajustes_dir(dados_dir());
  perfil_iniciar();

  // Perfil aberto pelo roteador com dados; ESQUERDA no primeiro dia e a borda.
  PerfilDados d;
  memset(&d, 0, sizeof d);
  snprintf(d.nome, sizeof d.nome, "Test");
  d.plays = 5; d.nDias = 30;
  tela = TELA_PERFIL;
  perfil_abrir();
  perfil_definir_dados(&d);
  tecla(SDLK_LEFT);
  assert(perfil_quer_sair());
  assert(perfil_aberto());           // #371: ainda vivo, a barra pode abrir por cima
  // Barra proibida (PiP): o roteador manda para a Home.
  trocarTela(TELA_HOME);
  assert(tela == TELA_HOME);
  assert(!perfil_aberto());          // o Perfil tem de parar de atualizar

  // Voltar ao Perfil reabre normalmente.
  trocarTela(TELA_PERFIL);
  assert(perfil_aberto());
  assert(!perfil_quer_sair());       // sem pedido de saida velho

  puts("PASS: perfil_sair_roteador");
  return 0;
}
