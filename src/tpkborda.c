// BORDA DAS LEGENDAS NA TV (.tpk Tizen 8/9, dono 24/10/2026).
//
// O nosso GL escreve sobre o furo transparente com blend SRC_ALPHA: o RGB da
// camada sai JA multiplicado pelo alfa. O DALi compoe o GLView como alfa
// "reto" e multiplica de novo, e toda borda suave escurece: o contorno preto
// das mascaras de typeset; no Colour test card o branco a 50 % ficava como o
// cinza 25 %. Ligar BlendPreMultipliedAlpha no Renderer do GLView conserta
// (provado na S95C), mas muda a interface inteira (fica um pouco mais clara).
//
// Por isso so vale enquanto o libass desenha: liga quando ha imagem do libass
// na tela, um quadro com imagem na fila do render ahead, ou uma fala que
// comeca em ate 200 ms (o DALi recebe a troca no quadro seguinte, entao a
// primeira linha ja sai certa); desliga 1 s depois da ultima imagem, ou na
// hora quando a faixa libass para ou o player fecha. Legenda comum, menus e o
// resto do app: sempre como antes. Quem troca e o host (Exibe.cs, Premult),
// no fio principal; nos pacotes 6.0/6.5 (GLWindow) nao ha GLView e nada muda.
#ifdef NV_TPK
#include "assrender.h"
#include "player.h"
#include <stdio.h>

typedef void (*TbFnInt)(int);
static TbFnInt hPremult;
static int reenviar;   // o host chegou depois: manda o estado atual de novo

// Host .NET (Exibe.cs). Host mais velho sem a chamada: nunca registra, nada muda.
__attribute__((visibility("default")))
void nv_tpk_premult_registrar(TbFnInt premult) {
  hPremult = premult;
  __atomic_store_n(&reenviar, 1, __ATOMIC_RELEASE);
}

// Fio do app, todo quadro (app.c).
void tpkborda_quadro(void) {
  static int estado;
  int quer = player_aberto() && assrender_borda_quer();
  if (quer == estado && !__atomic_exchange_n(&reenviar, 0, __ATOMIC_ACQ_REL)) return;
  estado = quer;
  if (hPremult) hPremult(quer);
}
#endif
