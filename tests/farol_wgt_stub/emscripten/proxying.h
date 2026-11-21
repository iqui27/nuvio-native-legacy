// emscripten/proxying.h de teste: declaracoes para o TU fechar; o teste roda
// sempre com emscripten_is_main_browser_thread() == 1, entao a fila e o
// proxy nunca sao consultados.
#ifndef NV_TEST_EMSCRIPTEN_PROXYING_H
#define NV_TEST_EMSCRIPTEN_PROXYING_H
typedef struct em_proxying_queue em_proxying_queue;
em_proxying_queue *em_proxying_queue_create(void);
int emscripten_main_runtime_thread_id(void);
void emscripten_proxy_sync(em_proxying_queue *q, int thread,
                           void (*func)(void *), void *arg);
#endif
