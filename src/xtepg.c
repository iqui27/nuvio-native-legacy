// Ver xtepg.h.
#include "xtepg.h"
#include "xtream.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define XE_N      192     // canais guardados (LRU)
#define XE_PROG   12      // programas por canal (o limit do pedido)
#define XE_FILA   24      // pedidos em espera
#define XE_VALE_S (30 * 60)
#define XE_FALHA_S (10 * 60)
// RITMO DOS PEDIDOS (dono, 01/10/2026, TCL: "50 pedido(s), 7 com programa, 37
// sem, 6 falha(s) (ultimo HTTP 429)"). Um pedido por vez (o fio e um so), com
// XE_GAP_MS entre eles; um 429 devolve o canal para o FIM da fila (os visiveis,
// pedidos por ultimo, saem antes — a fila e uma pilha) e para o fio pelo
// Retry-After do painel ou, sem ele, 2, 4, 8... ate XE_RECUO_MAX_S. O
// espacamento dobra a cada 429 e volta ao normal depois de XE_CALMA pedidos
// bons seguidos.
#define XE_GAP_MS      400
#define XE_GAP_MAX_MS  3000
#define XE_RECUO_MAX_S 60
#define XE_CALMA       10

enum { XE_VAZIO, XE_PEDIDO, XE_OK, XE_FALHOU };

typedef struct {
  char id[24];
  XtreamProg p[XE_PROG];
  int n, estado;
  time_t quando;
  unsigned uso;
} XeEnt;

// So o fio de desenho toca `ent`.
static XeEnt ent[XE_N];
static unsigned relogio;

// Entre os fios: a fila de pedidos (pilha: o ultimo pedido e o primeiro
// atendido — quem rola o guia quer os canais que estao na tela AGORA, nao os
// que passaram por ela) e os resultados prontos.
typedef struct { char id[24]; XtreamProg p[XE_PROG]; int n, st; } XeResp;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sinal = PTHREAD_COND_INITIALIZER;
static char   fila[XE_FILA][24];
static int    nFila;
static XeResp prontos[8];
static int    nProntos;
static int    fioVivo;
static unsigned geracao;              // xtepg_limpar invalida o que esta em voo

// Resumo no log a cada tanto, sem nada da pessoa (so contagens e HTTP).
static int logPedidos, logComGrade, logSem, logFalhas, logUltimoHttp;
static int log429;   // so o fio escreve; o passo le para o resumo

static void *fio(void *u) {
  int gap = XE_GAP_MS, seguidos429 = 0, bons = 0;
  (void)u;
  for (;;) {
    char id[24];
    unsigned g;
    XeResp r;
    pthread_mutex_lock(&trava);
    // O FIO NAO FICA VIVO A TOA: no Tizen cada fio ativo segura 8 MiB de
    // pilha num heap fixo de 256 MiB (tools/tizen.sh). Sem pedido por 20 s,
    // sai; o proximo xtepg_querer cria outro.
    while (!nFila) {
      struct timespec ate;
      clock_gettime(CLOCK_REALTIME, &ate);
      ate.tv_sec += 20;
      if (pthread_cond_timedwait(&sinal, &trava, &ate) != 0 && !nFila) {
        fioVivo = 0;
        pthread_mutex_unlock(&trava);
        return NULL;
      }
    }
    snprintf(id, sizeof id, "%s", fila[--nFila]);
    g = geracao;
    pthread_mutex_unlock(&trava);
    memset(&r, 0, sizeof r);
    snprintf(r.id, sizeof r.id, "%s", id);
    r.n = xtream_epg_curto(id, r.p, XE_PROG, &r.st);
    if (r.st == 429) {
      // O PAINEL PEDIU CALMA: o canal volta para o fundo da fila (nao vira
      // falha), e o fio espera o que o painel disse ou o recuo exponencial.
      int ra = xtream_ultimo_retry_after(), espera;
      seguidos429++; bons = 0; log429++;
      espera = ra > 0 ? (ra > 120 ? 120 : ra)
                      : (1 << (seguidos429 < 6 ? seguidos429 : 6));
      if (espera > XE_RECUO_MAX_S && ra <= 0) espera = XE_RECUO_MAX_S;
      gap = gap * 2 > XE_GAP_MAX_MS ? XE_GAP_MAX_MS : gap * 2;
      pthread_mutex_lock(&trava);
      if (g == geracao && nFila < XE_FILA) {
        memmove(fila[1], fila[0], sizeof fila[0] * (size_t)nFila);
        snprintf(fila[0], sizeof fila[0], "%s", id);
        nFila++;
      }
      pthread_mutex_unlock(&trava);
      printf("[xtepg] HTTP 429 do painel: pausa de %d s%s, intervalo %d ms\n", espera,
             ra > 0 ? " (Retry-After)" : "", gap);
      fflush(stdout);
      sleep((unsigned)espera);
      continue;
    }
    seguidos429 = 0;
    if (++bons >= XE_CALMA && gap > XE_GAP_MS) { gap = gap / 2 < XE_GAP_MS ? XE_GAP_MS : gap / 2; bons = 0; }
    pthread_mutex_lock(&trava);
    if (g == geracao && nProntos < (int)(sizeof prontos / sizeof prontos[0]))
      prontos[nProntos++] = r;
    pthread_mutex_unlock(&trava);
    usleep((useconds_t)gap * 1000);
  }
  return NULL;
}

static XeEnt *achar(const char *id) {
  int i;
  for (i = 0; i < XE_N; i++) if (ent[i].estado != XE_VAZIO && !strcmp(ent[i].id, id)) return &ent[i];
  return NULL;
}

static int velho(const XeEnt *e, time_t agora) {
  if (e->estado == XE_PEDIDO) return 0;
  if (e->estado == XE_FALHOU) return agora - e->quando > XE_FALHA_S;
  if (agora - e->quando > XE_VALE_S) return 1;
  // O que se guardou ja passou todo: pede a continuacao.
  return e->n > 0 && e->p[e->n - 1].fim <= agora;
}

void xtepg_querer(const char *id) {
  XeEnt *e;
  time_t agora = time(NULL);
  if (!xtream_e_id(id) || strlen(id) >= sizeof ent[0].id) return;
  e = achar(id);
  if (e) { e->uso = ++relogio; if (!velho(e, agora)) return; }
  else {
    int i, m = 0;
    for (i = 0; i < XE_N; i++) {
      if (ent[i].estado == XE_VAZIO) { m = i; break; }
      if (ent[i].estado != XE_PEDIDO && ent[i].uso < ent[m].uso) m = i;
    }
    if (ent[m].estado == XE_PEDIDO) return;   // tudo em voo: depois
    e = &ent[m];
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", id);
    e->uso = ++relogio;
  }
  e->estado = XE_PEDIDO;
  e->quando = agora;
  pthread_mutex_lock(&trava);
  if (nFila == XE_FILA) {                     // cheia: o mais antigo sai
    memmove(fila[0], fila[1], sizeof fila[0] * (XE_FILA - 1));
    nFila--;
  }
  snprintf(fila[nFila++], sizeof fila[0], "%s", id);
  if (!fioVivo) {
    pthread_t t;
    if (pthread_create(&t, NULL, fio, NULL) == 0) { pthread_detach(t); fioVivo = 1; }
  }
  pthread_cond_signal(&sinal);
  pthread_mutex_unlock(&trava);
}

void xtepg_passo(void) {
  XeResp r[8];
  int n, i;
  time_t agora = time(NULL);
  // Pedido que caiu da fila cheia fica PEDIDO para sempre sem isto: depois de
  // 2 min sem resposta, volta a poder ser pedido.
  { static time_t ultimaVarredura;
    if (agora != ultimaVarredura) {
      ultimaVarredura = agora;
      for (i = 0; i < XE_N; i++)
        if (ent[i].estado == XE_PEDIDO && agora - ent[i].quando > 300) {
          ent[i].estado = XE_FALHOU; ent[i].quando = agora - XE_FALHA_S; }
    } }
  pthread_mutex_lock(&trava);
  n = nProntos;
  memcpy(r, prontos, sizeof r[0] * (size_t)n);
  nProntos = 0;
  pthread_mutex_unlock(&trava);
  for (i = 0; i < n; i++) {
    XeEnt *e = achar(r[i].id);
    logPedidos++;
    if (r[i].n < 0) { logFalhas++; logUltimoHttp = r[i].st; }
    else if (r[i].n == 0) logSem++;
    else logComGrade++;
    if (!e) continue;                          // saiu do LRU enquanto voava
    if (r[i].n < 0) { e->estado = XE_FALHOU; e->quando = agora; continue; }
    memcpy(e->p, r[i].p, sizeof e->p[0] * (size_t)r[i].n);
    e->n = r[i].n;
    e->estado = XE_OK;
    e->quando = agora;
  }
  if (n && (logPedidos == 1 || logPedidos % 25 == 0)) {
    printf("[xtepg] grade curta: %d pedido(s), %d com programa, %d sem, %d falha(s), %d 429 recolocado(s)%s",
           logPedidos, logComGrade, logSem, logFalhas, log429, logFalhas ? "" : "\n");
    if (logFalhas) printf(" (ultimo HTTP %d)\n", logUltimoHttp);
    fflush(stdout);
  }
}

int xtepg_tem(const char *id) {
  XeEnt *e = id ? achar(id) : NULL;
  return e && e->estado == XE_OK && e->n > 0;
}

static void copiar(const XtreamProg *x, EpgProg *p) {
  p->ini = x->ini; p->fim = x->fim; p->titulo = x->titulo;
}

// Primeiro programa com fim > t, ou -1 (os do painel ja vem em ordem).
static int primeiroVivo(const XeEnt *e, time_t t) {
  int i;
  for (i = 0; i < e->n; i++) if (e->p[i].fim > t) return i;
  return -1;
}

int xtepg_agora(const char *id, time_t t, EpgProg *p) {
  XeEnt *e = id ? achar(id) : NULL;
  int m;
  if (!e || e->estado != XE_OK) return 0;
  m = primeiroVivo(e, t);
  if (m < 0 || e->p[m].ini > t) return 0;
  if (p) copiar(&e->p[m], p);
  return 1;
}

int xtepg_proximo(const char *id, time_t t, int k, EpgProg *p) {
  XeEnt *e = id ? achar(id) : NULL;
  int m;
  if (!e || e->estado != XE_OK || k < 0) return 0;
  m = primeiroVivo(e, t);
  if (m < 0) return 0;
  if (e->p[m].ini <= t) m++;
  m += k;
  if (m >= e->n) return 0;
  if (p) copiar(&e->p[m], p);
  return 1;
}

int xtepg_faixa_desde(const char *id, time_t de, time_t ate, int pular, EpgProg *out, int cap) {
  XeEnt *e = id ? achar(id) : NULL;
  int i, n = 0, v = 0;
  if (!e || e->estado != XE_OK || ate <= de || (out && cap <= 0)) return 0;
  for (i = 0; i < e->n && (!out || n < cap); i++)
    if (e->p[i].fim > de && e->p[i].ini < ate) {
      if (v++ < pular) continue;
      if (out) copiar(&e->p[i], &out[n]);
      n++;
    }
  return n;
}

int xtepg_faixa(const char *id, time_t de, time_t ate, EpgProg *out, int cap) {
  return xtepg_faixa_desde(id, de, ate, 0, out, cap);
}

void xtepg_limpar(void) {
  memset(ent, 0, sizeof ent);
  pthread_mutex_lock(&trava);
  nFila = 0; nProntos = 0; geracao++;
  pthread_mutex_unlock(&trava);
}
