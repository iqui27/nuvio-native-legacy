// Compila o ramo LS2 real no host. Precarrega os headers da plataforma antes
// de selecionar Luna; chamadas do barramento e payloads sao dublados.
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
// Antes do #undef __APPLE__: gl_compat.h (via ajustes.h) escolhe o GL do Mac.
#include "ajustes.h"
#include "webosver.h"
#undef __APPLE__
#include "../src/video.c"

static int pedidos, aceita = 1, assinaturas, respostas;
static char resposta[256];
static Filtro screensaverCallback;
static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  pedidos++;
  if (strstr(uri, "registerScreenSaverRequest")) {
    assinaturas++; screensaverCallback = cb;
  } else if (strstr(uri, "responseScreenSaverRequest")) {
    respostas++; snprintf(resposta, sizeof resposta, "%s", carga);
  }
  return aceita;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }
static void evento(const char *p, unsigned geracao) {
  aoEvento(NULL, (LSMessage *)p, (void *)(uintptr_t)geracao);
}
static int windowCalls;
static int fakeWindow(long handle, long x, long y, long w, long h, int full, long *task) {
  (void)handle; (void)x; (void)y; (void)w; (void)h; (void)full; (void)task;
  ++windowCalls; return 0;
}

int main(void) {
  // webosMaior(): a TV de 2017 (webOS 3.9) so tem o nyx e NAO e webOS 4.
  {
    FILE *f = fopen("/tmp/nv-webos-test.json", "w");
    assert(f); fputs("{\"webos_release\":\"3.9\"}", f); fclose(f);
    nv_webos_testar("/tmp/nv-webos-test.json", "/nonexistent/starfish-release");
    assert(webosMaior() == 3);
    assert(!dvVersaoLiberada(webosMaior()));   // a porta do Dolby Vision em MKV
    assert(dvVersaoLiberada(4));
    remove("/tmp/nv-webos-test.json");
    puts("webosMaior: nyx 3.9 sem starfish-release -> 3, Dolby Vision MKV fechado");
  }
  // Exercise the production LG path: settled geometry must not call ACB
  // every frame; PiP/fullscreen transitions must still reach the backend.
  acbJanela = fakeWindow; acb = 1; ligado = 1;
  snprintf(midia, sizeof midia, "fixture-window");
  video_janela(100, 100, 640, 360);
  for (int i = 0; i < 1000; ++i) video_janela(100, 100, 640, 360);
  assert(windowCalls == 1);
  video_janela(0, 0, 1920, 1080); assert(windowCalls == 2);
  video_janela(1400, 700, 480, 270); assert(windowCalls == 3);
  acb = 0; midia[0] = 0;
  puts("video_window_lg: 1001 stable requests -> 1 native call; transitions preserved");

  lsCall = fakeCall; lsPayload = fakePayload;
  ligado = pronto = tocando = 1; sessao = 7;
  snprintf(midia, sizeof midia, "fixture-media-id");
  video_pausar(1);
  assert(pedidos == 1 && !video_tocando() && !video_pausa_confirmada());
  evento("{\"paused\":true}", 6);
  assert(!video_pausa_confirmada()); // callback da sessao anterior
  evento("{\"paused\":true}", 7);
  assert(video_pausa_confirmada());
  video_pausar(0); assert(!video_pausa_confirmada());
  evento("{\"paused\":true}", 7); assert(!video_pausa_confirmada());
  evento("{\"playing\":true}", 7); assert(!video_pausa_confirmada());
  video_pausar(1); evento("{\"paused\":true}", 7);
  falhou = 1; assert(!video_pausa_confirmada()); falhou = 0;
  terminou = 1; assert(!video_pausa_confirmada()); terminou = 0;
  video_parar(); assert(!video_pausa_confirmada());
  pronto = ligado = 1; snprintf(midia, sizeof midia, "fixture-nova");
  evento("{\"paused\":true}", 7); assert(!video_pausa_confirmada());
  aceita = 0; video_pausar(1); assert(!video_pausa_confirmada());
  puts("video_pausa_lg: intencao, ack, geracao, play, erro e unload ok");

  // Registro no caminho real de um load valido, nunca no caminho de erro LS2.
  aceita = 1; sessao = 17; bus = (LSHandle *)(uintptr_t)1;
  midia[0] = 0; tocando = pausaPedida = terminou = falhou = 0;
  aoCarregar(NULL, (LSMessage *)"{\"mediaId\":\"fixture-load\"}",
             (void *)(uintptr_t)sessao);
  assert(assinaturas == 1 && protetorLigado && screensaverCallback);
  protegerScreensaver(); assert(assinaturas == 1); // sem duplicar assinatura
  const char *active = "{\"state\":\"Active\",\"timestamp\":1700000000123456}";
  tocando = 1;
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(respostas == 1 && strstr(resposta, "\"ack\":false") &&
         strstr(resposta, "\"timestamp\":1700000000123456}"));
  // Sem a tela de descanso do Nuvio (esmaecer real, linkado): ela segura o protetor da TV.
  esmaecer_estilo(ESM_ESTILO_ESCURECER);
  pausaPedida = 1;
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  pausaPedida = 0; tocando = 0; // pausa enviada pelo pipeline
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  tocando = 1; terminou = 1;
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  terminou = 0; falhou = 1;
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  falhou = 0; midia[0] = 0;
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  int antes = respostas;
  screensaverCallback(NULL, (LSMessage *)"{\"state\":\"Inactive\",\"text\":\"Active\",\"timestamp\":1}", NULL);
  screensaverCallback(NULL, (LSMessage *)"{\"state\":\"Active\"}", NULL);
  screensaverCallback(NULL, (LSMessage *)"{\"state\":\"Active\",\"timestamp\":null}", NULL);
  screensaverCallback(NULL, (LSMessage *)"{\"state\":\"Active\",\"timestamp\":\"1234567890123456789012345678901234567890123456789012345678901234567890\"}", NULL);
  assert(respostas == antes);
  screensaverCallback(NULL, (LSMessage *)"{ \"state\" : \"Active\", \"timestamp\" : \"exact,stamp\" }", NULL);
  assert(strstr(resposta, "\"timestamp\":\"exact,stamp\"}"));
  protetorLigado = 0; aceita = 0;
  protegerScreensaver(); assert(!protetorLigado && assinaturas == 2);
  aceita = 1; protegerScreensaver(); assert(protetorLigado && assinaturas == 3);
  // Fechar LS2 permite uma nova assinatura no proximo ciclo do backend.
  video_encerrar(); assert(!protetorLigado);
  antes = respostas;
  screensaverCallback(NULL, (LSMessage *)active, NULL); assert(respostas == antes);
  puts("video_screensaver_lg: load, assinatura, timestamp, pausas, fim, erro e ciclo ok");
}
