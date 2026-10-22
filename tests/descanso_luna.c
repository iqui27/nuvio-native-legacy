/* #205: a corrida do encerramento com o barramento LS2. O callback do
 * screensaver roda no fio do laco GLib; o video_encerrar dava loopParar()
 * (g_main_loop_quit, sem espera) e ia direto ao LSUnregister — o handle
 * sumia com o callback no meio do lsChamar. Este teste segura o callback
 * DENTRO do lsChamar e chama video_encerrar: sem o pthread_join, o fake do
 * unregister ve o callback em pleno voo e reprova. Compila o ramo luna real
 * de video.c (como dvretry_lg), com barramento e laco dublados. */
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
#include <signal.h>
#include <sys/wait.h>
#include <glib.h>
#include "dts/dts_playback.h"
// Antes do #undef __APPLE__: gl_compat.h (via ajustes.h) escolhe o GL do Mac.
#include "ajustes.h"
#include "legenda.h"
#include "dados.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"
#undef __APPLE__
#include "../src/video.c"

// DUBLES. O do lsCall segura o callback do screensaver DENTRO da chamada por
// pelo menos 200 ms: e a janela em que o video_encerrar antigo fazia o
// LSUnregister com o callback vivo. Flags volatile porque cruzam fios (teste
// curto; o produto usa __atomic, ver ackEstado/encerrando em video.c).
static volatile int emCallback, quit, liberado, liberouComCallback, respostas;
static char ackVisto[192];

static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  Uint32 inicio;
  (void)h; (void)cb; (void)ctx; (void)tok; (void)erro;
  if (!strstr(uri, "responseScreenSaverRequest")) return 1;
  snprintf(ackVisto, sizeof ackVisto, "%s", carga ? carga : "");
  emCallback = 1;
  inicio = SDL_GetTicks();
  while (!quit || SDL_GetTicks() - inicio < 200) usleep(1000);
  emCallback = 0;
  respostas++;
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }
static int fakeUnregister(LSHandle *h, void *e) {
  (void)h; (void)e;
  liberado = 1;
  if (emCallback) liberouComCallback = 1;   // a corrida de #205, flagrada
  return 1;
}
static void fakeRodar(void *l) {
  (void)l;
  aoPedidoScreensaver(NULL,
      (LSMessage *)(uintptr_t)"{\"state\":\"Active\",\"timestamp\":1730000000000}",
      NULL);
  while (!quit) usleep(1000);
}
static void fakeParar(void *l) { (void)l; quit = 1; }
static unsigned fakeAgendar(int priority, int (*cb)(void *), void *data,
                            void (*destroy)(void *)) {
  (void)priority; (void)destroy;
  cb(data); return 1;
}
// Encerrar precisa agendar o quit ANTES de o fio entrar no GLib real.
static void delayedRodar(void *l) {
  while (!__atomic_load_n(&encerrando, __ATOMIC_ACQUIRE)) usleep(1000);
  usleep(50000);
  g_main_loop_run(l);
}

int main(void) {
  lsPayload = fakePayload; lsCall = fakeCall; lsUnregister = fakeUnregister;
  alarm(10); // join travado reprova, sem prender o runner
  loopRodar = fakeRodar; loopParar = fakeParar; loopAgendar = fakeAgendar;
  bus = (LSHandle *)(uintptr_t)1; laco = (void *)(uintptr_t)1;
  ligado = 1; protetorLigado = 1;
  snprintf(midia, sizeof midia, "fixture-filme");
  ackMidia = 1;
  tocando = 1; pronto = 1; pausaPedida = 0; terminou = 0; falhou = 0;
  video_bombear();   // publica o retrato do ACK (um quadro do fio de desenho)
  assert(pthread_create(&fio, NULL, rodarLaco, NULL) == 0);
  // Espera o callback estar DENTRO do lsChamar, como num pedido Active real.
  for (int i = 0; i < 3000 && !emCallback; i++) usleep(1000);
  assert(emCallback);
  video_encerrar();
  // O join do fio (pos-fix) faz o unregister so acontecer com o callback
  // concluido; sem ele, o fake flagra o callback em pleno voo.
  assert(liberado && !liberouComCallback);
  assert(bus == NULL && !ligado);
  // O ACK saiu do retrato publicado: filme tocando -> ack:false (segurar).
  assert(respostas == 1 && strstr(ackVisto, "\"ack\":false"));
  assert(strstr(ackVisto, "\"timestamp\":1730000000000"));
  puts("descanso luna: encerrar espera o fio do laco e so entao libera o barramento");

  laco = g_main_loop_new(NULL, FALSE);
  loopRodar = delayedRodar;
  loopParar = (void (*)(void *))g_main_loop_quit;
  loopAgendar = (unsigned (*)(int, int (*)(void *), void *, void (*)(void *)))g_idle_add_full;
  bus = (LSHandle *)(uintptr_t)1; ligado = 1; liberado = 0;
  __atomic_store_n(&encerrando, 0, __ATOMIC_RELEASE);
  // Controle negativo: quit imediato reproduz o encerramento antigo.
  pid_t child = fork();
  assert(child >= 0);
  if (!child) {
    alarm(2);
    loopAgendar = fakeAgendar; // executa quit antes do run
    assert(pthread_create(&fio, NULL, rodarLaco, NULL) == 0);
    video_encerrar();
    _exit(1); // nao deveria retornar: quit perdido
  }
  int status;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM);
  puts("descanso luna: controle negativo, quit direto trava join (timeout confirmado)");
  assert(pthread_create(&fio, NULL, rodarLaco, NULL) == 0);
  video_encerrar();
  assert(liberado && !ligado && !bus);
  g_main_loop_unref(laco); laco = NULL;
  alarm(0);
  puts("descanso luna: GLib real, inicio atrasado, quit anterior ao run sem travar join");
  return 0;
}
