// Teste isolado do escalonador de fibras de src/fio1.c (alvo VIDAA --um-fio),
// SEM o resto do app: so pthread/SDL-thread por cima de fibras cooperativas.
// Roda em Node (tests/fio1.sh chama `node build/tests-fio1/fio1.js`), sem
// canvas nem browser — por isso os "eventos assincronos" aqui sao
// setTimeout do proprio Node, no lugar do fetch() que src/rede.c usa de
// verdade; o padrao de espera (dispara e faz polling com fio1_ceder) e
// exatamente o mesmo dos dois lados.
//
// Cada verificacao imprime "PASS: ..." ou "FAIL: ...". tests/fio1.sh falha o
// build se alguma linha comecar com FAIL ou se o processo abortar.
#include <emscripten.h>
#include <pthread.h>
#include <SDL.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fio1.h"

static int falhas = 0;
#define CHECA(cond, msg) do { \
  if (cond) printf("PASS: %s\n", msg); \
  else { printf("FAIL: %s\n", msg); falhas++; } \
} while (0)

// ------------------------------------------------ produtor/consumidor (2x2)
//
// Fila circular pequena de proposito (NFILA=4 para 100 itens totais): forca
// pthread_cond_wait dos dois lados (fila cheia trava quem produz, fila vazia
// trava quem consome), nao so o caminho feliz sem contencao.
#define NFILA 4
static int fila[NFILA];
static int cabeca, cauda, cheios;
static pthread_mutex_t mtxFila = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t naoVazia = PTHREAD_COND_INITIALIZER;
static pthread_cond_t naoCheia = PTHREAD_COND_INITIALIZER;
static int totalProduzido, totalConsumido;
static long somaProduzida, somaConsumida;

static void *produtor(void *arg) {
  int n = (int)(intptr_t)arg;
  for (int i = 1; i <= n; i++) {
    pthread_mutex_lock(&mtxFila);
    while (cheios == NFILA) pthread_cond_wait(&naoCheia, &mtxFila);
    fila[cauda] = i;
    cauda = (cauda + 1) % NFILA;
    cheios++;
    somaProduzida += i;
    totalProduzido++;
    pthread_cond_signal(&naoVazia);
    pthread_mutex_unlock(&mtxFila);
  }
  return NULL;
}

static void *consumidor(void *arg) {
  int n = (int)(intptr_t)arg;
  for (int i = 0; i < n; i++) {
    pthread_mutex_lock(&mtxFila);
    while (cheios == 0) pthread_cond_wait(&naoVazia, &mtxFila);
    int v = fila[cabeca];
    cabeca = (cabeca + 1) % NFILA;
    cheios--;
    somaConsumida += v;
    totalConsumido++;
    pthread_cond_broadcast(&naoCheia);
    pthread_mutex_unlock(&mtxFila);
  }
  return NULL;
}

// ------------------------------------------------------------- SDL_Delay

static char logSDL[4];
static int logSDLN;

static int sdlFuncA(void *d) { (void)d; SDL_Delay(30); logSDL[logSDLN++] = 'A'; return 7; }
static int sdlFuncB(void *d) { (void)d; SDL_Delay(10); logSDL[logSDLN++] = 'B'; return 9; }

// ------------------------------------------------------- pthread_create detached

static volatile int detacadoRodou;
static void *funcaoDetacada(void *arg) {
  (void)arg;
  SDL_Delay(5);
  detacadoRodou = 1;
  return NULL;
}

// ------------------------------------------------------- cond_timedwait

static pthread_mutex_t mtxTimed = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condTimed = PTHREAD_COND_INITIALIZER;   // ninguem sinaliza

// --------------------------------------------------- "http" assincrono falso
//
// Mesmo padrao de src/rede.c: uma EM_JS comum (nao EM_ASYNC_JS) dispara um
// setTimeout e devolve so o id; nv_http_pronto_assinc vira teste_async_pronto
// aqui. O ponto que este teste prova e que fio1_ceder() dentro de uma fibra
// que faz esse polling DEIXA OUTRAS FIBRAS RODAREM enquanto o temporizador
// nao dispara — exatamente a propriedade de que nv_http depende para nao
// travar o quadro inteiro numa unica requisicao.
EM_JS(int, teste_async_iniciar, (int ms), {
  if (!Module.testeAsyncReqs) { Module.testeAsyncReqs = {}; Module.testeAsyncId = 1; }
  var id = Module.testeAsyncId++;
  Module.testeAsyncReqs[id] = false;
  setTimeout(function () { Module.testeAsyncReqs[id] = true; }, ms);
  return id;
});
EM_JS(int, teste_async_pronto, (int id), {
  return (Module.testeAsyncReqs && Module.testeAsyncReqs[id]) ? 1 : 0;
});

static volatile int contadorConcorrente;
static volatile int esperaAsyncPronta;

static void *fibraContadora(void *arg) {
  (void)arg;
  while (!esperaAsyncPronta) { contadorConcorrente++; fio1_ceder(); }
  return NULL;
}
static void *fibraAsync(void *arg) {
  (void)arg;
  int id = teste_async_iniciar(30);
  while (!teste_async_pronto(id)) fio1_ceder();
  esperaAsyncPronta = 1;
  return NULL;
}

int main(void) {
  printf("=== tests/fio1.c: escalonador de fibras (--um-fio) ===\n");

  // --- produtor/consumidor: 2 produtores + 2 consumidores, fila de 4 ---
  pthread_t p0, p1, c0, c1;
  pthread_create(&p0, NULL, produtor, (void *)(intptr_t)50);
  pthread_create(&p1, NULL, produtor, (void *)(intptr_t)50);
  pthread_create(&c0, NULL, consumidor, (void *)(intptr_t)50);
  pthread_create(&c1, NULL, consumidor, (void *)(intptr_t)50);
  // pthread_join chamado da RAIZ (fora de qualquer fibra): exercita o ramo
  // de fio1_ceder() que gira o escalonador em vez de trocar de fibra.
  pthread_join(p0, NULL);
  pthread_join(p1, NULL);
  pthread_join(c0, NULL);
  pthread_join(c1, NULL);
  CHECA(totalProduzido == 100 && totalConsumido == 100,
       "produtor/consumidor: 100 produzidos e 100 consumidos (mutex+cond+fila cheia/vazia)");
  CHECA(somaProduzida == somaConsumida,
       "produtor/consumidor: soma produzida bate com a soma consumida (nada perdido/duplicado)");

  // --- pthread_create detached: ninguem junta, mas roda sozinho ---
  pthread_t td; pthread_attr_t atd;
  pthread_attr_init(&atd);
  pthread_attr_setdetachstate(&atd, PTHREAD_CREATE_DETACHED);
  pthread_create(&td, &atd, funcaoDetacada, NULL);
  pthread_attr_destroy(&atd);
  while (!detacadoRodou) fio1_ceder();
  CHECA(detacadoRodou, "pthread_create com PTHREAD_CREATE_DETACHED: roda ate o fim sem join");

  // --- SDL_CreateThread/SDL_Delay: ordem pelo tempo de espera, nao pela criacao ---
  SDL_Thread *ta = SDL_CreateThread(sdlFuncA, "a", NULL);
  SDL_Thread *tb = SDL_CreateThread(sdlFuncB, "b", NULL);
  int statusA = 0, statusB = 0;
  SDL_WaitThread(ta, &statusA);
  SDL_WaitThread(tb, &statusB);
  CHECA(logSDLN == 2 && logSDL[0] == 'B' && logSDL[1] == 'A',
       "SDL_Delay: quem dorme menos (10ms) termina antes de quem dorme mais (30ms), apesar de criado depois");
  CHECA(statusA == 7 && statusB == 9,
       "SDL_WaitThread: devolve o retorno (int) da funcao da fibra");

  // --- pthread_cond_timedwait: expira quando ninguem sinaliza ---
  struct timespec prazo;
  clock_gettime(CLOCK_REALTIME, &prazo);
  prazo.tv_nsec += 50L * 1000000L;
  if (prazo.tv_nsec >= 1000000000L) { prazo.tv_sec++; prazo.tv_nsec -= 1000000000L; }
  pthread_mutex_lock(&mtxTimed);
  double t0 = emscripten_get_now();
  int r = pthread_cond_timedwait(&condTimed, &mtxTimed, &prazo);
  double dt = emscripten_get_now() - t0;
  pthread_mutex_unlock(&mtxTimed);
  CHECA(r == ETIMEDOUT, "pthread_cond_timedwait: devolve ETIMEDOUT quando ninguem sinaliza ate o prazo");
  CHECA(dt >= 35.0, "pthread_cond_timedwait: nao voltou muito antes do prazo pedido (~50ms)");

  // --- "http" assincrono via polling: nao trava as outras fibras ---
  pthread_t tc, taAsync;
  pthread_create(&tc, NULL, fibraContadora, NULL);
  pthread_create(&taAsync, NULL, fibraAsync, NULL);
  pthread_join(taAsync, NULL);
  pthread_join(tc, NULL);
  CHECA(contadorConcorrente > 5,
       "polling assincrono (padrao do nv_http): outra fibra roda dezenas de vezes enquanto a 1a espera");

  printf("=== %s (%d falha%s) ===\n", falhas ? "REPROVADO" : "OK", falhas, falhas == 1 ? "" : "s");
  return falhas ? 1 : 0;
}
