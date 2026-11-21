// Anel de eventos do Farol — ver farolanel.h (lote sense, issue 2; secao 2.2
// do PLANO). Modulo puro: sem SDL, sem rede, sem malloc, sem dados.h — o
// teste linka so este .c, como tests/resolver.sh faz com resolverregra.c.
//
// Relogio: monotonico proprio (clock_gettime), o mesmo motivo de marco.c —
// SDL_GetTicks depende de SDL_Init e o Farol nao pode falhar em teste que
// nunca inicializa video. farolanel_relogio() deixa o app injetar outra fonte
// (SDL_GetTicks) UMA vez no arranque e o teste injetar um relogio falso.

#include "farolanel.h"
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

_Static_assert(sizeof(FarolEvento) <= 64, "FarolEvento tem de caber em 64 B");

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static FarolEvento anel[FAROL_ANEL_N];
static int esc;                  // slot da proxima gravacao
static int qtd;                  // slots validos (0..FAROL_ANEL_N)

static uint32_t agoraMsInterno(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
  return (uint32_t)((uint64_t)ts.tv_sec * 1000ULL +
                    (uint64_t)ts.tv_nsec / 1000000ULL);
}

// Atomico porque farol_ev le de qualquer fio; quem injeta escreve uma vez no
// arranque. _Atomic sobre o typedef: o qualificador tem de ser do ponteiro,
// nao do tipo da funcao.
typedef uint32_t (*FarolRelogio)(void);
static _Atomic FarolRelogio relogio = agoraMsInterno;

void farolanel_relogio(uint32_t (*fn)(void)) {
  atomic_store_explicit(&relogio, fn ? fn : agoraMsInterno, memory_order_release);
}

static uint32_t agoraMs(void) {
  FarolRelogio fn = atomic_load_explicit(&relogio, memory_order_acquire);
  return fn ? fn() : 0;
}

static int tokenValido(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
         c == '_' || c == '.' || c == ':' || c == '+' || c == '-';
}

void farolanel_token(const char *s, char out[FAROL_TOKEN_N]) {
  size_t i, n = 0;
  int ok = 1;
  if (!s || !s[0]) ok = 0;
  else {
    for (i = 0; s[i]; i++) {
      unsigned char c = (unsigned char)s[i];
      if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
      if (!tokenValido(c)) { ok = 0; break; }
      if (n < FAROL_TOKEN_N - 1) out[n++] = (char)c;
    }
  }
  if (!ok) { out[0] = 'x'; n = 1; }
  out[n] = '\0';
}

void farol_ev(FarolMod mod, int ev, int sev, int32_t a, int32_t b, const char *tok) {
  FarolEvento *e;
  if (mod < 0 || mod >= FM_N) mod = FM_FAROL;
  if (ev <= FE_NENHUM || ev >= FE_N) ev = FE_NENHUM;
  if (sev < 0) sev = 0; else if (sev > 2) sev = 2;
  pthread_mutex_lock(&trava);
  e = &anel[esc];
  e->t_ms = agoraMs();   // dentro da trava: t_ms nao decresce na ordem do anel
  e->mod = (uint8_t)mod;
  e->sev = (uint8_t)sev;
  e->ev = (uint16_t)ev;
  e->a = a;
  e->b = b;
  farolanel_token(tok, e->tok);
  esc = (esc + 1) % FAROL_ANEL_N;
  if (qtd < FAROL_ANEL_N) qtd++;
  pthread_mutex_unlock(&trava);
}

// O mais antigo e o slot (esc - qtd) mod N; a leitura anda qtd posicoes.
static int indiceMais(int i) {
  int ini = esc - qtd;
  if (ini < 0) ini += FAROL_ANEL_N;
  return (ini + i) % FAROL_ANEL_N;
}

int farolanel_ler(FarolEvento *dst, int max, uint32_t desde_ms) {
  int i, n = 0;
  if (!dst || max <= 0) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < qtd && n < max; i++) {
    const FarolEvento *e = &anel[indiceMais(i)];
    if (!desde_ms || (int32_t)(e->t_ms - desde_ms) >= 0) dst[n++] = *e;
  }
  pthread_mutex_unlock(&trava);
  return n;
}

int farolanel_contar(int ev, uint32_t janela_ms, uint32_t agora) {
  int i, n = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < qtd; i++) {
    const FarolEvento *e = &anel[indiceMais(i)];
    int32_t d;
    if (e->ev != (uint16_t)ev) continue;
    d = (int32_t)(agora - e->t_ms);
    if (!janela_ms || (d >= 0 && (uint32_t)d <= janela_ms)) n++;
  }
  pthread_mutex_unlock(&trava);
  return n;
}

void farolanel_zerar(void) {
  pthread_mutex_lock(&trava);
  memset(anel, 0, sizeof anel);
  esc = 0;
  qtd = 0;
  pthread_mutex_unlock(&trava);
}
