// Teste das fibras cooperativas (src/coop.c). Roda no Node em wasm2js, igual
// ao alvo Tizen 4. Ver tests/coop.sh.
#include <emscripten.h>
#include <emscripten/stack.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { falhas++; printf("FALHOU %s:%d ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// 1. produtor/consumidor com cond
static pthread_mutex_t filaM = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t filaC = PTHREAD_COND_INITIALIZER;
static int fila[64], nFila, consumidos, soma;
static void *produtor(void *a) {
  int i;
  (void)a;
  for (i = 1; i <= 50; i++) {
    pthread_mutex_lock(&filaM);
    fila[nFila++] = i;
    pthread_cond_signal(&filaC);
    pthread_mutex_unlock(&filaM);
    if (i % 7 == 0) usleep(2000);
  }
  return NULL;
}
static void *consumidor(void *a) {
  (void)a;
  pthread_mutex_lock(&filaM);
  while (consumidos < 50) {
    while (nFila == 0) pthread_cond_wait(&filaC, &filaM);
    soma += fila[--nFila];
    consumidos++;
  }
  pthread_mutex_unlock(&filaM);
  return (void *)7;
}

// 2. mutex disputado, com o dono cedendo no meio da secao critica
static pthread_mutex_t disputa = PTHREAD_MUTEX_INITIALIZER;
static int dentro, violacoes, ordem[2], nOrdem;
static void *segurar(void *a) {
  pthread_mutex_lock(&disputa);
  if (dentro) violacoes++;
  dentro = 1;
  ordem[nOrdem++] = (int)(intptr_t)a;
  usleep(20000);          // cede segurando a trava
  dentro = 0;
  pthread_mutex_unlock(&disputa);
  return NULL;
}

// 4. variavel por fibra (e pilha alinhada a 16: o LLVM conta com isso, ver
// slotLivre em src/coop.c — com 8 o fwrite do musl girava para sempre)
static _Thread_local int minha;
static int localRuim, desalinhadas;
static void *local(void *a) {
  int v = (int)(intptr_t)a, i;
  if (emscripten_stack_get_base() % 16) desalinhadas++;
  minha = v;
  for (i = 0; i < 5; i++) {
    coop_ceder();
    if (minha != v) localRuim++;
  }
  return NULL;
}

// 5. SDL
static SDL_mutex *sm;
static SDL_cond *sc;
static int sdlPronto;
static int sdlFio(void *a) {
  (void)a;
  SDL_Delay(5);
  SDL_LockMutex(sm);
  SDL_LockMutex(sm);      // SDL_mutex e recursivo
  sdlPronto = 1;
  SDL_CondSignal(sc);
  SDL_UnlockMutex(sm);
  SDL_UnlockMutex(sm);
  return 42;
}

// 6. muitos trabalhos destacados + lacos permanentes ocupando slots
static int feitos;
static void *curto(void *a) { (void)a; usleep(1000); feitos++; return NULL; }
static pthread_mutex_t eternoM = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t eternoC = PTHREAD_COND_INITIALIZER;
static int parar, eternosVivos;
static void *eterno(void *a) {
  (void)a;
  pthread_mutex_lock(&eternoM);
  eternosVivos++;
  while (!parar) pthread_cond_wait(&eternoC, &eternoM);
  eternosVivos--;
  pthread_mutex_unlock(&eternoM);
  return NULL;
}

// 7. pilha: locais grandes como os de descoberta.c:montar
static void *pilhaGrande(void *a) {
  char buf[256 * 1024];
  int i;
  (void)a;
  memset(buf, 1, sizeof buf);
  coop_ceder();
  for (i = 0; i < (int)sizeof buf; i += 4096) if (buf[i] != 1) return (void *)1;
  return NULL;
}

int main(void) {
  pthread_t p, c, a, b, t[8], e[16], g;
  pthread_attr_t det;
  void *r = NULL;
  int i, st = 0;
  double t0;
  struct timespec q;

  coop_registrar_local(&minha, sizeof minha);

  pthread_create(&c, NULL, consumidor, NULL);
  pthread_create(&p, NULL, produtor, NULL);
  pthread_join(p, NULL);
  pthread_join(c, &r);
  CONFERE(consumidos == 50 && soma == 1275, "consumidos=%d soma=%d", consumidos, soma);
  CONFERE(r == (void *)7, "retorno do join=%p", r);

  pthread_create(&a, NULL, segurar, (void *)1);
  pthread_create(&b, NULL, segurar, (void *)2);
  pthread_join(a, NULL);
  pthread_join(b, NULL);
  CONFERE(violacoes == 0 && nOrdem == 2 && ordem[0] == 1, "violacoes=%d ordem=%d,%d", violacoes, ordem[0], ordem[1]);

  // 3. timedwait vencendo, do principal
  pthread_mutex_lock(&filaM);
  clock_gettime(CLOCK_REALTIME, &q);
  q.tv_nsec += 30 * 1000000;
  if (q.tv_nsec >= 1000000000) { q.tv_sec++; q.tv_nsec -= 1000000000; }
  t0 = emscripten_get_now();
  i = pthread_cond_timedwait(&filaC, &filaM, &q);
  pthread_mutex_unlock(&filaM);
  CONFERE(i == ETIMEDOUT, "timedwait devolveu %d", i);
  CONFERE(emscripten_get_now() - t0 >= 25, "timedwait voltou em %.1f ms", emscripten_get_now() - t0);

  for (i = 0; i < 8; i++) pthread_create(&t[i], NULL, local, (void *)(intptr_t)(i + 100));
  for (i = 0; i < 8; i++) pthread_join(t[i], NULL);
  CONFERE(localRuim == 0, "variavel por fibra vazou %d vezes", localRuim);
  CONFERE(desalinhadas == 0, "%d fibras com pilha fora do alinhamento de 16", desalinhadas);
  CONFERE(minha == 0, "principal viu minha=%d", minha);

  {
    SDL_Thread *th;
    sm = SDL_CreateMutex();
    sc = SDL_CreateCond();
    th = SDL_CreateThread(sdlFio, "teste", NULL);
    SDL_LockMutex(sm);
    while (!sdlPronto) SDL_CondWait(sc, sm);
    SDL_UnlockMutex(sm);
    SDL_WaitThread(th, &st);
    CONFERE(st == 42, "SDL_WaitThread status=%d", st);
    SDL_DestroyCond(sc);
    SDL_DestroyMutex(sm);
  }

  for (i = 0; i < 16; i++) pthread_create(&e[i], NULL, eterno, NULL);
  pthread_attr_init(&det);
  pthread_attr_setdetachstate(&det, PTHREAD_CREATE_DETACHED);
  for (i = 0; i < 80; i++) { pthread_t x; pthread_create(&x, &det, curto, NULL); }
  pthread_attr_destroy(&det);
  t0 = emscripten_get_now();
  while (feitos < 80 && emscripten_get_now() - t0 < 5000) SDL_Delay(5);
  CONFERE(feitos == 80, "curtos feitos=%d", feitos);
  CONFERE(eternosVivos == 16, "eternos vivos=%d", eternosVivos);
  pthread_mutex_lock(&eternoM);
  parar = 1;
  pthread_cond_broadcast(&eternoC);
  pthread_mutex_unlock(&eternoM);
  for (i = 0; i < 16; i++) pthread_join(e[i], NULL);
  CONFERE(eternosVivos == 0, "eternos que nao sairam=%d", eternosVivos);

  pthread_create(&g, NULL, pilhaGrande, NULL);
  pthread_join(g, &r);
  CONFERE(r == NULL, "pilha grande corrompida");

  {
    int vivas, pico, fila;
    long trocas;
    coop_estatistica(&vivas, &pico, &trocas, &fila);
    printf("coop: vivas=%d pico=%d trocas=%ld fila=%d\n", vivas, pico, trocas, fila);
    CONFERE(vivas == 0 && fila == 0, "sobrou fibra viva=%d fila=%d", vivas, fila);
  }

  printf(falhas ? "coop: %d FALHA(S)\n" : "coop: tudo certo\n", falhas);
  fflush(stdout);
  emscripten_force_exit(falhas ? 1 : 0);
  return 0;
}
