// O PONTO DE PARTIDA com a tela do DV cobrindo (dvtela.h) e a sonda do MKV em
// nova tentativa (video_dvretry.h). A tela so silencia o player da TV: o HDR10
// segue andando por baixo dela durante as esperas de 3/8/20 s, e ninguem viu
// esse trecho. Entao:
//   * o caminho do DV, quando abre depois de uma nova tentativa, comeca no
//     ponto de partida (a retomada pedida com a tela de pe), nao em posSeg;
//   * o HDR10 que fica (sonda desistiu, perfil, audio, ou o botao HDR10) volta
//     ao ponto de partida antes de a tela sair;
//   * a recarga interna (pipeline morto) nao muda o ponto de partida;
//   * sem a tela (mini player, canal), nada muda.
// Compila o ramo LS2 real de video.c; barramento e motor do DV dublados.
#include "video.h"
#include "video_escala.h"
#include "video_reconexao.h"
#include "idioma.h"
#include "linguas.h"
#include "marco.h"
#include "mkv.h"
#include "mkvass.h"
#include "js.h"
#include "lsregistro.h"
#include "rede.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include "dts/dts_playback.h"
// Antes do #undef __APPLE__: gl_compat.h (via ajustes.h) escolhe o GL do Mac.
#include "ajustes.h"
#include "legenda.h"
#include "dados.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"
// O motor do DV de verdade abriria rede e fios: aqui so registra a partida.
#define dts_playback_start dublePartida
#define dts_playback_close dubleFechar
static double partidaPos = -1.0;
static int partidas;
static DtsPlayback *dublePartida(const char *url, const char *headers, int audio_stream,
                                 double position, int paused, const char *window,
                                 int webos_major, const DtsTrack *selected, int ordinal,
                                 int count) {
  (void)url; (void)headers; (void)audio_stream; (void)paused; (void)window;
  (void)webos_major; (void)selected; (void)ordinal; (void)count;
  partidas++; partidaPos = position;
  return (DtsPlayback *)(uintptr_t)0x1;
}
static void dubleFechar(DtsPlayback *p) { (void)p; }
#undef __APPLE__
#include "../src/video.c"

static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }
static void evento(const char *p) {
  aoEvento(NULL, (LSMessage *)p, (void *)(uintptr_t)sessao);
}

// A fonte DV abriu no player da TV com a tela de pe; a retomada pediu 600 s, o
// seek foi feito, e o HDR10 andou 31 s por baixo da tela (3 + 8 + 20 s de
// espera das novas tentativas).
static void fonteCoberta(int cobrir) {
  dtsSessao = NULL; dtsModoDv = 0; dtsTentou = 0; dvAudioMkvOrd = -1;
  ligado = pronto = tocando = 1;
  snprintf(midia, sizeof midia, "fixture-hdr10");
  snprintf(urlAtual, sizeof urlAtual, "http://h/filme.dv.mkv");
  dvHabilitado = 1; dvNao = 0; dvSondado = 0; dvMkvPerfil = 0; dvAudios = 0;
  nv_dvsonda_zerar(&mkvRetry); mkvRetryEm = 0;
  posSeg = 0.0;
  video_dv_tela(cobrir);
  video_buscar(600.0);               // a retomada do player (player.c, prontoDesde)
  seekEm = seekEnvEm = seekRetryEm = 0;   // o seek foi e voltou
  evento("{\"currentTime\":631000}");
  assert(posSeg > 630.9 && posSeg < 631.1);
}
static void sondaDesistiu(void) {
  int t;
  while (nv_dvsonda_falhou(&mkvRetry, &t)) {}
  assert(mkvRetry.desistiu);
  dvSondado = 1;
}

int main(void) {
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1; sessao = 40;

  // 1. Tres falhas: HDR10 fica e volta ao ponto de partida.
  fonteCoberta(1);
  sondaDesistiu();
  assert(!dvPronto() && dvNao == VIDEO_DV_NAO_SONDA);
  assert(seekEm && seekAlvo > 599.9 && seekAlvo < 600.1);
  puts("dvretry_lg: sonda desistiu com a tela de pe -> HDR10 volta a 600 s (andara ate 631 s)");

  // 2. A nova tentativa leu o cabecalho: o DV nasce no ponto de partida.
  fonteCoberta(1);
  dvSondado = 1; dvMkvPerfil = 8; dvMkvBl = 1; dvMkvRpu = 1; dvMkvEl = 0;
  assert(dvPronto());
  dtsModoDv = 1; partidas = 0;
  assert(iniciarDts(-1) && partidas == 1);
  assert(partidaPos > 599.9 && partidaPos < 600.1);
  assert(posSeg > 599.9 && posSeg < 600.1);
  dtsSessao = NULL;
  puts("dvretry_lg: DV aberto na nova tentativa -> caminho parte de 600 s, nao de 631 s");

  // 3. "Assistir agora em HDR10" durante a espera: o mesmo ponto de partida.
  fonteCoberta(1);
  video_dv_recusar();
  assert(dvNao == VIDEO_DV_NAO_PESSOA && seekEm && seekAlvo > 599.9 && seekAlvo < 600.1);
  puts("dvretry_lg: botao HDR10 durante a espera -> volta a 600 s");

  // 4. Perfil 7 lido so na terceira tentativa: idem.
  fonteCoberta(1);
  dvSondado = 1; dvMkvPerfil = 7; dvMkvEl = 1;
  assert(!dvPronto() && dvNao == VIDEO_DV_NAO_PERFIL);
  assert(seekEm && seekAlvo > 599.9 && seekAlvo < 600.1);
  dvMkvEl = 0;
  puts("dvretry_lg: perfil 7 lido tarde -> volta a 600 s");

  // 5. Pipeline morto e recarregado com a tela de pe: a recarga busca a posicao
  //    de onde morreu (631 s), mas o ponto de partida continua 600 s.
  fonteCoberta(1);
  posAoCarregar = 631.0;
  evento("{\"loadCompleted\":true}");
  assert(seekAlvo > 630.9 && seekAlvo < 631.1);
  seekEm = 0;
  sondaDesistiu();
  assert(!dvPronto());
  assert(seekEm && seekAlvo > 599.9 && seekAlvo < 600.1);
  puts("dvretry_lg: recarga interna nao move o ponto de partida");

  // 6. Sem a tela (mini player, canal): a pessoa viu o HDR10 andar; nada de
  //    voltar, e o DV parte de onde o HDR10 esta (como antes).
  fonteCoberta(0);
  sondaDesistiu();
  assert(!dvPronto() && !seekEm && posSeg > 630.9);
  fonteCoberta(0);
  dvSondado = 1; dvMkvPerfil = 8; dvMkvBl = 1; dvMkvRpu = 1;
  assert(dvPronto());
  dtsModoDv = 1;
  assert(iniciarDts(-1) && partidaPos > 630.9 && partidaPos < 631.1);
  dtsSessao = NULL;
  puts("dvretry_lg: sem a tela, nada muda");

  // 7. Folga: o HDR10 andou 1 s so (sonda do trecho da pre-busca, sem rede):
  //    nao vale um seek (re-buffer) por isso.
  fonteCoberta(1);
  evento("{\"currentTime\":601000}");
  sondaDesistiu();
  assert(!dvPronto() && !seekEm);
  puts("dvretry_lg: 1 s andado nao vira seek");
  return 0;
}
