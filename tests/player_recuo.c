// O recuo continuo por quadro cancela Next e espera progresso no destino.
// A memoria do unico pulo automatico continua pertencendo a intro.c.
#define video_pos posicaoTeste
#define video_buscar buscarTeste
#define video_url_atual urlTeste
#define faixas_legenda_temporaria legendaTemporariaTeste
#include "../src/player.c"
#include <assert.h>

static double posicaoReal;
static int legendasLigadas, legendasDesligadas;

double posicaoTeste(void) { return posicaoReal; }
void buscarTeste(double alvo) { posicaoReal = alvo; }
const char *urlTeste(void) { return "http://127.0.0.1/video"; }
int legendaTemporariaTeste(int ligar, const char *idioma) {
  (void)idioma;
  if (ligar) legendasLigadas++;
  else legendasDesligadas++;
  return 1;
}

static void testarLegendaNoCommit(void) {
  comVideo = 1;
  tocando = retomandoSalto = scrubbing = 0;
  posicaoReal = posSeg = 100;

  saltar(-1, 0);
  assert(posSeg == 90);
  legTempQuadro(SDL_GetTicks());
  assert(!legendasLigadas && !legTemp.motivos);
  saltar(1, 0);
  terminarSalto();
  legTempQuadro(SDL_GetTicks());
  assert(posicaoReal == 100 && !legendasLigadas);

  saltar(-1, 0);
  posSeg = 50;
  terminarSalto();
  legTempQuadro(SDL_GetTicks());
  assert(posicaoReal == 50 && !legendasLigadas);

  saltar(-1, 0);
  terminarSalto();
  legTempQuadro(SDL_GetTicks());
  assert(posicaoReal == 40 && legendasLigadas == 1 && legTemp.motivos == LEGTEMP_VOLTA);
  saltar(1, 0);
  terminarSalto();
  assert(posicaoReal == 50 && legendasDesligadas == 1 && !legTemp.motivos);

  saltar(-1, 0);
  terminarSalto();
  assert(legTempPendente);
  saltar(1, 0);
  legTempQuadro(SDL_GetTicks());
  assert(legTempPendente && legendasLigadas == 1);
  legTempMudoEm = SDL_GetTicks() + 500;
  player_voltar_a_esperar();
  assert(!legTempPendente && !legTemp.motivos && !legTemp.voltaFim && !legTempMudoEm);
  assert(legendasDesligadas == 2);
}

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
  testarLegendaNoCommit();
  SDL_Quit();
  puts("player_recuo: recuo continuo confirmado; memoria unica de intro_auto preservada");
  return 0;
}
