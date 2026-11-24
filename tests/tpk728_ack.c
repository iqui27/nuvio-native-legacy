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
//
// r4 (#728): a geracao viaja ate o host e volta no ack — o C confirma a
// operacao que o host diz ter executado. Os acks deste arquivo sao por
// pedido postado (ackPostada); a fila do host e imitada como a sonda do
// revisor (evidencias/sol-f65r4/host-queue-probe.c):
//   "fila"          A em curso, B pendente, C enfileirado, D sobrescreve C:
//                   os acks de A e B sao superados e o de D confirma;
//   "falha"         EV_BUSCA_FALHOU aposenta a operacao e o proximo seek
//                   confirma sem esperar o prazo;
//   "mesmo-depois"  seek novo ao mesmo alvo armado DEPOIS da classificacao
//                   do ack velho nao e confirmado por ele;
//   "prazo-depois"  o -2 do prazo gravado depois da classificacao nao apaga
//                   a confirmacao ja feita na mesma trava.
#include <pthread.h>
#ifdef SOL_REAL_MKVFIX
#include "mkvfix.h"
#include <time.h>
#endif

#define main seek_main
#include "rede_tpk728_seek.c"
#undef main

static void prepararRetomadaPausada(void) {
  quedaEReabre();
#ifdef SOL_REAL_MKVFIX
  // Sem o define da fixture o mkvfix de producao sonda o transporte falso num
  // fio real: esperar o fim antes de medir o estado do seek (mf.pend solto).
  struct timespec delay = {0, 1000000};
  for (int i = 0; i < 1000 && mkvfix_estado() == MKVFIX_SONDANDO; i++)
    nanosleep(&delay, NULL);
  ok(mkvfix_estado() == MKVFIX_FALHOU, "sonda MKV de producao terminou com o transporte falso");
#endif
  posMs = 0;
  playerPronto();
  nv_tpk_video_evento(1, 720000, 0);
  bombear(3);
  ok(ultBusca == 12000, "retomada enviou seek a 12 s");
  posMs = 12000;
  ackPostada(-1);   // o host devolve a geracao do seek da retomada
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
  ackPostada(-1);
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
  ackPostada(1);   // o host executou o primeiro pedido, ja superado
  bombear(3);
  ok(video_tpk_capa() && video_busca_em_voo(), "ack de alvo velho nao rearma apos o prazo");
  posMs = 14000;
  ackPostada(2);
  ackPostada(2);
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
  ackPostada(1);
  bombear(3);
  ok(!supersede_during_ack && ultBusca == 14000 && video_tpk_capa() && video_busca_em_voo(),
     "seek novo sobrevive ao ack velho que ja tinha lido o alvo antigo");
  posMs = 14000;
  ackPostada(2);
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
  ackPostada(1);
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
  ackPostada(1); // completion of the first request for 13 s
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
  ackPostada(0);
  ok(video_tpk_capa() && video_busca_em_voo(), "ack velho de 12 s nao libera o seek pausado");
  posMs = 0;
  ackPostada(-1);
  bombear(3);
  printf("reconnect clock: host=%d pos=%.3f capa=%d busca=%d\n",
         posMs, video_pos(), video_tpk_capa(), video_busca_em_voo());
  ok(video_pos() == 0 && !video_tpk_capa(), "ack tardio de 0 s libera capa e o relogio da #710");
}

// Fila do host como em tizen-tpk/Video.cs:699-745 (sonda do revisor,
// evidencias/sol-f65r4/host-queue-probe.c): uma operacao em curso e um
// pendente so, sobrescrito pelo pedido mais recente; o ack sai da operacao
// em curso e o pendente assume na mesma continuacao.
static struct { int ms; unsigned ger; } hostAtivo = { -1, 0 }, hostPend = { -1, 0 };
static int proxPost;

static void hostZerarFila(void) {
  proxPost = nPost;                     // a retomada ja foi confirmada
  hostAtivo.ms = -1; hostAtivo.ger = 0;
  hostPend = hostAtivo;
}
static void hostTick(void) {            // o host consome um pedido postado
  if (proxPost >= nPost) abort();
  if (hostAtivo.ger) { hostPend.ms = postMs[proxPost]; hostPend.ger = postGer[proxPost]; }
  else { hostAtivo.ms = postMs[proxPost]; hostAtivo.ger = postGer[proxPost]; }
  proxPost++;
}
static void hostAceita(void) {          // ack da em curso; o pendente assume
  if (!hostAtivo.ger) abort();
  posMs = hostAtivo.ms;
  nv_tpk_video_evento(11, hostAtivo.ms, (int)hostAtivo.ger);
  hostAtivo = hostPend; hostPend.ms = -1; hostPend.ger = 0;
}

static void host_queue(void) {
  prepararRetomadaPausada();
  hostZerarFila();
  video_buscar(13); hostTick();         /* A em curso. */
  video_buscar(14); hostTick();         /* B pendente no host. */
  video_buscar(15);                     /* C postado atras da continuacao de A. */
  hostAceita();                         /* Ack de A, host inicia B. */
  ok(hostAtivo.ms == 14000, "host iniciou B antes de receber C");
  hostTick();                           /* C vira pendente enquanto B roda. */
  video_buscar(16); hostTick();         /* D substitui C no pendente do host. */
  hostAceita();                         /* Ack de B, host inicia D. */
  ok(hostAtivo.ms == 16000, "host substituiu C por D sem iniciar C");
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  hostAceita();                         /* Ack de D, dentro do prazo do host. */
  bombear(3);
  printf("host_queue: posMs=%d tocando=%d capa=%d busca=%d\n",
         posMs, video_tocando(), video_tpk_capa(), video_busca_em_voo());
  ok(posMs == 16000 && !video_tocando() && !video_tpk_capa() && !video_busca_em_voo(),
     "ack de D libera capa e busca sem esperar ack de C, que nunca iniciou");
}

static void host_falha(void) {
  prepararRetomadaPausada();
  hostZerarFila();
  video_buscar(13); hostTick();
  /* O catch generico emite EV_BUSCA_FALHOU; o finally libera buscando. */
  nv_tpk_video_evento(12, hostAtivo.ms, (int)hostAtivo.ger);
  hostAtivo.ms = -1; hostAtivo.ger = 0;
  video_buscar(14); hostTick();
  bombear(3);
  SDL_Delay(10100);
  bombear(3);
  hostAceita();
  bombear(3);
  printf("host_failure: posMs=%d tocando=%d capa=%d busca=%d\n",
         posMs, video_tocando(), video_tpk_capa(), video_busca_em_voo());
  ok(posMs == 14000 && !video_tocando() && !video_tpk_capa() && !video_busca_em_voo(),
     "seek aceito depois de EV_BUSCA_FALHOU libera capa e busca");
}

// Ponto -DNV_TEST_TPK_ACK do fio do host: o ack ja saiu da trava de
// classificacao/confirmacao. Entre isso e o fim do tratamento entram, por
// ordem forcada, o pedido novo ao mesmo alvo ou o -2 do prazo das legendas.
static int pos_ack_acao;
void tpk_teste_pos_ack_ponto(void) {
  int acao = pos_ack_acao;
  pos_ack_acao = 0;
  if (acao == 1) video_buscar(13);
  else if (acao == 2) { SDL_Delay(10100); bombear(3); }
}

static void mesmo_depois(void) {
  prepararRetomadaPausada();
  hostZerarFila();
  video_buscar(13); hostTick();
  pos_ack_acao = 1;
  hostAceita();        // ack do primeiro 13 s; o pedido novo entra depois da trava
  hostTick();          // o host recebeu o pedido novo
  bombear(3);
  printf("same_after_classification: host=%d active=%d capa=%d busca=%d\n",
         posMs, hostAtivo.ms, video_tpk_capa(), video_busca_em_voo());
  ok(hostAtivo.ms == 13000 && video_tpk_capa() && video_busca_em_voo(),
     "ack classificado antes do pedido novo nao confirma o mesmo alvo do pedido novo");
}

static void prazo_depois(void) {
  prepararRetomadaPausada();
  hostZerarFila();
  video_buscar(13); hostTick();
  bombear(3);
  pos_ack_acao = 2;
  hostAceita();
  bombear(3);
  printf("deadline_after_classification: host=%d capa=%d busca=%d\n",
         posMs, video_tpk_capa(), video_busca_em_voo());
  ok(!video_tpk_capa() && !video_busca_em_voo(),
     "prazo depois da classificacao nao descarta ack valido");
}

// F98: EV_BUSCA_FALHOU da geracao corrente na retomada pausada — o pouso
// exige -1 e o -2 da falha nao confirma; a falha aposenta a reabertura que
// esperava o ack (capa e busca soltam sem esperar o teto de 60 s).
static void falha_isolada(void) {
  prepararRetomadaPausada();
  hostZerarFila();
  video_buscar(12);
  hostTick();
  ok(hostAtivo.ms == posMs && !video_tocando(), "host pausado na posicao do seek repetido");
  unsigned antes = video_busca_falhas();
  nv_tpk_video_evento(12, hostAtivo.ms, (int)hostAtivo.ger);
  bombear(3);
  printf("isolated_failure: posMs=%d tocando=%d capa=%d busca=%d falhas=%u\n",
         posMs, video_tocando(), video_tpk_capa(), video_busca_em_voo(), video_busca_falhas() - antes);
  ok(video_busca_falhas() == antes + 1, "falha chegou ao app");
  ok(!video_tpk_capa() && !video_busca_em_voo(), "falha corrente aposenta capa e busca com host pausado no alvo");
}

int main(int argc, char **argv) {
  static const CdnlinkGanchos g = { fOrigem, fRecente, fResolver, fValidade };
  SDL_Init(0);
  recenteSim = 1;
  validadeMs = 150 * 60 * 1000;
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscarP, hInt, hJanela, hPos);
  nv_tpk_video_registrar_busca_geracao(hBuscarGerP);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_cdn_ganchos(&g);
  if (argc > 1 && !strcmp(argv[1], "prazo")) deadline();
  else if (argc > 1 && !strcmp(argv[1], "repetido")) repeated_target();
  else if (argc > 1 && !strcmp(argv[1], "supersede")) superseded_during_ack();
  else if (argc > 1 && !strcmp(argv[1], "supersede-prazo")) superseded_then_expired();
  else if (argc > 1 && !strcmp(argv[1], "relogio")) recon_clock();
  else if (argc > 1 && !strcmp(argv[1], "fila")) host_queue();
  else if (argc > 1 && !strcmp(argv[1], "falha")) host_falha();
  else if (argc > 1 && !strcmp(argv[1], "mesmo-depois")) mesmo_depois();
  else if (argc > 1 && !strcmp(argv[1], "prazo-depois")) prazo_depois();
  else if (argc > 1 && !strcmp(argv[1], "falha-isolada")) falha_isolada();
  else distinct();
  video_parar();
  return falhas != 0;
}
