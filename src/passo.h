/* MAIN-THREAD STALL FINDER (owner 16/11: turning Burn in Off froze the app for
 * 2.3 s inside the update step, `[quadro] upd=2302`, and the log could not say
 * where). The main thread names the step it enters with NV_PASSO("..."); the
 * frame loop calls passo_quadro() once per frame. A watcher thread
 * (passo_iniciar, .tpk) logs any step the main thread stays in for 300 ms or
 * more, and how long it stayed once frames come back:
 *
 *   [parado] main thread 300+ ms in "app: video_bombear: tbBombear" (frame 812)
 *   [parado] main thread back after 2310 ms; last step "app: video_bombear: tbBombear"
 *
 * A mark is one relaxed pointer store (string literals only). The waits for
 * input are marked "eventos" and never reported (the app hidden or idle). */
#ifndef NV_PASSO_H
#define NV_PASSO_H

/* Weak in every unit: tests that build one .c without main.c still link. */
__attribute__((weak)) const char *volatile nv_passo_atual;
__attribute__((weak)) volatile unsigned nv_passo_quadros;

#define NV_PASSO(s) __atomic_store_n(&nv_passo_atual, (const char *)(s), __ATOMIC_RELAXED)
static inline void passo_quadro(void) { __atomic_add_fetch(&nv_passo_quadros, 1u, __ATOMIC_RELAXED); }

/* Starts the watcher (once). Defined in main.c. */
void passo_iniciar(void);

#endif
