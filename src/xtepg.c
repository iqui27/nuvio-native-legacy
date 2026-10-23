// Ver xtepg.h.
#include "xtepg.h"
#include "xtream.h"
#include "perfisessao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdatomic.h>

#define XE_N      192     // canais guardados (LRU)
#define XE_PROG   12      // programas por canal (o limit do pedido)
#define XE_FILA   XE_N      // pedidos em espera
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
  char id[80];
  XtreamProg p[XE_PROG];
  int n, estado;
  time_t quando;
  unsigned uso, pedido;
} XeEnt;

// So o fio de desenho toca `ent`.
static XeEnt ent[XE_N];
static unsigned relogio, proximoPedido;

// Entre os fios: a fila de pedidos (pilha: o ultimo pedido e o primeiro
// atendido — quem rola o guia quer os canais que estao na tela AGORA, nao os
// que passaram por ela) e os resultados prontos.
typedef struct { char id[80]; XtreamProg p[XE_PROG]; int n, st; unsigned pedido; NvPerfilSessao sessao; } XeResp;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  sinal = PTHREAD_COND_INITIALIZER;
static char   fila[XE_FILA][80];
static unsigned filaPedido[XE_FILA];
static time_t filaDesde[XE_FILA];
static unsigned char filaTentativas[XE_FILA];
static int    nFila;
static XeResp prontos[8];
static int    nProntos;
static int    fioVivo;
static unsigned geracao;              // xtepg_limpar invalida o que esta em voo
static NvPerfilSessao sessaoFila;

// O recuo de um painel antigo nao segura os canais do perfil novo por um
// minuto. A mesma condicao da fila acorda imediatamente ao limpar a sessao.
static void pausar(unsigned ms, unsigned g, NvPerfilSessao sessao) {
  struct timespec ate;
  clock_gettime(CLOCK_REALTIME, &ate);
  ate.tv_sec += ms / 1000; ate.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (ate.tv_nsec >= 1000000000L) { ate.tv_sec++; ate.tv_nsec -= 1000000000L; }
  pthread_mutex_lock(&trava);
  while (g == geracao && nv_perfil_sessao_atual(sessao))
    if (pthread_cond_timedwait(&sinal, &trava, &ate) != 0) break;
  pthread_mutex_unlock(&trava);
}

// Resumo no log a cada tanto, sem nada da pessoa (so contagens e HTTP).
static int logPedidos, logComGrade, logSem, logFalhas, logUltimoHttp;
static atomic_int log429;   // so o fio escreve; o passo le para o resumo

static void *fio(void *u) {
  int gap = XE_GAP_MS, seguidos429 = 0, bons = 0;
  unsigned ultimaGeracao = 0;
  (void)u;
  for (;;) {
    char id[80];
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
    unsigned pedido = filaPedido[nFila];
    unsigned tentativas = filaTentativas[nFila];
    time_t desde = filaDesde[nFila];
    g = geracao;
    NvPerfilSessao sessao = sessaoFila;
    pthread_mutex_unlock(&trava);
    if (!nv_perfil_sessao_atual(sessao) || time(NULL)-desde>45) continue;
    if (ultimaGeracao != g) { gap = XE_GAP_MS; seguidos429 = bons = 0; ultimaGeracao = g; }
    memset(&r, 0, sizeof r);
    r.sessao = sessao; r.pedido = pedido;
    snprintf(r.id, sizeof r.id, "%s", id);
    r.n = xtream_epg_curto(id, r.p, XE_PROG, &r.st);
    int transiente = r.n < 0 && (r.st == 0 || r.st == 408 || r.st >= 500);
    if ((r.st == 429 && tentativas < 2) || (transiente && tentativas < 1)) {
      // O PAINEL PEDIU CALMA: o canal volta para o fundo da fila (nao vira
      // falha), e o fio espera o que o painel disse ou o recuo exponencial.
      int ra = xtream_ultimo_retry_after(), espera;
      seguidos429++; bons = 0; if(r.st==429)log429++;
      espera = r.st!=429 ? 1 : ra > 0 ? (ra > 120 ? 120 : ra)
                      : (1 << (seguidos429 < 6 ? seguidos429 : 6));
      if (espera > XE_RECUO_MAX_S && ra <= 0) espera = XE_RECUO_MAX_S;
      gap = gap * 2 > XE_GAP_MAX_MS ? XE_GAP_MAX_MS : gap * 2;
      pthread_mutex_lock(&trava);
      if (g == geracao && nv_perfil_sessao_atual(sessao) && nFila < XE_FILA) {
        memmove(fila[1], fila[0], sizeof fila[0] * (size_t)nFila);
        memmove(filaDesde+1,filaDesde,sizeof filaDesde[0]*(size_t)nFila);
        memmove(filaPedido+1, filaPedido, sizeof filaPedido[0] * (size_t)nFila);
        memmove(filaTentativas+1, filaTentativas, sizeof filaTentativas[0] * (size_t)nFila);
        snprintf(fila[0], sizeof fila[0], "%s", id);
        filaDesde[0] = desde; filaPedido[0] = pedido; filaTentativas[0] = (unsigned char)(tentativas+1);
        nFila++;
      }
      pthread_mutex_unlock(&trava);
      printf("[xtepg] HTTP %d do painel: pausa de %d s%s, intervalo %d ms\n", r.st, espera,
             ra > 0 ? " (Retry-After)" : "", gap);
      fflush(stdout);
      pausar((unsigned)espera * 1000, g, sessao);
      continue;
    }
    seguidos429 = 0;
    if (++bons >= XE_CALMA && gap > XE_GAP_MS) { gap = gap / 2 < XE_GAP_MS ? XE_GAP_MS : gap / 2; bons = 0; }
    pthread_mutex_lock(&trava);
    while (g == geracao && nv_perfil_sessao_atual(sessao) && nProntos == (int)(sizeof prontos / sizeof prontos[0]))
      pthread_cond_wait(&sinal,&trava);
    if (g == geracao && nv_perfil_sessao_atual(sessao)) prontos[nProntos++] = r;
    pthread_mutex_unlock(&trava);
    pausar((unsigned)gap, g, sessao);
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
    int i, m = -1;
    for (i = 0; i < XE_N; i++) {
      if (ent[i].estado == XE_VAZIO) { m = i; break; }
      if (ent[i].estado != XE_PEDIDO && (m < 0 || ent[i].uso < ent[m].uso)) m = i;
    }
    if (m < 0) return;   // tudo em voo: depois
    e = &ent[m];
    memset(e, 0, sizeof *e);
    snprintf(e->id, sizeof e->id, "%s", id);
    e->uso = ++relogio;
  }
  e->estado = XE_PEDIDO;
  e->quando = agora;
  e->pedido = ++proximoPedido;
  pthread_mutex_lock(&trava);
  sessaoFila = nv_perfil_sessao_ler();
  if (nFila == XE_FILA) {
    e->estado = XE_FALHOU;
    pthread_mutex_unlock(&trava); return;
  }
  // A retry after a deadline replaces queued work for the same channel.
  for(int i=0;i<nFila;) {
    if(strcmp(fila[i],id)){i++;continue;}
    nFila--;
    memmove(fila[i],fila[i+1],sizeof fila[0]*(size_t)(nFila-i));
    memmove(filaPedido+i,filaPedido+i+1,sizeof filaPedido[0]*(size_t)(nFila-i));
    memmove(filaTentativas+i,filaTentativas+i+1,sizeof filaTentativas[0]*(size_t)(nFila-i));
    memmove(filaDesde+i,filaDesde+i+1,sizeof filaDesde[0]*(size_t)(nFila-i));
  }
  snprintf(fila[nFila], sizeof fila[0], "%s", id);
  filaDesde[nFila] = agora; filaPedido[nFila] = e->pedido; filaTentativas[nFila] = 0; nFila++;
  if (!fioVivo) {
    pthread_t t;
    if (pthread_create(&t, NULL, fio, NULL) == 0) { pthread_detach(t); fioVivo = 1; }
    else { nFila--; e->estado = XE_FALHOU; }
  }
  pthread_cond_signal(&sinal);
  pthread_mutex_unlock(&trava);
}

void xtepg_passo(void) {
  XeResp r[8];
  int n, i;
  time_t agora = time(NULL);
  pthread_mutex_lock(&trava);
  n = nProntos;
  memcpy(r, prontos, sizeof r[0] * (size_t)n);
  nProntos = 0;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
  for (i = 0; i < n; i++) {
    if (!nv_perfil_sessao_atual(r[i].sessao)) continue;
    XeEnt *e = achar(r[i].id);
    logPedidos++;
    if (r[i].n < 0) { logFalhas++; logUltimoHttp = r[i].st; }
    else if (r[i].n == 0) logSem++;
    else logComGrade++;
    if (!e || e->estado != XE_PEDIDO || e->pedido != r[i].pedido) continue;                          // saiu do LRU enquanto voava
    if (r[i].n < 0) { e->estado = XE_FALHOU; e->quando = agora; continue; }
    memcpy(e->p, r[i].p, sizeof e->p[0] * (size_t)r[i].n);
    e->n = r[i].n;
    e->estado = XE_OK;
    e->quando = agora;
  }
  // Pending work has a terminal deadline, including requests waiting in the queue.
  { static time_t ultimaVarredura;
    if (agora != ultimaVarredura) {
      ultimaVarredura = agora;
      for (i = 0; i < XE_N; i++)
        if (ent[i].estado == XE_PEDIDO && agora - ent[i].quando > 45) {
          ent[i].estado = XE_FALHOU; ent[i].quando = agora; }
    } }
  if (n && (logPedidos == 1 || logPedidos % 25 == 0)) {
    printf("[xtepg] grade curta: %d pedido(s), %d com programa, %d sem, %d falha(s), %d 429 recolocado(s)%s",
           logPedidos, logComGrade, logSem, logFalhas, log429, logFalhas ? "" : "\n");
    if (logFalhas) printf(" (ultimo HTTP %d)\n", logUltimoHttp);
    fflush(stdout);
  }
}

int xtepg_carregando(const char *id) {
  XeEnt *e = id ? achar(id) : NULL;
  return e && e->estado == XE_PEDIDO;
}

int xtepg_estado(const char *id) {
  XeEnt *e = id ? achar(id) : NULL;
  if (!e) return XTEPG_PARADO;
  if (e->estado == XE_PEDIDO) return XTEPG_CARREGANDO;
  if (e->estado == XE_FALHOU) return XTEPG_FALHOU;
  return e->n > 0 ? XTEPG_PRONTO : XTEPG_VAZIO;
}
void xtepg_priorizar(const char *id) {
  if (!id) return;
  pthread_mutex_lock(&trava);
  for (int i = 0; i < nFila-1; i++) if (!strcmp(fila[i],id)) {
    char ultimo[sizeof fila[0]]; memcpy(ultimo,fila[i],sizeof ultimo);
    unsigned p = filaPedido[i]; unsigned char t = filaTentativas[i]; time_t d=filaDesde[i];
    memmove(fila[i],fila[i+1],sizeof fila[0]*(size_t)(nFila-i-1));
    memmove(filaDesde+i,filaDesde+i+1,sizeof filaDesde[0]*(size_t)(nFila-i-1));
    memmove(filaPedido+i,filaPedido+i+1,sizeof filaPedido[0]*(size_t)(nFila-i-1));
    memmove(filaTentativas+i,filaTentativas+i+1,sizeof filaTentativas[0]*(size_t)(nFila-i-1));
    memcpy(fila[nFila-1],ultimo,sizeof ultimo); filaPedido[nFila-1]=p; filaTentativas[nFila-1]=t;filaDesde[nFila-1]=d;
    break;
  }
  pthread_mutex_unlock(&trava);
}

int xtepg_tem(const char *id) {
  XeEnt *e = id ? achar(id) : NULL;
  return e && e->n > 0 && e->p[e->n-1].fim > time(NULL);
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
  if (!e || !e->n) return 0;
  m = primeiroVivo(e, t);
  if (m < 0 || e->p[m].ini > t) return 0;
  if (p) copiar(&e->p[m], p);
  return 1;
}

int xtepg_proximo(const char *id, time_t t, int k, EpgProg *p) {
  XeEnt *e = id ? achar(id) : NULL;
  int m;
  if (!e || !e->n || k < 0) return 0;
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
  if (!e || !e->n || ate <= de || (out && cap <= 0)) return 0;
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
  relogio = 0;
  pthread_mutex_lock(&trava);
  nFila = 0; nProntos = 0; geracao++;
  pthread_cond_broadcast(&sinal);
  pthread_mutex_unlock(&trava);
}
