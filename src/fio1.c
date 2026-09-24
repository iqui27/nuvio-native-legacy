// Implementacao do escalonador de fibras do alvo VIDAA --um-fio. So compila
// com NV_UM_FIO (ver fio1.h para o porque deste arquivo existir).
//
// ESTRATEGIA GERAL: cada "fio" que o app pede (pthread_create ou
// SDL_CreateThread) vira uma NvFibra: uma pilha C e uma pilha de asyncify
// alocadas no heap (o unico heap de 256 MiB fixos que este build tem — ver o
// comentario grande em tools/tizen.sh), trocadas por emscripten_fiber_swap.
// O "fio principal" tambem e tratado como uma fibra especial (o RAIZ, sem
// alocacao propria: emscripten_fiber_init_from_current_context captura o
// ponto de retorno certo a cada troca, entao um unico emscripten_fiber_t
// global chega).
//
// MUTEX/COND SAO GIRA-E-CEDE, NAO FILA DE ESPERA: pthread_mutex_lock que acha
// o dono ocupado so faz `while (dono) fio1_ceder();`. Isso e seguro (nao
// gasta CPU de verdade: ceder troca IMEDIATAMENTE para outra fibra pronta, o
// escalonador so volta a chamar quem estava esperando quando sobrar
// orcamento) e muito mais simples que filas de espera com bugs de acordar a
// fibra errada. Esta na mesma familia de solucao que pthread_cond_wait com
// polling: correto porque cond_wait pode acordar por engano por definicao
// (spurious wakeup), entao um laco que reconfirma a condicao e sempre exigido
// de qualquer chamador correto.
//
// IDENTIDADE == PONTEIRO: pthread_t e SDL_Thread* sao o proprio ponteiro para
// a NvFibra (ou a constante FIO1_ID_RAIZ para "o fio principal"). Nao ha
// tabela de busca por id porque nunca precisamos de uma: join/detach recebem
// de volta exatamente o ponteiro que create devolveu.
#ifdef NV_UM_FIO

#include "fio1.h"

#include <emscripten.h>
#include <emscripten/fiber.h>
#include <pthread.h>
#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// PILHAS PADRAO. 256 KB de pilha C / 64 KB de pilha de asyncify por fibra,
// bem menores que os 2 MB de DEFAULT_PTHREAD_STACK_SIZE do build mt/.
//
// POR QUE MENOR E NAO O MESMO NUMERO: no build mt/ cada pthread_create vira
// um Worker cujo wasm compartilha o MESMO heap de 256 MiB via
// SharedArrayBuffer (e por isso os comentarios de tizen.sh contam pilha de
// pthread como parte do orcamento de heap). Aqui e igual: a pilha da fibra
// tambem sai do mesmo heap fixo, so que agora com potencialmente 12-20 fibras
// vivas ao mesmo tempo (o mesmo pico de pthread_create simultaneos que o
// comentario do POOL em tizen.sh mediu). 2 MB x 20 = 40 MB so de pilha C, o
// que reabriria a mesma pressao de heap que aquele comentario descreve. 256
// KB x 20 = 5 MB, sobra muito mais para arte e catalogo.
//
// NAO MEDIDO NA TV: este numero e uma estimativa a partir do que os PROPRIOS
// pontos de pthread_attr_setstacksize deste repo already pedem explicitamente
// (mapa.c pede 256 KB pela mesma razao — "o padrao do WebAssembly e pequeno").
// Quem passa attr explicito (descoberta.c 2 MB, mapa.c 256 KB, diretor.c so
// PTHREAD_CREATE_DETACHED sem tamanho) continua recebendo exatamente o que
// pediu, is este default so vale para pthread_create(t, NULL, ...) e para
// SDL_CreateThread (que nunca leva tamanho explicito neste codigo). Uma
// pilha C estourada aqui NAO avisa (mesma familia de bug que o comentario de
// STACK_SIZE=8MB em tools/tizen.sh ja descreve para a pilha principal): se
// aparecer "memory access out of bounds" vindo de dentro de uma fibra, e
// aqui que se aumenta, com ASSERTIONS=1 para achar qual.
#define FIO1_PILHA_C_PADRAO   (256u * 1024u)
#define FIO1_PILHA_ASY_PADRAO (64u * 1024u)

// Pilha de asyncify do RAIZ (o proprio main()). Maior que a das fibras de
// proposito: com ASYNCIFY sem ASYNCIFY_ONLY (exigido para as fibras
// funcionarem em qualquer profundidade de pilha — ver tools/tizen.sh), TODA a
// cadeia de chamadas de main() ate onde quer que o quadro esteja quando cede
// fica instrumentada, e e essa cadeia inteira que a pilha de asyncify do raiz
// precisa cobrir no momento da troca. 128 KB e um palpite conservador (4x a
// pilha de uma fibra comum); NAO MEDIDO NA TV.
#define FIO1_PILHA_ASY_RAIZ (128u * 1024u)

// Fatia de tempo (ms) que uma espera bloqueante NO FIO PRINCIPAL (fora de
// qualquer fibra — por exemplo um pthread_join chamado direto do laco de
// quadro) usa para girar o escalonador antes de reconferir a condicao. Nao
// precisa ser fina: so existe para nao girar em C puro sem nunca checar o
// relogio de novo.
#define FIO1_FATIA_BLOQUEIO_MS 4.0

// Quanto tempo (ms) uma espera bloqueante no fio principal pode levar antes
// de avisar no log. Blocking no fio principal e sempre uma limitacao (o
// navegador nao pode desenhar nem processar entrada nesse meio tempo — ainda
// estamos dentro do C ininterrupto de um "quadro"), entao vale saber quando
// acontece e por quanto tempo.
#define FIO1_AVISO_BLOQUEIO_MS 100.0

typedef enum {
  FIO1_PRONTA = 0,   // na fila, esperando a vez
  FIO1_RODANDO,      // e a fibra que esta executando C agora
  FIO1_DORMINDO,      // SDL_Delay/usleep/nanosleep; acorda por tempo
  FIO1_ZUMBI          // start_routine terminou; falta juntar (ou ja foi liberada)
} NvEstadoFibra;

typedef struct NvFibra {
  emscripten_fiber_t ctx;
  unsigned char *pilhaC;
  unsigned char *pilhaAsy;
  void *(*fn)(void *);
  void *arg;
  void *retorno;
  NvEstadoFibra estado;
  double acordarEm;       // valido so quando estado == FIO1_DORMINDO
  int destacada;          // pthread_detach / SDL_DetachThread
  struct NvFibra *prox;   // encadeamento: fila de prontas OU lista de dormentes (nunca as duas)
} NvFibra;

// "Fio principal" (fora de qualquer fibra) como uma identidade distinta de
// NULL — NULL fica reservado para "mutex destravado". Nunca e desreferenciado
// como NvFibra de verdade, so comparado por igualdade de ponteiro.
#define FIO1_ID_RAIZ ((NvFibra *)1)

static emscripten_fiber_t raiz;
static unsigned char pilhaAsyRaiz[FIO1_PILHA_ASY_RAIZ];
static int raizPronta;

// Fibra que esta rodando AGORA (NULL = estamos no fio principal, fora de
// qualquer fibra). So o escalonador escreve nesta variavel.
static NvFibra *atual;

static NvFibra *filaProntosCabeca, *filaProntosCauda;
static NvFibra *listaDormindo;

static NvFibra *fio1_quem_sou_eu(void) { return atual ? atual : FIO1_ID_RAIZ; }

int fio1_em_fibra(void) { return atual != NULL; }

double fio1_agora(void) { return emscripten_get_now(); }

static void enfileirarPronta(NvFibra *f) {
  f->estado = FIO1_PRONTA;
  f->prox = NULL;
  if (filaProntosCauda) filaProntosCauda->prox = f; else filaProntosCabeca = f;
  filaProntosCauda = f;
}

static NvFibra *tirarPrimeiraPronta(void) {
  NvFibra *f = filaProntosCabeca;
  if (f) { filaProntosCabeca = f->prox; if (!filaProntosCabeca) filaProntosCauda = NULL; f->prox = NULL; }
  return f;
}

static void colocarParaDormir(NvFibra *f, double acordarEm) {
  f->estado = FIO1_DORMINDO;
  f->acordarEm = acordarEm;
  f->prox = listaDormindo;
  listaDormindo = f;
}

// Varre quem esta dormindo e devolve para a fila de prontas quem ja passou do
// horario. Lista pequena (dezenas de fibras no pico) — varredura linear a
// cada chamada e mais simples que uma fila de prioridade e nao apareceu como
// custo em nenhuma medida deste projeto.
static void acordarDormentes(void) {
  double agora = fio1_agora();
  NvFibra **p = &listaDormindo;
  while (*p) {
    NvFibra *f = *p;
    if (agora >= f->acordarEm) { *p = f->prox; enfileirarPronta(f); }
    else p = &f->prox;
  }
}

static void liberarFibra(NvFibra *f) {
  free(f->pilhaC);
  free(f->pilhaAsy);
  free(f);
}

// Ponto de entrada de TODA fibra nova (chamado por emscripten_fiber_init na
// primeira vez que o escalonador troca para ela). Ao terminar, a funcao
// original nao pode simplesmente "retornar" — o fiber da Emscripten nao tem
// para onde voltar sozinho — entao ficamos "estacionados" trocando de volta
// para quem nos chamou para sempre; o escalonador nunca mais nos poe para
// rodar porque o estado ZUMBI so entra na fila quando alguem colhe com join
// (e mesmo assim so removemos, nunca reenfileiramos).
static void fio1_trampolim(void *arg) {
  NvFibra *f = (NvFibra *)arg;
  f->retorno = f->fn(f->arg);
  f->estado = FIO1_ZUMBI;
  for (;;) emscripten_fiber_swap(&f->ctx, &raiz);
}

// PILHA C ALINHADA EM 16, e nao malloc(). O malloc do Emscripten so garante 8,
// e emscripten_fiber_init usa o topo (pilha + tamanho) como veio. O compilador
// supoe o ponteiro de pilha alinhado em 16 e troca `+ 8` por `| 8` em endereco
// de local: com a pilha 8 mod 16 o `| 8` nao soma nada. MEDIDO no app: o
// __stdio_write da musl passava ao fd_write o iovec vazio (iov_len 0) com
// iovcnt 1, recebia 0 byte e girava no `for (;;)` para sempre — a pagina
// inteira congelava ao gravar fileirasui-p1.txt (19767 B) na fibra da
// montagem da home, sp+16 = 8 mod 16. Nada disto e da fibra em si: qualquer
// codigo otimizado numa pilha desalinhada pode errar calado. Tamanho tambem
// multiplo de 16 (exigencia do aligned_alloc, e o topo sai alinhado junto).
// tests/fio1-pilha.sh.
#define FIO1_ALINHAMENTO_PILHA 16u

static NvFibra *fio1_nova_fibra(void *(*fn)(void *), void *arg, size_t pilhaC, int destacada) {
  size_t pilhaAsy = FIO1_PILHA_ASY_PADRAO;
  size_t tamC = (pilhaC ? pilhaC : FIO1_PILHA_C_PADRAO) & ~(size_t)(FIO1_ALINHAMENTO_PILHA - 1);
  NvFibra *f = (NvFibra *)calloc(1, sizeof *f);
  if (!f) return NULL;
  if (tamC < FIO1_ALINHAMENTO_PILHA) tamC = FIO1_PILHA_C_PADRAO;
  f->pilhaC = (unsigned char *)aligned_alloc(FIO1_ALINHAMENTO_PILHA, tamC);
  f->pilhaAsy = (unsigned char *)malloc(pilhaAsy);
  if (!f->pilhaC || !f->pilhaAsy) { free(f->pilhaC); free(f->pilhaAsy); free(f); return NULL; }
  f->fn = fn; f->arg = arg; f->destacada = destacada;
  emscripten_fiber_init(&f->ctx, fio1_trampolim, f,
                        f->pilhaC, tamC,
                        f->pilhaAsy, pilhaAsy);
  enfileirarPronta(f);
  return f;
}

// Roda UMA fibra ate ela ceder (ou terminar) e devolve o controle aqui. So o
// escalonador (chamado a partir do fio principal, nunca de dentro de outra
// fibra) chama isto.
static void escalonarFibra(NvFibra *f) {
  atual = f;
  f->estado = FIO1_RODANDO;
  emscripten_fiber_swap(&raiz, &f->ctx);
  atual = NULL;
  switch (f->estado) {
    case FIO1_RODANDO:
      // Nao deveria acontecer (toda saida de fio1_ceder/dormir troca o
      // estado antes de ceder), mas por seguranca tratamos como "pronta de
      // novo" em vez de perder a fibra silenciosamente.
      enfileirarPronta(f);
      break;
    case FIO1_PRONTA:
      enfileirarPronta(f);
      break;
    case FIO1_DORMINDO:
      // colocarParaDormir ja moveu para listaDormindo antes de ceder.
      break;
    case FIO1_ZUMBI:
      if (f->destacada) liberarFibra(f);
      // se nao for destacada, fica solta ate pthread_join/SDL_WaitThread
      // colher — exatamente como uma pthread real presa esperando join.
      break;
  }
}

void fio1_rodar(double orcamento_ms) {
  if (!raizPronta) {
    emscripten_fiber_init_from_current_context(&raiz, pilhaAsyRaiz, sizeof pilhaAsyRaiz);
    raizPronta = 1;
  }
  double t0 = fio1_agora();
  do {
    acordarDormentes();
    NvFibra *f = tirarPrimeiraPronta();
    if (!f) break;
    escalonarFibra(f);
  } while ((fio1_agora() - t0) < orcamento_ms);
}

void fio1_ceder(void) {
  if (atual) {
    // Dentro de uma fibra: so marcamos a INTENCAO (pronta de novo) e
    // devolvemos o controle ao escalonador — e escalonarFibra, depois que o
    // swap abaixo retornar para ELE (nao para nos), quem de fato poe esta
    // fibra de volta na fila. Enfileirar aqui tambem seria inofensivo em
    // termos de concorrencia (so um fio de JS executa por vez), mas
    // duplicaria a entrada na fila quando escalonarFibra tambem enfileira ao
    // ver o estado PRONTA — por isso so o estado muda aqui.
    atual->estado = FIO1_PRONTA;
    emscripten_fiber_swap(&atual->ctx, &raiz);
  } else {
    // Fora de qualquer fibra (o proprio main(), bloqueado numa espera — join
    // ou mutex contestado). Nao ha para quem ceder: giramos o escalonador
    // por uma fatia pequena. Quem chamou fio1_ceder num laco (todo mutex/
    // join/cond deste arquivo funciona assim) vai reconferir a condicao e
    // chamar de novo se precisar.
    fio1_rodar(FIO1_FATIA_BLOQUEIO_MS);
    // emscripten_sleep(0) NAO E REDUNDANTE COM O fio1_rodar ACIMA — ACHADO
    // RODANDO tests/fio1.c (o teste de polling assincrono TRAVAVA sem esta
    // linha, timeout em vez de terminar). O motivo: emscripten_fiber_swap
    // troca a PILHA C, mas nunca devolve o controle ao MOTOR de JS (Node ou
    // o navegador) — do ponto de vista dele, main() ainda esta dentro de UMA
    // UNICA chamada sincrona ininterrupta, nao importa quantas fibras
    // trocamos por baixo. Se a fibra que estamos esperando (via
    // pthread_join, por exemplo) estiver presa num laco `while (!pronto())
    // fio1_ceder();` tipo o de nv_http em rede.c, ela SEMPRE volta pronta de
    // novo (nunca dorme, nunca bloqueia de verdade) — fio1_rodar nunca acha
    // a fila de prontas vazia, e girar so ele para sempre nunca deixa o
    // fetch()/setTimeout/promise que ela espera RESOLVER, porque o motor de
    // JS so processa timers e microtasks quando a pilha de chamadas sincrona
    // volta a ele. emscripten_sleep(0) e uma suspensao de verdade (via
    // Asyncify, o mesmo mecanismo de nv_ceder_quadro em main.c): devolve o
    // controle ao motor de JS por uma volta, deixando timers/fetch/IDBFS em
    // voo avancar, antes de retomar aqui. So funciona em QUALQUER
    // profundidade de pilha (dentro de pthread_join, dentro de addons.c,
    // etc.) porque o --um-fio compila SEM -sASYNCIFY_ONLY — com a lista
    // restrita (do build mt/) isto abortaria com "unreachable" fora de main
    // e dados_iniciar.
    //
    // CUSTO: como nv_ceder_quadro, setTimeout(0) pode ser estrangulado para
    // ~1/s numa aba em segundo plano — uma espera bloqueante no fio
    // principal que dependa de rede pode entao demorar segundos se a TV/o
    // navegador jogar a pagina para tras nesse meio tempo. E o mesmo
    // trade-off que main.c ja aceita para o quadro inteiro; aqui so se
    // aplica a esperas bloqueantes na raiz, que ja sao o caminho raro (dai o
    // aviso de FIO1_AVISO_BLOQUEIO_MS acima).
    emscripten_sleep(0);
  }
}

static void fio1_dormir_ms(double ms) {
  if (ms <= 0.0) { fio1_ceder(); return; }
  if (atual) {
    NvFibra *eu = atual;
    colocarParaDormir(eu, fio1_agora() + ms);
    emscripten_fiber_swap(&eu->ctx, &raiz);
  } else {
    // SDL_Delay/usleep/nanosleep chamados do proprio fio principal: o laco
    // de quadro nunca faz isso (cede por rAF — ver nv_ceder_quadro em
    // main.c), mas se algum caminho de inicializacao chamar, girar o
    // escalonador em vez de travar de verdade deixa as fibras de trabalho
    // avancarem nesse meio tempo.
    double alvo = fio1_agora() + ms;
    while (fio1_agora() < alvo) fio1_rodar(FIO1_FATIA_BLOQUEIO_MS);
  }
}

// ---------------------------------------------------------------- pthread_t

int __wrap_pthread_create(pthread_t *tid, const pthread_attr_t *attr,
                          void *(*fn)(void *), void *arg) {
  size_t pilhaC = FIO1_PILHA_C_PADRAO;
  int destacada = 0;
  if (attr) {
    size_t sz = 0;
    if (pthread_attr_getstacksize((pthread_attr_t *)attr, &sz) == 0 && sz > 0) pilhaC = sz;
    int ds = 0;
    if (pthread_attr_getdetachstate((pthread_attr_t *)attr, &ds) == 0 && ds == PTHREAD_CREATE_DETACHED) destacada = 1;
  }
  NvFibra *f = fio1_nova_fibra(fn, arg, pilhaC, destacada);
  if (!f) return EAGAIN;
  if (tid) *tid = (pthread_t)(uintptr_t)f;
  return 0;
}

int __wrap_pthread_join(pthread_t tid, void **retorno) {
  NvFibra *f = (NvFibra *)(uintptr_t)tid;
  if (!f || f == FIO1_ID_RAIZ) return 0;
  int naRaiz = (atual == NULL);
  double t0 = naRaiz ? fio1_agora() : 0.0;
  int avisou = 0;
  while (f->estado != FIO1_ZUMBI) {
    fio1_ceder();
    if (naRaiz && !avisou && (fio1_agora() - t0) > FIO1_AVISO_BLOQUEIO_MS) {
      printf("[fio1] pthread_join no fio principal ha mais de %.0f ms\n", FIO1_AVISO_BLOQUEIO_MS);
      fflush(stdout);
      avisou = 1;
    }
  }
  if (retorno) *retorno = f->retorno;
  liberarFibra(f);
  return 0;
}

int __wrap_pthread_detach(pthread_t tid) {
  NvFibra *f = (NvFibra *)(uintptr_t)tid;
  if (!f || f == FIO1_ID_RAIZ) return 0;
  f->destacada = 1;
  if (f->estado == FIO1_ZUMBI) liberarFibra(f);
  return 0;
}

pthread_t __wrap_pthread_self(void) { return (pthread_t)(uintptr_t)fio1_quem_sou_eu(); }
int __wrap_pthread_equal(pthread_t a, pthread_t b) { return a == b; }

// ---------------------------------------------------------- mutex / cond

// pthread_mutex_t/pthread_cond_t do musl sao uniões opacas so para reservar
// espaco (ver PTHREAD_MUTEX_INITIALIZER = {{{0}}} em pthread.h) — como este
// build nunca ativa o pthread real do musl, ninguem mais olha para dentro
// desses bytes, e podemos reaproveitar o espaco para o nosso proprio estado.
// O initializer estatico ja zera tudo, que e exatamente "destravado"/
// "geracao 0" — por isso nenhum mutex/cond deste app (a maioria e
// PTHREAD_MUTEX_INITIALIZER estatico, nunca passa por pthread_mutex_init)
// precisa de init explicito para comecar valido.
typedef struct { NvFibra *dono; } NvMutexInterna;
typedef struct { unsigned geracao; } NvCondInterna;

_Static_assert(sizeof(NvMutexInterna) <= sizeof(pthread_mutex_t), "pthread_mutex_t pequena demais para o shim fio1");
_Static_assert(sizeof(NvCondInterna) <= sizeof(pthread_cond_t), "pthread_cond_t pequena demais para o shim fio1");

static NvMutexInterna *mutexInterno(void *m) { return (NvMutexInterna *)m; }
static NvCondInterna *condInterna(void *c) { return (NvCondInterna *)c; }

static void nv_mutex_lock(NvMutexInterna *im) {
  NvFibra *eu = fio1_quem_sou_eu();
  while (im->dono != NULL && im->dono != eu) fio1_ceder();
  im->dono = eu;
}
static int nv_mutex_trylock(NvMutexInterna *im) {
  NvFibra *eu = fio1_quem_sou_eu();
  if (im->dono != NULL && im->dono != eu) return EBUSY;
  im->dono = eu;
  return 0;
}
static void nv_mutex_unlock(NvMutexInterna *im) { im->dono = NULL; }

int __wrap_pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *attr) {
  (void)attr; // PTHREAD_MUTEX_RECURSIVE nao e usado em lugar nenhum deste
              // codigo (conferido por grep) — nao ha o que ler aqui.
  mutexInterno(m)->dono = NULL;
  return 0;
}
int __wrap_pthread_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }
int __wrap_pthread_mutex_lock(pthread_mutex_t *m) { nv_mutex_lock(mutexInterno(m)); return 0; }
int __wrap_pthread_mutex_trylock(pthread_mutex_t *m) { return nv_mutex_trylock(mutexInterno(m)); }
int __wrap_pthread_mutex_unlock(pthread_mutex_t *m) { nv_mutex_unlock(mutexInterno(m)); return 0; }

int __wrap_pthread_cond_init(pthread_cond_t *c, const pthread_condattr_t *attr) {
  (void)attr; condInterna(c)->geracao = 0; return 0;
}
int __wrap_pthread_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }
int __wrap_pthread_cond_signal(pthread_cond_t *c) { condInterna(c)->geracao++; return 0; }
int __wrap_pthread_cond_broadcast(pthread_cond_t *c) { condInterna(c)->geracao++; return 0; }

int __wrap_pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
  NvCondInterna *ic = condInterna(c);
  unsigned g = ic->geracao;
  nv_mutex_unlock(mutexInterno(m));
  while (ic->geracao == g) fio1_ceder();
  nv_mutex_lock(mutexInterno(m));
  return 0;
}

int __wrap_pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *prazo) {
  NvCondInterna *ic = condInterna(c);
  unsigned g = ic->geracao;
  int expirou = 0;
  nv_mutex_unlock(mutexInterno(m));
  // mkvass.c usa CLOCK_REALTIME (o comentario dele explica: MONOTONIC exigiria
  // pthread_condattr_setclock, que nao chama) — mesma base de tempo aqui.
  double alvoMs = prazo ? ((double)prazo->tv_sec * 1000.0 + (double)prazo->tv_nsec / 1e6) : 0.0;
  while (ic->geracao == g) {
    if (prazo) {
      struct timespec agora;
      clock_gettime(CLOCK_REALTIME, &agora);
      double agoraMs = (double)agora.tv_sec * 1000.0 + (double)agora.tv_nsec / 1e6;
      if (agoraMs >= alvoMs) { expirou = 1; break; }
    }
    fio1_ceder();
  }
  nv_mutex_lock(mutexInterno(m));
  return expirou ? ETIMEDOUT : 0;
}

// -------------------------------------------------------------- sono/yield

int __wrap_usleep(useconds_t us) { fio1_dormir_ms((double)us / 1000.0); return 0; }

int __wrap_nanosleep(const struct timespec *pedido, struct timespec *resto) {
  double ms = pedido ? ((double)pedido->tv_sec * 1000.0 + (double)pedido->tv_nsec / 1e6) : 0.0;
  fio1_dormir_ms(ms);
  if (resto) { resto->tv_sec = 0; resto->tv_nsec = 0; }
  return 0;
}

// --------------------------------------------------------------- SDL_* fio

// SDL_Thread/SDL_mutex/SDL_cond nunca sao definidos de verdade neste arquivo
// (sao opacos nos cabecalhos do SDL2 tambem) — so repassamos ponteiros para
// NvFibra/NvMutexInterna/NvCondInterna alocadas por nos, e o app nunca olha
// para dentro deles, so os devolve para SDL_WaitThread/LockMutex/etc.

typedef struct { SDL_ThreadFunction fn; void *dados; } NvPonteSDL;

static void *fio1_trampolim_sdl(void *arg) {
  NvPonteSDL p = *(NvPonteSDL *)arg;
  free(arg);
  intptr_t r = (intptr_t)p.fn(p.dados);
  return (void *)r;
}

SDL_Thread *__wrap_SDL_CreateThread(SDL_ThreadFunction fn, const char *nome, void *dados) {
  (void)nome;
  NvPonteSDL *p = (NvPonteSDL *)malloc(sizeof *p);
  if (!p) return NULL;
  p->fn = fn; p->dados = dados;
  NvFibra *f = fio1_nova_fibra(fio1_trampolim_sdl, p, FIO1_PILHA_C_PADRAO, 0);
  if (!f) { free(p); return NULL; }
  return (SDL_Thread *)f;
}

void __wrap_SDL_WaitThread(SDL_Thread *t, int *status) {
  NvFibra *f = (NvFibra *)t;
  if (!f) return;
  void *ret = NULL;
  __wrap_pthread_join((pthread_t)(uintptr_t)f, &ret);
  if (status) *status = (int)(intptr_t)ret;
}

void __wrap_SDL_DetachThread(SDL_Thread *t) {
  if (!t) return;
  __wrap_pthread_detach((pthread_t)(uintptr_t)t);
}

SDL_mutex *__wrap_SDL_CreateMutex(void) {
  NvMutexInterna *m = (NvMutexInterna *)calloc(1, sizeof *m);
  return (SDL_mutex *)m;
}
int __wrap_SDL_LockMutex(SDL_mutex *m) { nv_mutex_lock(mutexInterno(m)); return 0; }
int __wrap_SDL_TryLockMutex(SDL_mutex *m) { return nv_mutex_trylock(mutexInterno(m)); }
int __wrap_SDL_UnlockMutex(SDL_mutex *m) { nv_mutex_unlock(mutexInterno(m)); return 0; }
void __wrap_SDL_DestroyMutex(SDL_mutex *m) { free(m); }

SDL_cond *__wrap_SDL_CreateCond(void) {
  NvCondInterna *c = (NvCondInterna *)calloc(1, sizeof *c);
  return (SDL_cond *)c;
}
void __wrap_SDL_DestroyCond(SDL_cond *c) { free(c); }
int __wrap_SDL_CondSignal(SDL_cond *c) { condInterna(c)->geracao++; return 0; }
int __wrap_SDL_CondBroadcast(SDL_cond *c) { condInterna(c)->geracao++; return 0; }
int __wrap_SDL_CondWait(SDL_cond *c, SDL_mutex *m) {
  return __wrap_pthread_cond_wait((pthread_cond_t *)condInterna(c), (pthread_mutex_t *)mutexInterno(m));
}

void __wrap_SDL_Delay(Uint32 ms) { fio1_dormir_ms((double)ms); }

// --------------------------------------------- outras APIs de fio usadas

// webp.c chama emscripten_async_run_in_main_runtime_thread (que expande para
// esta funcao com o "_" no fim — ver emscripten/threading_legacy.h) SO
// quando emscripten_is_main_browser_thread() diz que nao esta no fio
// principal. A implementacao REAL desse symbol vive na parte de threading do
// runtime, que sem -pthread nem entra no link — e exatamente o "undefined
// symbol: emscripten_async_run_in_main_runtime_thread_" que o wasm-ld dava
// antes deste wrap existir (CONFERIDO: o link falhava sem
// -sERROR_ON_UNDEFINED_SYMBOLS=0 ate este wrap ser adicionado).
//
// SEM -pthread, emscripten_is_main_browser_thread() (a propria do runtime,
// nao nossa) SEMPRE devolve verdadeiro — so ha um fio de JS. Isso quer dizer
// que o ramo de webp.c que chamaria esta funcao e MORTO em tempo de
// execucao neste build: nunca deveria disparar de verdade. O wrap abaixo so
// fecha o link; se disparar mesmo assim (emscripten_is_main_browser_thread
// mudou de comportamento numa versao futura do emsdk, por exemplo), avisa no
// log em vez de travar silenciosamente sem nunca criar o Worker de decode.
void __wrap_emscripten_async_run_in_main_runtime_thread_(int sig, void *func_ptr, ...) {
  (void)sig; (void)func_ptr;
  printf("[fio1] emscripten_async_run_in_main_runtime_thread chamado sem -pthread — "
        "nao deveria (so ha um fio de JS neste build; ver comentario em fio1.c)\n");
  fflush(stdout);
}

#endif /* NV_UM_FIO */
