// #728 (r3): borda de 2,5 s do pouso do progressivo no TPK. Sonda do revisor
// (evidencias/sol-f65r3/dts-edges.c) adotada como teste, na mesma fixture de
// tests/tpk-dts-reabrir.c (host e conversor falsos, TS continuo).
//
// O primeiro quadro do fluxo novo e a prova do pouso — inclusive na borda: a
// janela e inclusiva, como a do vigia do reabrindo (|pos - mantida| > 2.5
// segura; = 2.5 libera). Alvo 600 s com quadro-chave em 597,5 s solta capa e
// relogio igual a solta em 597,4 s; 2,6 s fora segue coberto.
#define main dts_main
#include "tpk-dts-reabrir.c"
#undef main

static void at_position(double start, int ms) {
  inicioStream = start;
  posMs = ms;
  playerPronto();
  bombear(3);
}

int main(int argc, char **argv) {
  const char *scenario = argc > 1 ? argv[1] : "borda";
  SDL_Init(0);
  montarCabecalho();
  cabPronto = 1;
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_definir_reconexao(1);
  video_tocar("http://h/a.mkv");
  at_position(0, 1000);
  posMs = 660000;
  bombear(3);
  ok(perto(video_pos(), 660.0) && video_tpk_fonte_tocou() && video_tpk_convertendo(), "convertido toca em 660 s");

  if (!strcmp(scenario, "fora")) {
    video_buscar(600);
    at_position(597, 597400);
    ok(video_tpk_capa() && video_busca_em_voo() && perto(video_pos(), 600),
       "quadro a 2,6 s do alvo segue coberto");
    posMs = 597600;
    bombear(3);
    ok(!video_tpk_capa() && !video_busca_em_voo() && perto(video_pos(), 597.6),
       "quadro a 2,4 s do alvo libera");
  } else {
    video_buscar(600);
    at_position(597, 597500);
    printf("limit: host=%d pos=%.3f capa=%d busca=%d\n",
           posMs, video_pos(), video_tpk_capa(), video_busca_em_voo());
    ok(!video_tpk_capa() && perto(video_pos(), 597.5),
       "quadro exatamente a 2,5 s do alvo libera capa e relogio");
  }
  video_parar();
  return falhas != 0;
}
