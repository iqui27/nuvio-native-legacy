// Fim de parte multipartes (app.c, parteNoFim) com relogio e host simulados:
// a posicao parada nos ultimos 3 s so pode virar "fim" quando nao ha pedido
// pendente. Com seek em curso (busca para tras a partir de 1498/1500 s),
// reconexao ou posicao segurada pela reabertura, nunca avanca — a posicao que
// o host devolve e a de antes do pedido. Sem pendencia, o fim inferido vale.
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef unsigned Uint32;
static Uint32 agora;
unsigned SDL_GetTicks(void) { return agora; }
static int fimFluxo, prontoV, falhouV, pausaP, bufferando;
static int buscando, reconectando, vooV;
static int video_terminou(void) { return fimFluxo; }
static int video_pronto(void) { return prontoV; }
static int video_falhou(void) { return falhouV; }
static int player_pausa_pessoa(void) { return pausaP; }
static unsigned video_bufferando_ms(void) { return bufferando; }
static int player_buscando(void) { return buscando; }
static int video_reconectando(void) { return reconectando; }
static int video_busca_em_voo(void) { return vooV; }
static double parteFimPos = -1;
static Uint32 parteFimDesde;
static const char *parteFimComo = "";
#include "fim.c"

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void reset(void) {
  agora = 1000; fimFluxo = 0; prontoV = 1; falhouV = 0; pausaP = 0;
  bufferando = 0; buscando = 0; reconectando = 0; vooV = 0;
  parteFimPos = -1; parteFimDesde = 0; parteFimComo = "";
}

int main(void) {
  /* Sem pendencia: posicao parada a 1498/1500 por 2,1 s infere o fim. */
  reset();
  ok(!parteNoFim(1500.0, 1498.0), "stationary probe arms the timer");
  agora += 2100;
  ok(parteNoFim(1500.0, 1498.0), "no seek: stopped at the end advances");

  /* A mesma leitura com a busca para tras em curso: nunca avanca. */
  reset();
  buscando = 1;
  ok(!parteNoFim(1500.0, 1498.0), "seek in flight: no end inferred");
  agora += 2100;
  ok(!parteNoFim(1500.0, 1498.0), "seek in flight past 2 s: still no end");
  buscando = 0;
  agora += 4000;
  ok(!parteNoFim(1500.0, 1400.0), "seek landed at 1400: not the end");

  /* Posicao mantida pela reabertura/reconexao (.tpk): idem. */
  reset();
#ifdef NV_TPK
  vooV = 1;
  ok(!parteNoFim(1500.0, 1498.0), "position held by reopen: no end inferred");
  agora += 2100;
  ok(!parteNoFim(1500.0, 1498.0), "held position past 2 s: still no end");
  vooV = 0;
#endif
  reconectando = 1;
  ok(!parteNoFim(1500.0, 1498.0), "reconnecting: no end inferred");

  /* Fim de fluxo de verdade so avanca quando o pendente ja pousou. */
  reset();
  fimFluxo = 1; buscando = 1;
  ok(!parteNoFim(1500.0, 1498.0), "end of stream during a seek: holds");
  buscando = 0;
  ok(parteNoFim(1500.0, 1498.0), "end of stream after the seek: advances");

  /* A busca resolvida, o fim parado volta a valer do zero (2 s novos). */
  reset();
  buscando = 1;
  (void)parteNoFim(1500.0, 1498.0);
  agora += 2100;
  buscando = 0;
  ok(!parteNoFim(1500.0, 1498.0), "timer restarts after the seek resolves");
  agora += 2100;
  ok(parteNoFim(1500.0, 1498.0), "stationary again for 2 s: advances");

  printf(falhas ? "partes-fim-seek: %d FAIL(S)\n" : "partes-fim-seek: ok\n", falhas);
  return falhas != 0;
}
