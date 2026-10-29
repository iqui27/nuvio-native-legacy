#ifndef NV_FIO1_H
#define NV_FIO1_H
// Escalonador cooperativo de "fios verdes" para o alvo VIDAA de UM SO FIO
// (NV_UM_FIO). So existe quando NV_UM_FIO esta definido: o build mt/ usa
// pthreads de verdade (-pthread) e nunca inclui este arquivo em nada.
//
// POR QUE ISTO EXISTE: ver a nota grande em tools/tizen.sh sobre --um-fio. Em
// resumo, uma pagina VIDAA hospedada pode nao ter SharedArrayBuffer isolado
// (COOP/COEP), e sem ele -pthread nem carrega. O app tem 76 pontos de
// pthread_create espalhados por 38 arquivos que NINGUEM vai reescrever para
// um modelo de callback so para rodar sem SharedArrayBuffer — em vez disso,
// este arquivo IMPERSONA pthread/SDL-thread por cima de fibras cooperativas
// (emscripten/fiber.h, que precisa de ASYNCIFY mas nao de -pthread), e
// tools/tizen.sh troca as chamadas por --wrap no link. O codigo dos 38
// arquivos fica INTOCADO.
//
// O QUE UMA FIBRA E AQUI: uma pilha C propria + uma pilha do asyncify
// propria, trocadas por emscripten_fiber_swap. So uma fibra roda C por vez —
// nao ha paralelismo real, so alternancia cooperativa, exatamente como um
// select()/epoll de fios verdes em qualquer runtime de linguagem com
// goroutine/green-thread. Onde o codigo original bloqueava esperando outro
// FIO DE VERDADE, aqui ele cede (fio1_ceder) para o ESCALONADOR, que escolhe
// a proxima fibra pronta.
#ifdef NV_UM_FIO

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Chamada UMA VEZ POR QUADRO, do fio principal (main.c, junto de
// nv_ceder_quadro — ver o gancho la). Roda fibras prontas em round-robin ate
// o orcamento (em ms) estourar ou a fila de prontas esvaziar. Faz tambem a
// primeira inicializacao preguicosa do "fio raiz" (o proprio main(), como uma
// fibra especial que representa "fora de qualquer fibra").
void fio1_rodar(double orcamento_ms);

// Cede o controle uma vez. De DENTRO de uma fibra, isso troca para o
// escalonador (que decide o proximo a rodar). De FORA de qualquer fibra
// (chamado do proprio main(), por exemplo dentro de um pthread_join ou de um
// pthread_mutex_lock contestado que caiu no fio principal), gira o
// escalonador por uma fatia pequena — nunca devolve ao navegador, porque
// quem chamou esta BLOQUEADO esperando uma condicao, exatamente como um
// pthread_join de verdade bloquearia o fio principal.
void fio1_ceder(void);

// Relogio monotonico em ms (emscripten_get_now por baixo). Usado para sono
// (SDL_Delay/usleep/nanosleep) e para o timeout de pthread_cond_timedwait.
double fio1_agora(void);

// 1 se o C que esta rodando AGORA esta dentro de uma fibra fio1 (um "fio" que
// o app pediu com pthread_create/SDL_CreateThread); 0 se e o proprio main(),
// fora de qualquer fibra. rede.c usa isto para decidir se pode ceder em vez
// de bloquear ao esperar um fetch() assincrono.
int fio1_em_fibra(void);

#ifdef __cplusplus
}
#endif
#endif /* NV_UM_FIO */
#endif /* NV_FIO1_H */
