// #728 (r3): corridas do ack de seek no TPK, com relogio controlado e ordem
// forcada pelos pontos -DNV_TEST_TPK_ACK de src/video_tpk.c. Sonda do revisor
// (evidencias/sol-f65r3/ack-edges.c) adotada como teste.
//
// O host serializa as buscas, guarda so o ultimo pedido pendente e emite o
// ack da operacao EM ANDAMENTO antes de o assumir (tizen-tpk/Video.cs). So o
// ack da geracao corrente confirma:
//   "prazo"     ack valido processado pelo fio do host entre a leitura da
//               marca e o -2 do prazo das legendas — o -1 nao pode ser
//               sobrescrito;
//   "repetido"  seek 13 s em andamento, pede 14 s, volta a pedir 13 s: o ack
//               do primeiro 13 s nao pode confirmar o segundo;
//   "distinto"  ack de alvo velho depois do prazo segue descartado;
//   "supersede" ack que leu o alvo antigo e so entao classifica, com um seek
//               novo (14 s) entrando no meio;
//   "supersede-prazo" o mesmo, com o prazo do seek novo gravando -2 de novo;
//   "relogio"   seek a 0 s substitui a retomada de 12 s: o ack velho e
//               descartado e o ack de 0 s libera capa e o relogio da #710.
#include <pthread.h>

#define main seek_main
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

// Advance the clock without waiting. The production 10 s and 15 s values stay intact.
static Uint32 ticks = 1;
Uint32 SDL_GetTicks(void) { return ticks; }
void SDL_Delay(Uint32 ms) { ticks += ms; }

static int inject_ack;
static unsigned injected;
static int supersede_during_ack;

// Ponto -DNV_TEST_TPK_ACK do fio do host: o ack ja foi recebido e vai ser
// classificado. Entre isso e a decisao entra o pedido novo de seek do app.
void tpk_teste_ack_ponto(void) {
  if (!supersede_during_ack) return;
  int expire_new_seek = supersede_during_ack == 2;
  supersede_during_ack = 0;
  video_buscar(14);
  if (expire_new_seek) {
    bombear(3);
    SDL_Delay(10100);
    bombear(3);
  }
}

static void *host_ack(void *unused) {
  (void)unused;
  nv_tpk_video_evento(11, 300000, 0);
  return NULL;
}

// Ponto -DNV_TEST_TPK_ACK do fio do app: a marca do prazo foi lida e o -2 vai
// ser gravado. O ack do host tem de caber exatamente nessa janela.
void tpk_teste_prazo_ponto(void) {
  if (!inject_ack) return;
  inject_ack = 0;
  posMs = 300000;
  pthread_t host;
  if (pthread_create(&host, NULL, host_ack, NULL) || pthread_join(host, NULL)) abort();
  ++injected;
}

static void deadline(void) {
  prepararRetomadaPausada();
  video_buscar(300);
  bombear(3);
  inject_ack = 1;
  SDL_Delay(10100);
  bombear(3);
  printf("deadline: worker ack=%u host=%d capa=%d busca=%d\n",
         injected, posMs, video_tpk_capa(), video_busca_em_voo());
  ok(injected == 1 && !video_tocando() && !video_tpk_capa() && !video_busca_em_voo(),
     "ack valido na janela do prazo das legendas tem de sobreviver");
}

static void distinct(void) {
  prepararRetomadaPausada();
  video_buscar(13);
  video_buscar(14);
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  posMs = 13000;
  nv_tpk_video_evento(11, 13000, 0);
  bombear(3);
  ok(video_tpk_capa() && video_busca_em_voo(), "ack de alvo velho nao rearma apos o prazo");
  posMs = 14000;
  nv_tpk_video_evento(11, 14000, 0);
  nv_tpk_video_evento(11, 14000, 0);
  bombear(3);
  ok(!video_tpk_capa() && !video_busca_em_voo(), "ack corrente e duplicado imediato fecham o seek");
}

static void superseded_during_ack(void) {
  prepararRetomadaPausada();
  video_buscar(13);
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  supersede_during_ack = 1;
  posMs = 13000;
  nv_tpk_video_evento(11, 13000, 0);
  bombear(3);
  ok(!supersede_during_ack && ultBusca == 14000 && video_tpk_capa() && video_busca_em_voo(),
     "seek novo sobrevive ao ack velho que ja tinha lido o alvo antigo");
  posMs = 14000;
  nv_tpk_video_evento(11, 14000, 0);
  bombear(3);
  ok(!video_tpk_capa() && !video_busca_em_voo(), "seek novo fecha com o proprio ack");
}

static void superseded_then_expired(void) {
  prepararRetomadaPausada();
  video_buscar(13);
  bombear(3);
  SDL_Delay(1000);
  supersede_during_ack = 2;
  posMs = 13000;
  nv_tpk_video_evento(11, 13000, 0);
  bombear(3);
  printf("old ack after new deadline: current=%d host=%d capa=%d busca=%d\n",
         ultBusca, posMs, video_tpk_capa(), video_busca_em_voo());
  ok(!supersede_during_ack && ultBusca == 14000 && video_tpk_capa() && video_busca_em_voo(),
     "ack velho nao confirma o alvo novo depois de o prazo gravar -2 de novo");
}

static void repeated_target(void) {
  prepararRetomadaPausada();
  video_buscar(13);
  video_buscar(14);
  video_buscar(13);
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  posMs = 13000;
  nv_tpk_video_evento(11, 13000, 0); // completion of the first request for 13 s
  bombear(3);
  printf("repeated target: latest ack withheld host=%d capa=%d busca=%d\n",
         posMs, video_tpk_capa(), video_busca_em_voo());
  ok(video_tpk_capa() && video_busca_em_voo(), "ack velho nao confirma seek novo com o mesmo alvo");
}

static void recon_clock(void) {
  quedaEReabre();
  posMs = 12000;
  playerPronto();
  nv_tpk_video_evento(1, 720000, 0);
  bombear(3);
  ok(ultBusca == 12000, "seek automatico enviado e ainda sem ack");
  video_pausar(1);
  video_buscar(0);
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  nv_tpk_video_evento(11, 12000, 0);
  ok(video_tpk_capa() && video_busca_em_voo(), "ack velho de 12 s nao libera o seek pausado");
  posMs = 0;
  nv_tpk_video_evento(11, 0, 0);
  bombear(3);
  printf("reconnect clock: host=%d pos=%.3f capa=%d busca=%d\n",
         posMs, video_pos(), video_tpk_capa(), video_busca_em_voo());
  ok(video_pos() == 0 && !video_tpk_capa(), "ack tardio de 0 s libera capa e o relogio da #710");
}

int main(int argc, char **argv) {
  static const CdnlinkGanchos g = { fOrigem, fRecente, fResolver, fValidade };
  SDL_Init(0);
  recenteSim = 1;
  validadeMs = 150 * 60 * 1000;
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscarP, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_cdn_ganchos(&g);
  if (argc > 1 && !strcmp(argv[1], "prazo")) deadline();
  else if (argc > 1 && !strcmp(argv[1], "repetido")) repeated_target();
  else if (argc > 1 && !strcmp(argv[1], "supersede")) superseded_during_ack();
  else if (argc > 1 && !strcmp(argv[1], "supersede-prazo")) superseded_then_expired();
  else if (argc > 1 && !strcmp(argv[1], "relogio")) recon_clock();
  else distinct();
  video_parar();
  return falhas != 0;
}
