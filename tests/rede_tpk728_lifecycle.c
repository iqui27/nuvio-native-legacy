#define main main_teste_original
#include "rede_tpk728_seek.c"
#undef main

static void prepararRetomadaPausada(void) {
  quedaEReabre();
  posMs = 0;
  playerPronto();
  nv_tpk_video_evento(1, 720000, 0);
  bombear(3);
  ok(ultBusca == 12000, "retomada enviou seek a 12 s");
  posMs = 12000;
  nv_tpk_video_evento(11, 12000, 0);
  video_pausar(1);
  ok(!video_tocando(), "host pausado");
}

static void ackAtrasado(void) {
  puts("== ack anterior com alvo distinto");
  prepararRetomadaPausada();
  bombear(3);
  ok(video_tpk_capa(), "retomada automatica pausada espera tocar");
  video_buscar(180);
  video_buscar(300);
  posMs = 300000;
  nv_tpk_video_evento(11, 180000, 0);
  nv_tpk_video_evento(11, 12000, 0);
  bombear(3);
  ok(video_tpk_capa() && video_busca_em_voo(), "ack velho nao libera nem com a posicao no alvo novo");
  nv_tpk_video_evento(11, 300000, 0);
  bombear(3);
  ok(!video_tpk_capa() && !video_busca_em_voo(), "ack correspondente libera a 300 s");
  video_parar();
}

static void cdnDepoisDeSeek(void) {
  puts("== seek interrompido por renovacao automatica no resume");
  recenteSim = 1;
  validadeMs = 5500;
  prepararRetomadaPausada();
  video_buscar(300);
  ok(ultBusca == 300000 && video_pos() == 300, "seek do usuario em voo na sessao anterior");

  SDL_Delay(550);
  video_pausar(0);
  ok(nAbriu == 3 && video_pos() == 12, "resume renova CDN na posicao real anterior de 12 s");
  validadeMs = 150 * 60 * 1000;
  posMs = 0;
  playerPronto();
  bombear(3);
  ok(ultBusca == 12000, "sessao nova emite seek automatico a 12 s");
  posMs = 12000;
  nv_tpk_video_evento(11, 12000, 0);
  video_pausar(1);
  bombear(3);
  printf("host=%d pos=%.3f tocando=%d capa=%d busca=%d\n",
         posMs, video_pos(), video_tocando(), video_tpk_capa(), video_busca_em_voo());
  ok(!video_tocando() && video_tpk_capa() && video_busca_em_voo(),
     "reabertura automatica deve esperar tocar mesmo apos seek da sessao descartada");
  video_pausar(0);
  bombear(3);
  ok(!video_tpk_capa() && !video_busca_em_voo(), "retomada automatica libera ao tocar");
  video_parar();
  recenteSim = 0;
}

static void zeroAntesDaReabertura(void) {
  puts("== seek pausado a zero enfileirado durante a espera da reconexao");
  recenteSim = 1;
  nAbriu = 0;
  posMs = 0;
  ultBusca = -1;
  video_definir_reconexao(1);
  video_definir_mp4(1);
  video_tocar("http://addon.test/a.mp4");
  esperarAbrir(1);
  posMs = 1000;
  playerPronto();
  bombear(3);
  posMs = 12000;
  bombear(3);
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);
  bombear(2);
  ok(video_reconectando() && nAbriu == 1, "queda em espera antes de abrir a nova sessao");
  video_pausar(1);
  video_buscar(0);
  esperarAbrir(2);
  posMs = 0;
  playerPronto();
  bombear(3);
  printf("host=%d pos=%.3f tocando=%d capa=%d busca=%d ultBusca=%d\n",
         posMs, video_pos(), video_tocando(), video_tpk_capa(), video_busca_em_voo(), ultBusca);
  ok(!video_tocando() && posMs == 0, "nova sessao em pausa e no inicio");
  ok(video_pos() == 0 && !video_tpk_capa() && !video_busca_em_voo(),
     "novo alvo zero deve substituir a posicao da queda");
  video_parar();
  recenteSim = 0;
}

static void prazoLegendaSemAck(void) {
  puts("== seek pausado sem ack ate apos o prazo das legendas");
  prepararRetomadaPausada();
  video_buscar(13.0);
  ok(ultBusca == 13000 && posMs == 12000, "seek enviado e host segue na posicao anterior");
  bombear(3);
  ok(video_tpk_capa() && video_busca_em_voo(), "capa e busca em voo antes do prazo");
  SDL_Delay(10100);
  bombear(3);
  printf("host=%d pos=%.3f tocando=%d capa=%d busca=%d ack=0\n",
         posMs, video_pos(), video_tocando(), video_tpk_capa(), video_busca_em_voo());
  ok(!video_tocando() && video_tpk_capa() && video_busca_em_voo(),
     "prazo de legenda nao deve confirmar o pouso pausado sem ack");
  video_parar();
}

int main(int argc, char **argv) {
  static const CdnlinkGanchos g = { fOrigem, fRecente, fResolver, fValidade };
  SDL_Init(0);
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscarP, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_cdn_ganchos(&g);
  if (argc > 1 && !strcmp(argv[1], "ack")) ackAtrasado();
  else if (argc > 1 && !strcmp(argv[1], "zero")) zeroAntesDaReabertura();
  else if (argc > 1 && !strcmp(argv[1], "sem-ack")) prazoLegendaSemAck();
  else cdnDepoisDeSeek();
  return falhas != 0;
}
