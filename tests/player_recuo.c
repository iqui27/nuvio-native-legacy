// O recuo continuo por quadro cancela Next e espera progresso no destino.
// A memoria do unico pulo automatico continua pertencendo a intro.c.
#include "../src/player.c"
#include <assert.h>

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  Uint32 agora = SDL_GetTicks() + 3000;
  aberto = 1; entrada = 1; comVideo = tocando = 0;
  duracaoSeg = 1500; posSeg = 1450;
  scrubbing = 1; scrubDir = -1; scrubTocava = 0;
  scrubSalto = (SaltoEst){.inicio = agora - 2000, .ultimaTecla = agora, .rep = 1};
  scrubUltimo = agora;
  creditosauto_zerar(&creditosAuto);
  intro_auto_marcar(INTRO_CREDITOS, 1460);
  player_atualizar(.05f, agora);
  assert(posSeg < 1450);
  assert(creditosAuto.buscaPendente && creditosAuto.buscaRecuo);
  assert(creditosAuto.inicioBusca == 1450 && creditosAuto.fimBusca == posSeg);
  assert(intro_auto_ja(INTRO_CREDITOS, 1460));
  // Continuar avancando nao altera o destino do recuo.
  double ate = creditosAuto.fimBusca;
  scrubDir = 1; scrubSalto.ultimaTecla = agora + 16; scrubUltimo = agora + 16;
  player_atualizar(.05f, agora + 16);
  assert(creditosAuto.fimBusca == ate);
  SDL_Quit();
  puts("player_recuo: recuo continuo confirmado; memoria unica de intro_auto preservada");
  return 0;
}
