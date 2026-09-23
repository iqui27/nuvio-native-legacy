#ifndef NV_COOP_H
#define NV_COOP_H
// Fios cooperativos para o alvo Tizen 4.0 (Chromium M56).
//
// POR QUE EXISTE: o M56 nao tem WebAssembly nem SharedArrayBuffer, entao o
// build sai em wasm2js e SEM pthreads. O app cria ~76 fios (pthread e SDL) e
// varios deles sao lacos permanentes presos em cond_wait: rodar os fios "em
// serie" trava na primeira espera. Em vez de reescrever cada ponto, os fios
// viram FIBRAS (emscripten_fiber_* sobre ASYNCIFY) e toda espera bloqueante
// vira "ceder a vez". coop_fio.h redireciona as chamadas para ca por macro, so
// quando NV_COOP esta definido; os outros alvos nao veem nada disto.
//
// REGRAS:
//  - Fibra que espera (mutex ocupado, cond, join, sleep, rede) CEDE ao fio
//    principal. Nunca ha preempcao: laco de CPU sem espera roda ate o fim.
//  - O fio principal NUNCA bloqueia. Quando ele espera (join, mutex de fibra,
//    Delay), gira o escalonador e, se nada anda, dorme no laco de eventos do
//    navegador (emscripten_sleep) para XHR e timers poderem disparar.
//  - O escalonador roda uma vez por quadro, dentro de coop_rodar().
//
// Tudo aqui e de um fio so: nao ha corrida entre fibras fora dos pontos de
// troca, e os atomicos C11 continuam corretos.

#ifdef NV_COOP
#include <pthread.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Gira fibras prontas ate acabar o orcamento (ms). Chamar do fio principal,
// uma vez por quadro.
void coop_rodar(double orcamentoMs);
// Cede a vez. Na fibra: volta ao escalonador. No principal: uma volta dele.
void coop_ceder(void);
// 1 dentro de fibra, 0 no fio principal.
int coop_em_fibra(void);
// Espera ate *p != valor ou ate o prazo (ms de emscripten_get_now; <0 = sem
// prazo). Devolve 1 se a condicao mudou, 0 se venceu o prazo.
int coop_esperar(const volatile int *p, int valor, double prazoMs);

// Variavel _Thread_local que precisa ser por fibra: cada fibra ve a sua copia
// (zerada ao nascer, como um fio novo). Registrar ANTES da primeira fibra.
void coop_registrar_local(void *ptr, size_t tam);

// Numeros para o registro: fibras vivas, pico e trocas.
void coop_estatistica(int *vivas, int *pico, long *trocas, int *fila);
// A fatia mais longa desde a ultima chamada (ms) e de onde nasceu o fio que a
// rodou ("arquivo.c:linha" do pthread_create). Zera ao ler. E o numero que diz
// quem precisa de coop_ceder(): fibra nao e preemptada.
double coop_fatia_max(const char **origem);

int coop_pthread_create(pthread_t *t, const pthread_attr_t *a,
                        void *(*fn)(void *), void *arg);
int coop_pthread_create_em(pthread_t *t, const pthread_attr_t *a,
                           void *(*fn)(void *), void *arg,
                           const char *arq, int linha);
int coop_pthread_join(pthread_t t, void **ret);
int coop_pthread_detach(pthread_t t);
pthread_t coop_pthread_self(void);

int coop_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a);
int coop_mutex_destroy(pthread_mutex_t *m);
int coop_mutex_lock(pthread_mutex_t *m);
int coop_mutex_trylock(pthread_mutex_t *m);
int coop_mutex_unlock(pthread_mutex_t *m);

int coop_cond_init(pthread_cond_t *c, const pthread_condattr_t *a);
int coop_cond_destroy(pthread_cond_t *c);
int coop_cond_wait(pthread_cond_t *c, pthread_mutex_t *m);
int coop_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                        const struct timespec *quando);
int coop_cond_signal(pthread_cond_t *c);
int coop_cond_broadcast(pthread_cond_t *c);

int coop_usleep(unsigned us);
unsigned coop_sleep(unsigned s);
int coop_nanosleep(const struct timespec *t, struct timespec *resto);
int coop_sched_yield(void);

// SDL. Tipos opacos no lado do SDL; aqui sao ponteiros para as mesmas
// estruturas do pthread acima.
struct SDL_Thread;
struct SDL_mutex;
struct SDL_cond;
struct SDL_Thread *coop_sdl_criar(int (*fn)(void *), const char *nome, void *dado);
struct SDL_Thread *coop_sdl_criar_em(int (*fn)(void *), const char *nome, void *dado,
                                     const char *arq, int linha);
void coop_sdl_esperar(struct SDL_Thread *t, int *st);
void coop_sdl_soltar(struct SDL_Thread *t);
struct SDL_mutex *coop_sdl_mutex(void);
void coop_sdl_mutex_fim(struct SDL_mutex *m);
int coop_sdl_travar(struct SDL_mutex *m);
int coop_sdl_tentar(struct SDL_mutex *m);
int coop_sdl_soltar_trava(struct SDL_mutex *m);
struct SDL_cond *coop_sdl_cond(void);
void coop_sdl_cond_fim(struct SDL_cond *c);
int coop_sdl_cond_esperar(struct SDL_cond *c, struct SDL_mutex *m);
int coop_sdl_cond_esperar_ms(struct SDL_cond *c, struct SDL_mutex *m, unsigned ms);
int coop_sdl_cond_sinal(struct SDL_cond *c);
int coop_sdl_cond_todos(struct SDL_cond *c);
void coop_sdl_delay(unsigned ms);

#ifdef __cplusplus
}
#endif
#endif // NV_COOP
#endif
