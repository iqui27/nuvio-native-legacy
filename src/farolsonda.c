#include "farolsonda.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#ifndef FAROL_SONDA_WORKER
#ifdef NV_REC_URL
#define FAROL_SONDA_WORKER NV_REC_URL
#else
#define FAROL_SONDA_WORKER ""
#endif
#endif

extern int player_aberto(void);
extern int player_mini_ativo(void);

typedef struct {
  char *url;
  char *cab[34];
  long long fim;
} Pedido;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int rodando, cancelada;
static FarolSonda resultado = {-1, -1, 0, -1, -1, 0, 0};

static long long agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static int parar(void *u) {
  Pedido *p = u;
  pthread_mutex_lock(&trava);
  int sim = cancelada;
  pthread_mutex_unlock(&trava);
  return sim || agora() >= p->fim;
}

static void limpar(Pedido *p) {
  free(p->url);
  for (int i = 0; p->cab[i]; i++) free(p->cab[i]);
  free(p);
}

static void medir(Pedido *p, const char *url, int fonte, FarolSonda *s) {
  long long restante = p->fim - agora();
  if (!*url || restante <= 0 || parar(p)) return;
  RedePedido req = {0};
  RedeResposta res = {0};
  req.url = url;
  req.cabecalhos = fonte ? (const char *const *)p->cab : NULL;
  req.prazo_ms = restante < 2000 ? (unsigned)restante : 2000;
  req.max_bytes = fonte ? 2 : 512;
  req.max_cabecalhos = 8192;
  req.seguir = 1;
  req.sonda = 1;
  req.parar = parar;
  req.parar_usuario = p;
  rede_pedir(&req, &res);
  if (fonte) {
    s->http = res.status;
    if (res.status) {
      s->range = res.status == 206;
      s->ttfb_ms = (int)res.primeiro_byte_ms;
    }
  } else s->internet_http = res.status;
  rede_resposta_limpar(&res);
}

static void *executar(void *u) {
  Pedido *p = u;
  FarolSonda s = {-1, -1, 0, -1, -1, 0, 0};
  /* A raiz do Worker responde 401 sem autenticacao; a rota de saude e a que
   * prova alcance real do servico. */
  char saude[sizeof FAROL_SONDA_WORKER + sizeof "/v1/saude"];
  snprintf(saude, sizeof saude, "%s/v1/saude", FAROL_SONDA_WORKER);
  medir(p, FAROL_SONDA_WORKER[0] ? saude : FAROL_SONDA_WORKER, 0, &s);
  medir(p, p->url, 1, &s);
  limpar(p);
  pthread_mutex_lock(&trava);
  s.cancelada = cancelada;
  resultado = s;
  rodando = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

int farolsonda_iniciar(const char *url, const char *const *cab) {
  if (!url || !*url || strlen(url) > 16384 || player_aberto() ||
      player_mini_ativo()) return 0;
  pthread_mutex_lock(&trava);
  if (rodando) { pthread_mutex_unlock(&trava); return 0; }
  Pedido *p = calloc(1, sizeof *p);
  if (!p) { pthread_mutex_unlock(&trava); return 0; }
  p->url = strdup(url);
  int n = 0, ok = p->url != NULL;
  for (int i = 0; ok && cab && cab[i]; i++) {
    if (i >= 32 || strlen(cab[i]) > 8192) { ok = 0; break; }
    if (!strncasecmp(cab[i], "Range:", 6)) continue;
    p->cab[n] = strdup(cab[i]);
    if (!p->cab[n]) { ok = 0; break; }
    n++;
  }
  if (ok) { p->cab[n] = strdup("Range: bytes=0-1"); ok = p->cab[n] != NULL; }
  pthread_attr_t attr;
  int attr_ok = pthread_attr_init(&attr) == 0;
  if (!ok || !attr_ok) {
    if (attr_ok) pthread_attr_destroy(&attr);
    limpar(p); pthread_mutex_unlock(&trava); return 0;
  }
  p->fim = agora() + 5000;
  cancelada = 0;
  rodando = 1;
  resultado = (FarolSonda){-1, -1, 0, -1, -1, 0, 0};
  pthread_t fio;
  int erro = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (!erro) erro = pthread_create(&fio, &attr, executar, p);
  pthread_attr_destroy(&attr);
  if (erro) { rodando = 0; limpar(p); }
  pthread_mutex_unlock(&trava);
  return !erro;
}

int farolsonda_estado(FarolSonda *out) {
  if (player_aberto() || player_mini_ativo()) farolsonda_cancelar();
  pthread_mutex_lock(&trava);
  if (out) *out = resultado;
  int pronta = !rodando;
  pthread_mutex_unlock(&trava);
  return pronta;
}

void farolsonda_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (rodando) cancelada = 1;
  pthread_mutex_unlock(&trava);
}
