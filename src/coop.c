// Fios cooperativos (fibras) para o alvo Tizen 4.0. Ver coop.h.
#ifdef NV_COOP
#include "coop.h"
#include <emscripten.h>
#include <emscripten/fiber.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// PILHA DE 1 MB POR FIBRA. O alvo com pthreads roda com 2 MB por fio
// (DEFAULT_PTHREAD_STACK_SIZE em tools/tizen.sh) e ja estourou com a pilha
// padrao do Emscripten em descoberta.c:montar. Aqui ha uma diferenca que pesa:
// em wasm2js um estouro NAO trapa, escreve por cima do heap em silencio. Por
// isso cada pilha tem uma SENTINELA no fundo, conferida a cada volta da fibra;
// se ela mudar, o app aborta com o nome do problema em vez de corromper.
#ifndef COOP_PILHA
#define COOP_PILHA (1024 * 1024)   // multiplo de 16: ver slotLivre()
#endif
// Pilha do ASYNCIFY: guarda os locais de cada quadro da pilha quando a fibra
// cede. Com ASYNCIFY completo cada quadro salva so os locais wasm (os vetores
// grandes ficam na pilha C, que nao e copiada).
#ifndef COOP_APILHA
#define COOP_APILHA (64 * 1024)
#endif
// Teto de fibras VIVAS. Nao da para ser pequeno: ha ~14 lacos permanentes
// (assrender, extras, mkvass, tex_cache, recomenda) presos em espera o tempo
// todo; um teto de 8 deixaria os trabalhos curtos na fila para sempre.
#ifndef COOP_MAX
#define COOP_MAX 48
#endif
_Static_assert(COOP_PILHA % 16 == 0, "pilha de fibra em multiplo de 16");
#define SENTINELA_TAM 64
#define SENTINELA_BYTE 0xA5
// A sentinela fica A 256 BYTES do fundo, e nao no fundo: ali o Emscripten
// grava o cookie dele quando o build tem -sSTACK_OVERFLOW_CHECK (diagnostico),
// e os dois se acusariam de estouro um ao outro.
#define SENTINELA_POS 256

typedef struct Slot Slot;
typedef struct Fio {
  void *(*fn)(void *);
  int (*fnSdl)(void *);
  void *arg;
  void *ret;
  volatile int fim;
  int destacado;
  int id;
  Slot *slot;
  // Condicao de espera: pronto quando *esperaP != esperaV ou quando o prazo
  // (ms de emscripten_get_now) passa. Sem nenhum dos dois: pronto ja.
  const volatile int *esperaP;
  int esperaV;
  double prazo;
  unsigned char *tls;
  struct Fio *prox;
  const char *arq;        // de onde nasceu (pthread_create), para o registro
  int linha;
} Fio;

struct Slot {
  emscripten_fiber_t fb;
  char *pilha;
  char *apilha;
  Fio *fio;
};

static emscripten_fiber_t principal;
static char *principalApilha;
static int iniciado;
static Fio fioPrincipal = { .id = 1, .prazo = -1 };
static Fio *atual;              // NULL = fio principal
static Slot *slots[COOP_MAX];
static int nSlots;
static Fio *filaIni, *filaFim;
static int nFila;
static int proximoId = 2;
static int vivas, pico;
static long trocas;
static double fatiaMax;
static char fatiaOrigem[96];

// ---- variaveis por fibra (_Thread_local vira global num build sem fios)
#define LOCAIS_MAX 16
static struct { void *p; size_t tam; } locais[LOCAIS_MAX];
static int nLocais;
static size_t tamLocais;
static unsigned char *tlsPrincipal;

void coop_registrar_local(void *ptr, size_t tam) {
  if (nLocais >= LOCAIS_MAX) { printf("[coop] locais demais\n"); abort(); }
  locais[nLocais].p = ptr;
  locais[nLocais].tam = tam;
  nLocais++;
  tamLocais += tam;
  free(tlsPrincipal);
  tlsPrincipal = (unsigned char *)calloc(1, tamLocais);
}

static void tlsSalvar(unsigned char *dst) {
  size_t o = 0;
  int i;
  if (!dst) return;
  for (i = 0; i < nLocais; i++) { memcpy(dst + o, locais[i].p, locais[i].tam); o += locais[i].tam; }
}
static void tlsCarregar(const unsigned char *src) {
  size_t o = 0;
  int i;
  for (i = 0; i < nLocais; i++) {
    if (src) memcpy(locais[i].p, src + o, locais[i].tam);
    else memset(locais[i].p, 0, locais[i].tam);
    o += locais[i].tam;
  }
}

static double agora(void) { return emscripten_get_now(); }

static void iniciar(void) {
  if (iniciado) return;
  iniciado = 1;
  principalApilha = (char *)malloc(COOP_APILHA);
  if (!principalApilha) { printf("[coop] sem memoria para o principal\n"); abort(); }
  emscripten_fiber_init_from_current_context(&principal, principalApilha, COOP_APILHA);
}

static void liberar(Fio *f) {
  free(f->tls);
  free(f);
}

static void entrada(void *arg);

static Slot *slotLivre(void) {
  int i;
  Slot *s;
  for (i = 0; i < nSlots; i++) if (!slots[i]->fio) return slots[i];
  if (nSlots >= COOP_MAX) return NULL;
  s = (Slot *)calloc(1, sizeof *s);
  if (!s) return NULL;
  // PILHA ALINHADA A 16, e nao malloc puro (que no Emscripten garante 8). O
  // LLVM supoe o ponteiro de pilha alinhado a 16 e usa isso: no __stdio_write
  // do musl, `iovs + 1` virou `(sp+16) | 8`. Com a pilha em 8 mod 16 o `| 8`
  // nao anda, o writev recebe o iovec vazio e o fwrite gira PARA SEMPRE
  // escrevendo 0 bytes — MEDIDO no Chrome (23/09): app congelado em
  // fil_gravar_registro -> fwrite, 100% de CPU, sem erro nenhum.
  s->pilha = (char *)aligned_alloc(16, COOP_PILHA);
  s->apilha = (char *)malloc(COOP_APILHA);
  if (!s->pilha || !s->apilha) {
    free(s->pilha); free(s->apilha); free(s);
    return NULL;
  }
  // No teste pequeno (tests/coop.sh) o malloc puro CALHAVA de vir alinhado e
  // tudo passava; no app, nao. A garantia tem de estar aqui, e nao no teste.
  if (((uintptr_t)s->pilha + COOP_PILHA) & 15) {
    printf("[coop] pilha de fibra fora do alinhamento de 16\n");
    abort();
  }
  memset(s->pilha + SENTINELA_POS, SENTINELA_BYTE, SENTINELA_TAM);
  emscripten_fiber_init(&s->fb, entrada, s, s->pilha, COOP_PILHA,
                        s->apilha, COOP_APILHA);
  slots[nSlots++] = s;
  return s;
}

static void conferirSentinela(Slot *s) {
  int i;
  for (i = 0; i < SENTINELA_TAM; i++)
    if ((unsigned char)s->pilha[SENTINELA_POS + i] != SENTINELA_BYTE) {
      printf("[coop] ESTOURO DE PILHA numa fibra (%d KB nao bastaram)\n",
             COOP_PILHA / 1024);
      fflush(stdout);
      abort();
    }
}

// Tira da fila o que couber nos slots.
static void atribuir(void) {
  while (filaIni) {
    Slot *s = slotLivre();
    Fio *f;
    if (!s) return;
    f = filaIni;
    filaIni = f->prox;
    if (!filaIni) filaFim = NULL;
    f->prox = NULL;
    nFila--;
    f->slot = s;
    s->fio = f;
  }
}

static int pronto(const Fio *f) {
  if (!f->esperaP && f->prazo < 0) return 1;
  if (f->esperaP && *f->esperaP != f->esperaV) return 1;
  if (f->prazo >= 0 && agora() >= f->prazo) return 1;
  return 0;
}

// Do principal para a fibra do slot; volta quando ela ceder ou acabar.
static void entrar(Slot *s) {
  tlsSalvar(tlsPrincipal);
  tlsCarregar(s->fio->tls);
  atual = s->fio;
  trocas++;
  {
    Fio *f = s->fio;
    const char *arq = f->arq;
    int linha = f->linha;
    double t0 = agora(), d;
    emscripten_fiber_swap(&principal, &s->fb);
    // `f` pode ter sido liberado (destacado que terminou): so o que foi
    // copiado antes da troca vale aqui.
    d = agora() - t0;
    if (d > fatiaMax) {
      const char *b = arq ? strrchr(arq, '/') : NULL;
      fatiaMax = d;
      snprintf(fatiaOrigem, sizeof fatiaOrigem, "%s:%d", b ? b + 1 : (arq ? arq : "?"), linha);
    }
  }
  atual = NULL;
  conferirSentinela(s);
}

// Da fibra atual para o principal.
static void voltar(void) {
  Fio *f = atual;
  tlsSalvar(f->tls);
  tlsCarregar(tlsPrincipal);
  emscripten_fiber_swap(&f->slot->fb, &principal);
  // De volta: entrar() ja carregou as nossas variaveis.
}

static void entrada(void *arg) {
  Slot *s = (Slot *)arg;
  for (;;) {
    Fio *f = s->fio;
    if (f->fnSdl) f->ret = (void *)(intptr_t)f->fnSdl(f->arg);
    else f->ret = f->fn(f->arg);
    s->fio = NULL;
    f->slot = NULL;
    f->fim = 1;
    vivas--;
    if (f->destacado) liberar(f);
    // Estacionada: o slot fica livre e a proxima vez que entrar() cair aqui
    // ja havera outro fio em s->fio.
    tlsCarregar(tlsPrincipal);
    emscripten_fiber_swap(&s->fb, &principal);
  }
}

// Uma volta pelas fibras prontas. Devolve quantas rodaram.
static int passo(void) {
  int i, n = 0;
  iniciar();
  atribuir();
  for (i = 0; i < nSlots; i++) {
    Slot *s = slots[i];
    if (!s->fio || !pronto(s->fio)) continue;
    entrar(s);
    n++;
  }
  if (nFila) atribuir();
  return n;
}

void coop_rodar(double orcamentoMs) {
  double t0 = agora();
  while (passo() > 0 && agora() - t0 < orcamentoMs) {}
}

int coop_em_fibra(void) { return atual != NULL; }

// O principal nao pode bloquear e nao pode ceder a si mesmo: gira as fibras e,
// quando nenhuma anda, devolve o controle ao navegador por um instante para
// XHR e timers dispararem (sao eles que destravam as fibras que esperam rede).
static void principalAguardar(void) {
  if (passo() == 0) emscripten_sleep(1);
}

int coop_esperar(const volatile int *p, int valor, double prazoMs) {
  for (;;) {
    if (p && *p != valor) return 1;
    if (prazoMs >= 0 && agora() >= prazoMs) return 0;
    if (atual) {
      atual->esperaP = p;
      atual->esperaV = valor;
      atual->prazo = prazoMs;
      voltar();
      atual->esperaP = NULL;
      atual->prazo = -1;
    } else {
      principalAguardar();
    }
    if (!p && prazoMs < 0) return 1;
  }
}

void coop_ceder(void) {
  if (atual) {
    atual->esperaP = NULL;
    atual->prazo = -1;
    voltar();
  } else {
    passo();
  }
}

double coop_fatia_max(const char **origem) {
  double d = fatiaMax;
  static char copia[96];
  memcpy(copia, fatiaOrigem, sizeof copia);
  if (origem) *origem = copia;
  fatiaMax = 0;
  fatiaOrigem[0] = 0;
  return d;
}

void coop_estatistica(int *v, int *p, long *t, int *fila) {
  if (v) *v = vivas;
  if (p) *p = pico;
  if (t) *t = trocas;
  if (fila) *fila = nFila;
}

// ---- fios
static Fio *novoFio(void) {
  Fio *f = (Fio *)calloc(1, sizeof *f);
  if (!f) return NULL;
  if (tamLocais) {
    f->tls = (unsigned char *)calloc(1, tamLocais);
    if (!f->tls) { free(f); return NULL; }
  }
  f->id = proximoId++;
  if (proximoId < 2) proximoId = 2;
  f->prazo = -1;
  if (filaFim) filaFim->prox = f; else filaIni = f;
  filaFim = f;
  nFila++;
  vivas++;
  if (vivas > pico) pico = vivas;
  return f;
}

int coop_pthread_create(pthread_t *t, const pthread_attr_t *a,
                        void *(*fn)(void *), void *arg) {
  Fio *f;
  int det = 0;
  if (a) pthread_attr_getdetachstate(a, &det);
  f = novoFio();
  if (!f) return EAGAIN;
  f->fn = fn;
  f->arg = arg;
  f->destacado = det == PTHREAD_CREATE_DETACHED;
  if (t) *t = (pthread_t)f;
  return 0;
}

int coop_pthread_create_em(pthread_t *t, const pthread_attr_t *a,
                           void *(*fn)(void *), void *arg,
                           const char *arq, int linha) {
  int r = coop_pthread_create(t, a, fn, arg);
  if (!r && t) { ((Fio *)*t)->arq = arq; ((Fio *)*t)->linha = linha; }
  return r;
}

int coop_pthread_join(pthread_t t, void **ret) {
  Fio *f = (Fio *)t;
  if (!f || f == &fioPrincipal || f == atual) return EINVAL;
  coop_esperar(&f->fim, 0, -1);
  if (ret) *ret = f->ret;
  liberar(f);
  return 0;
}

int coop_pthread_detach(pthread_t t) {
  Fio *f = (Fio *)t;
  if (!f || f == &fioPrincipal) return EINVAL;
  if (f->fim) liberar(f);
  else f->destacado = 1;
  return 0;
}

pthread_t coop_pthread_self(void) {
  return (pthread_t)(atual ? atual : &fioPrincipal);
}

// ---- mutex: guardado DENTRO do pthread_mutex_t (24 bytes no wasm32), que
// so este arquivo interpreta. PTHREAD_MUTEX_INITIALIZER e tudo zero = livre.
typedef struct { volatile int dono; int cont; int recursivo; } Trava;
_Static_assert(sizeof(Trava) <= sizeof(pthread_mutex_t), "Trava cabe no mutex");

static int idAtual(void) { return atual ? atual->id : 1; }

int coop_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) {
  Trava *x = (Trava *)m;
  int tipo = PTHREAD_MUTEX_NORMAL;
  memset(m, 0, sizeof *m);
  if (a) pthread_mutexattr_gettype(a, &tipo);
  x->recursivo = tipo == PTHREAD_MUTEX_RECURSIVE;
  return 0;
}

int coop_mutex_destroy(pthread_mutex_t *m) { (void)m; return 0; }

int coop_mutex_lock(pthread_mutex_t *m) {
  Trava *x = (Trava *)m;
  int eu = idAtual();
  if (x->dono == eu) {
    if (x->recursivo) { x->cont++; return 0; }
    // Mesmo fio travando duas vezes. Com pthreads isso seria deadlock; aqui
    // viraria espera eterna pelo proprio dono. Grita e segue.
    printf("[coop] mutex travado de novo pelo mesmo fio (%d)\n", eu);
    return EDEADLK;
  }
  while (x->dono) coop_esperar(&x->dono, x->dono, -1);
  x->dono = eu;
  x->cont = 1;
  return 0;
}

int coop_mutex_trylock(pthread_mutex_t *m) {
  Trava *x = (Trava *)m;
  int eu = idAtual();
  if (x->dono == eu && x->recursivo) { x->cont++; return 0; }
  if (x->dono) return EBUSY;
  x->dono = eu;
  x->cont = 1;
  return 0;
}

int coop_mutex_unlock(pthread_mutex_t *m) {
  Trava *x = (Trava *)m;
  if (x->recursivo && x->cont > 1) { x->cont--; return 0; }
  x->cont = 0;
  x->dono = 0;
  return 0;
}

// ---- cond: um contador de geracao dentro do pthread_cond_t. signal acorda
// TODOS (acordar sem motivo e permitido pelo POSIX; quem espera testa o
// predicado em laco).
typedef struct { volatile int seq; } Cond;

int coop_cond_init(pthread_cond_t *c, const pthread_condattr_t *a) {
  (void)a;
  memset(c, 0, sizeof *c);
  return 0;
}
int coop_cond_destroy(pthread_cond_t *c) { (void)c; return 0; }

static int condEsperar(Cond *c, pthread_mutex_t *m, double prazoMs) {
  int s = c->seq, ok;
  coop_mutex_unlock(m);
  ok = coop_esperar(&c->seq, s, prazoMs);
  coop_mutex_lock(m);
  return ok ? 0 : ETIMEDOUT;
}

int coop_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) {
  return condEsperar((Cond *)c, m, -1);
}

int coop_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                        const struct timespec *quando) {
  struct timespec ag;
  double falta;
  clock_gettime(CLOCK_REALTIME, &ag);
  falta = (double)(quando->tv_sec - ag.tv_sec) * 1000.0 +
          (double)(quando->tv_nsec - ag.tv_nsec) / 1e6;
  if (falta < 0) falta = 0;
  return condEsperar((Cond *)c, m, agora() + falta);
}

int coop_cond_signal(pthread_cond_t *c) { ((Cond *)c)->seq++; return 0; }
int coop_cond_broadcast(pthread_cond_t *c) { ((Cond *)c)->seq++; return 0; }

// ---- sono
static void dormirMs(double ms) {
  if (ms <= 0) { coop_ceder(); return; }
  coop_esperar(NULL, 0, agora() + ms);
}
int coop_usleep(unsigned us) { dormirMs(us / 1000.0); return 0; }
unsigned coop_sleep(unsigned s) { dormirMs(s * 1000.0); return 0; }
int coop_nanosleep(const struct timespec *t, struct timespec *resto) {
  if (resto) { resto->tv_sec = 0; resto->tv_nsec = 0; }
  if (t) dormirMs(t->tv_sec * 1000.0 + t->tv_nsec / 1e6);
  return 0;
}
int coop_sched_yield(void) { coop_ceder(); return 0; }

// ---- SDL
struct SDL_Thread *coop_sdl_criar(int (*fn)(void *), const char *nome, void *dado) {
  Fio *f;
  (void)nome;
  f = novoFio();
  if (!f) return NULL;
  f->fnSdl = fn;
  f->arg = dado;
  return (struct SDL_Thread *)f;
}

struct SDL_Thread *coop_sdl_criar_em(int (*fn)(void *), const char *nome, void *dado,
                                     const char *arq, int linha) {
  Fio *f = (Fio *)coop_sdl_criar(fn, nome, dado);
  if (f) { f->arq = arq; f->linha = linha; }
  return (struct SDL_Thread *)f;
}

void coop_sdl_esperar(struct SDL_Thread *t, int *st) {
  void *r = NULL;
  if (!t) return;
  coop_pthread_join((pthread_t)t, &r);
  if (st) *st = (int)(intptr_t)r;
}

void coop_sdl_soltar(struct SDL_Thread *t) {
  if (t) coop_pthread_detach((pthread_t)t);
}

struct SDL_mutex *coop_sdl_mutex(void) {
  pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof *m);
  if (!m) return NULL;
  // SDL_mutex e recursivo por contrato.
  ((Trava *)m)->recursivo = 1;
  return (struct SDL_mutex *)m;
}
void coop_sdl_mutex_fim(struct SDL_mutex *m) { free(m); }
int coop_sdl_travar(struct SDL_mutex *m) {
  return m ? coop_mutex_lock((pthread_mutex_t *)m) : -1;
}
int coop_sdl_tentar(struct SDL_mutex *m) {
  if (!m) return -1;
  return coop_mutex_trylock((pthread_mutex_t *)m) == 0 ? 0 : 1; // SDL_MUTEX_TIMEDOUT
}
int coop_sdl_soltar_trava(struct SDL_mutex *m) {
  return m ? coop_mutex_unlock((pthread_mutex_t *)m) : -1;
}

struct SDL_cond *coop_sdl_cond(void) {
  return (struct SDL_cond *)calloc(1, sizeof(pthread_cond_t));
}
void coop_sdl_cond_fim(struct SDL_cond *c) { free(c); }
int coop_sdl_cond_esperar(struct SDL_cond *c, struct SDL_mutex *m) {
  if (!c || !m) return -1;
  return condEsperar((Cond *)c, (pthread_mutex_t *)m, -1);
}
int coop_sdl_cond_esperar_ms(struct SDL_cond *c, struct SDL_mutex *m, unsigned ms) {
  if (!c || !m) return -1;
  return condEsperar((Cond *)c, (pthread_mutex_t *)m, agora() + ms) ? 1 : 0;
}
int coop_sdl_cond_sinal(struct SDL_cond *c) { if (c) ((Cond *)c)->seq++; return 0; }
int coop_sdl_cond_todos(struct SDL_cond *c) { if (c) ((Cond *)c)->seq++; return 0; }
void coop_sdl_delay(unsigned ms) { dormirMs(ms); }

#endif // NV_COOP
