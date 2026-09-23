#ifndef NV_COOP_FIO_H
#define NV_COOP_FIO_H
// Incluido a forca em TODO arquivo do build Tizen 4 (tools/tizen.sh --tizen4
// passa `-include src/coop_fio.h`). Redireciona as primitivas de fio para as
// fibras de coop.c. Os cabecalhos reais vem PRIMEIRO: com as guardas de
// include, o #include que o arquivo faz depois vira no-op e as macros abaixo
// ja valem. As macros sao todas de funcao, para nao pegar nome de campo ou
// de tipo.
#ifdef NV_COOP
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "coop.h"

#define pthread_create(t, a, f, x) coop_pthread_create_em(t, a, f, x, __FILE__, __LINE__)
#define pthread_join(t, r) coop_pthread_join(t, r)
#define pthread_detach(t) coop_pthread_detach(t)
#define pthread_self() coop_pthread_self()
#define pthread_mutex_init(m, a) coop_mutex_init(m, a)
#define pthread_mutex_destroy(m) coop_mutex_destroy(m)
#define pthread_mutex_lock(m) coop_mutex_lock(m)
#define pthread_mutex_trylock(m) coop_mutex_trylock(m)
#define pthread_mutex_unlock(m) coop_mutex_unlock(m)
#define pthread_cond_init(c, a) coop_cond_init(c, a)
#define pthread_cond_destroy(c) coop_cond_destroy(c)
#define pthread_cond_wait(c, m) coop_cond_wait(c, m)
#define pthread_cond_timedwait(c, m, q) coop_cond_timedwait(c, m, q)
#define pthread_cond_signal(c) coop_cond_signal(c)
#define pthread_cond_broadcast(c) coop_cond_broadcast(c)
#define usleep(u) coop_usleep(u)
#define sleep(s) coop_sleep(s)
#define nanosleep(t, r) coop_nanosleep(t, r)
#define sched_yield() coop_sched_yield()

#define SDL_CreateThread(f, n, d) coop_sdl_criar_em(f, n, d, __FILE__, __LINE__)
#define SDL_WaitThread(t, s) coop_sdl_esperar(t, s)
#define SDL_DetachThread(t) coop_sdl_soltar(t)
#define SDL_SetThreadPriority(p) 0
#define SDL_CreateMutex() coop_sdl_mutex()
#define SDL_DestroyMutex(m) coop_sdl_mutex_fim(m)
#define SDL_LockMutex(m) coop_sdl_travar(m)
#define SDL_TryLockMutex(m) coop_sdl_tentar(m)
#define SDL_UnlockMutex(m) coop_sdl_soltar_trava(m)
#define SDL_CreateCond() coop_sdl_cond()
#define SDL_DestroyCond(c) coop_sdl_cond_fim(c)
#define SDL_CondWait(c, m) coop_sdl_cond_esperar(c, m)
#define SDL_CondWaitTimeout(c, m, ms) coop_sdl_cond_esperar_ms(c, m, ms)
#define SDL_CondSignal(c) coop_sdl_cond_sinal(c)
#define SDL_CondBroadcast(c) coop_sdl_cond_todos(c)
#define SDL_Delay(ms) coop_sdl_delay(ms)
#endif
#endif
