// Compila o ramo LS2 real no host. Precarrega os headers da plataforma antes
// de selecionar Luna; chamadas do barramento e payloads sao dublados.
#include "video.h"
#include "nvtmp.h"
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
#include "ajustes.h" // Load host GL headers before selecting the real LS2 branch.
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
static char resposta[256], edgeColor[256];
static int seekRequests;
static double seekPosition;

static Filtro screensaverCallback;
static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  if (strstr(uri, "setSubtitleCharacterEdgeColor"))
    snprintf(edgeColor, sizeof edgeColor, "%s", carga);
  pedidos++;
  if (strstr(uri, "/seek")) {
    seekRequests++;
    seekPosition = js_num(carga, NULL, "position", -1);
  }
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
static void *fakeLoop(void *u) { return u; }
static int windowCalls;
static int fakeWindow(long handle, long x, long y, long w, long h, int full, long *task) {
  (void)handle; (void)x; (void)y; (void)w; (void)h; (void)full; (void)task;
  ++windowCalls; return 0;
}

static int cropCalls;
static int fakeCrop(long handle, long sx, long sy, long sw, long sh,
                    long dx, long dy, long dw, long dh, int full, long *task) {
  assert(handle == 1 && sx == 480 && sy == 270 && sw == 960 && sh == 540);
  assert(dx == 0 && dy == 0 && dw == 1920 && dh == 1080 && full && task);
  ++cropCalls; return 1;
}

int main(void) {
  // webosMaior(): a TV de 2017 (webOS 3.9) so tem o nyx e NAO e webOS 4.
  {
    FILE *f = fopen(nvt("/nv-webos-test.json"), "w");
    assert(f); fputs("{\"webos_release\":\"3.9\"}", f); fclose(f);
    nv_webos_testar(nvt("/nv-webos-test.json"), "/nonexistent/starfish-release");
    assert(webosMaior() == 3);
    assert(!dvVersaoLiberada(webosMaior()));   // a porta do Dolby Vision em MKV
    assert(dvVersaoLiberada(4));
    remove(nvt("/nv-webos-test.json"));
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
  bus = (LSHandle *)(uintptr_t)1;
  ligado = pronto = tocando = 1; sessao = 7;
  snprintf(midia, sizeof midia, "fixture-media-id");
  Uint64 stamp, same, before = SDL_GetTicks64();
  evento("{\"currentTime\":12500}", 7);
  assert(video_pos_amostra(&stamp) == 12.5 && stamp >= before);
  SDL_Delay(10);
  assert(video_pos_amostra(&same) == 12.5 && same == stamp);
  evento("{\"currentTime\":12500}", 6);
  assert(video_pos_amostra(&same) == 12.5 && same == stamp);
  evento("{\"currentTime\":12500}", 7);
  assert(video_pos_amostra(&same) == 12.5 && same > stamp);
  video_buscar(90);
  assert(video_pos() == 90 && video_pos_amostra(&same) == 12.5);
  seekEm = 0;
  video_velocidade(150); video_taxa_sync(950);
  assert(video_velocidade_atual() == 150 && video_taxa_efetiva() == 143);
  video_taxa_sync(1000); video_velocidade(100);
#ifdef NV_JUNTOS_TEST
  puts("video_juntos_lg: native samples, stale generation, optimistic seek and sync rate OK");
  return 0;
#endif
  video_pausar(1);
  assert(pedidos == 1 && !video_tocando() && !video_pausa_confirmada());
  evento("{\"paused\":true}", 6);
  assert(!video_pausa_confirmada()); // callback da sessao anterior
  evento("{\"paused\":true}", 7);
  assert(video_pausa_confirmada());
  // #426: destination and source crop reach ACB while paused, without LS2 play.
  acb = 1; acbJanelaCustom = fakeCrop;
  int beforeWindow = windowCalls, beforeRequests = pedidos;
  video_janela(240, 0, 1440, 1080);
  video_janela_fonte(480, 270, 960, 540, 0, 0, 1920, 1080);
  assert(windowCalls == beforeWindow + 1 && cropCalls == 1);
  assert(pedidos == beforeRequests && !video_tocando() && video_pausa_confirmada());
  acb = 0; acbJanelaCustom = NULL;
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
  {
    VideoLegendaEstilo style = {120, 0, 0, 3, 1};
    aceita = 1; ligado = 1; style.corBorda = 2;
    video_legenda_estilo(&style);
    assert(strstr(edgeColor, "\"charEdgeColor\":\"yellow\""));
    style.corBorda = -1; video_legenda_estilo(&style);
    assert(strstr(edgeColor, "\"charEdgeColor\":\"black\""));
    puts("PASS native LG outline color and default fallback");
  }

  // Registro no caminho real de um load valido, nunca no caminho de erro LS2.
  aceita = 1; sessao = 17; bus = (LSHandle *)(uintptr_t)1;
  midia[0] = 0; tocando = pausaPedida = terminou = falhou = 0;
  aoCarregar(NULL, (LSMessage *)"{\"mediaId\":\"fixture-load\"}",
             (void *)(uintptr_t)sessao);
  assert(assinaturas == 1 && protetorLigado && screensaverCallback);
  protegerScreensaver(); assert(assinaturas == 1); // sem duplicar assinatura
  const char *active = "{\"state\":\"Active\",\"timestamp\":1700000000123456}";
  evento("{\"playing\":true}", sessao);
  video_bombear(); // ultimo quadro: tocando; os proximos eventos nao desenham
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(respostas == 1 && strstr(resposta, "\"ack\":false") &&
         strstr(resposta, "\"timestamp\":1700000000123456}"));
  // Sem a tela de descanso do Nuvio (esmaecer real, linkado): ela segura o protetor da TV.
  esmaecer_estilo(ESM_ESTILO_ESCURECER);
  video_pausar(1);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  video_pausar(0); video_bombear();
  evento("{\"paused\":true}", sessao); // pausa do pipeline, sem quadro
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  evento("{\"playing\":true}", sessao); video_bombear();
  evento("{\"errorCode\":100,\"errorText\":\"Playing error\"}", sessao);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  evento("{\"playing\":true}", sessao); // retomada publicada sem quadro
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":false"));
  evento("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}", sessao);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":false")); // aviso nao para o video
  evento("{\"endOfStream\":true}", sessao);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  video_parar();
  evento("{\"playing\":true}", sessao - 1); // sessao encerrada nao revive ACK
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  // A politica tambem muda sem quadros (sem filme).
  esmaecer_estilo(ESM_ESTILO_VITRINE);
  esmaecer_escolha(ESM_PADRAO);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":false"));
  esmaecer_escolha(0);
  screensaverCallback(NULL, (LSMessage *)active, NULL);
  assert(strstr(resposta, "\"ack\":true"));
  esmaecer_escolha(ESM_PADRAO);
  esmaecer_estilo(ESM_ESTILO_ESCURECER);
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
  assert(pthread_create(&fio, NULL, fakeLoop, NULL) == 0);
  video_encerrar(); assert(!protetorLigado);
  antes = respostas;
  screensaverCallback(NULL, (LSMessage *)active, NULL); assert(respostas == antes);
  puts("video_screensaver_lg: load, assinatura, timestamp, pausas, fim, erro e ciclo ok");

  // A reload seek refused by firmware must retry the reload position, even
  // when the previous manual seek differs or the reload starts at zero.
  // The screensaver cycle above closed LS2 (video_encerrar): reopen it.
  __atomic_store_n(&encerrando, 0, __ATOMIC_RELEASE); bus = (LSHandle *)(uintptr_t)1;
  const double reloadPositions[] = {1800.0, 0.0, 0.5};
  for (unsigned i = 0; i < sizeof reloadPositions / sizeof reloadPositions[0]; ++i) {
    video_parar();
    ligado = 1; pronto = 0; acb = 0;
    snprintf(midia, sizeof midia, "fixture-reload");
    seekAlvo = 420.0;
    posAoCarregar = reloadPositions[i];
    recarregarPausa = 0;
    int before = seekRequests;
    evento("{\"loadCompleted\":true}", sessao);
    assert(seekRequests == before + 1);
    assert(seekPosition == reloadPositions[i] * 1000.0);
    evento("{\"errorCode\":700,\"errorText\":\"seek Failure\"}", sessao);
    assert(seekRetryEm && !falhou);
    seekRetryEm = SDL_GetTicks() | 1;
    SDL_Delay(2);
    video_bombear();
    assert(seekRequests == before + 2);
    assert(seekPosition == reloadPositions[i] * 1000.0);
    assert(!falhou);
  }
  puts("video_reload_lg: refused seeks retry the reload position, including zero");
}
