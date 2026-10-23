// Saude da rede — ver redesaude.h.
#include "redesaude.h"
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static RedeSaude global;

static int avaliar(RedeSaude *s, unsigned agora) {
  if (!s->offline && s->falhas >= REDE_SAUDE_FALHAS && s->nHosts >= REDE_SAUDE_HOSTS &&
      agora - s->primeira >= REDE_SAUDE_JANELA &&
      !(s->voltouOk && agora - s->voltou < REDE_SAUDE_CARENCIA)) {
    s->offline = 1; s->seq++;
    return 1;
  }
  return 0;
}

static int passoDesde(RedeSaude *s, int ok, unsigned host, unsigned primeira, unsigned agora) {
  int i;
  if (!host) return 0;
  if (!s->temInicio) { s->inicio = agora; s->temInicio = 1; }
  if (ok < 0) {   // timeout de DNS: no arranque, antes de qualquer sucesso, nao diz nada
    if (!s->temOk && agora - s->inicio < REDE_SAUDE_ARRANQUE) return 0;
    ok = 0;
  }
  if (ok) {
    s->falhas = 0; s->nHosts = 0;
    s->ultimoOk = agora; s->temOk = 1;
    if (s->offline) {
      s->offline = 0; s->voltou = agora; s->voltouOk = 1; s->seq++;
      return 1;
    }
    return 0;
  }
  if (s->offline) return 0;
  // Um sucesso de outro fio durante o retry reinicia a janela de falhas.
  if (s->temOk && (int)(primeira - s->ultimoOk) < 0) primeira = s->ultimoOk;
  if (!s->falhas || (int)(primeira - s->primeira) < 0) s->primeira = primeira;
  s->falhas++;
  for (i = 0; i < s->nHosts; i++) if (s->hosts[i] == host) break;
  if (i == s->nHosts && s->nHosts < 4) s->hosts[s->nHosts++] = host;
  return avaliar(s, agora);
}

int rede_saude_passo(RedeSaude *s, int ok, unsigned host, unsigned agora) {
  return passoDesde(s, ok, host, agora, agora);
}

int rede_saude_passo_dns(RedeSaude *s, unsigned host, unsigned agora) {
  return passoDesde(s, -1, host, agora, agora);
}

// Chamado com trava; stdout faz parte do registro enviado.
static void transicao(int mudou, unsigned agora) {
  if (!mudou) return;
  printf("[rede] saude=%s falhas=%d hosts=%d janela=%u ms\n",
         global.offline ? "offline" : "online", global.falhas, global.nHosts,
         global.offline ? agora - global.primeira : 0);
  fflush(stdout);
}

// FNV-1a do host da URL ("https://a.b/c" -> "a.b"); 0 para endereco local.
static unsigned hostHash(const char *url) {
  const char *p = url ? strstr(url, "://") : NULL, *f;
  unsigned h = 2166136261u;
  size_t n;
  if (!p) return 0;
  p += 3;
  f = p + strcspn(p, "/:?#");
  n = (size_t)(f - p);
  if (!n) return 0;
  if ((n >= 4 && !strncmp(p, "127.", 4)) || (n == 9 && !strncmp(p, "localhost", 9)) ||
      (n >= 3 && !strncmp(p, "10.", 3)) || (n >= 8 && !strncmp(p, "192.168.", 8)))
    return 0;
  for (; p < f; p++) { h ^= (unsigned char)*p; h *= 16777619u; }
  return h ? h : 1u;
}

static unsigned agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)((unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull);
}

void rede_saude_nota(int codigo, const char *url) {
  rede_saude_nota_desde(codigo, url, 0);
}

void rede_saude_nota_desde(int codigo, const char *url, unsigned primeiraMs) {
  int ok;
  unsigned h, agora;
  switch (codigo) {
    case 0: ok = 1; break;
    case REDE_CURL_DNS_LENTO: ok = -1; break;
    case 6: case 7: case 28: case 35: case 52: case 55: case 56: ok = 0; break;
    default: return;   // cancelado (42), teto (23), uso errado: nao diz nada da rede
  }
  h = hostHash(url);
  if (!h) return;
  pthread_mutex_lock(&trava);
  agora = agoraMs();
  transicao(passoDesde(&global, ok, h, primeiraMs ? primeiraMs : agora, agora), agora);
  pthread_mutex_unlock(&trava);
}

int rede_saude_offline(void) {
  int o;
  unsigned agora;
  pthread_mutex_lock(&trava);
  agora = agoraMs();
  transicao(avaliar(&global, agora), agora);
  o = global.offline;
  pthread_mutex_unlock(&trava);
  return o;
}

unsigned rede_saude_seq(void) {
  unsigned s, agora;
  pthread_mutex_lock(&trava);
  // ilhasinais_passo consulta por quadro: janela/carencia vencem sem novo pedido.
  agora = agoraMs();
  transicao(avaliar(&global, agora), agora);
  s = global.seq;
  pthread_mutex_unlock(&trava);
  return s;
}

// --- pedidos por host (ver redesaude.h) ----------------------------------------
typedef struct { char host[64]; int pedidos, falhas; unsigned ms[16]; int nMs, kMs; } HostConta;
static HostConta hostsC[REDE_HOSTS_MAX];
static int nHostsC;
static char ultOkHost[64];
static unsigned ultOkMs;
static int temUltOk;

void rede_hosts_nota(const char *url, int codigo, int http, unsigned ms) {
  const char *p = url ? strstr(url, "://") : NULL, *f;
  char h[64];
  size_t n;
  int i;
  if (!p || codigo == 42 || codigo == 23) return;   // cancelado / teto: nao diz nada
  p += 3;
  f = p + strcspn(p, "/:?#");
  n = (size_t)(f - p);
  if (!n || n >= sizeof h || !hostHash(url)) return;
  memcpy(h, p, n); h[n] = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nHostsC; i++) if (!strcmp(hostsC[i].host, h)) break;
  if (i == nHostsC) {
    if (nHostsC < REDE_HOSTS_MAX) nHostsC++;
    else {   // cheio: sai o de menos pedidos
      int m = 0, k;
      for (k = 1; k < nHostsC; k++) if (hostsC[k].pedidos < hostsC[m].pedidos) m = k;
      i = m;
    }
    memset(&hostsC[i], 0, sizeof hostsC[i]);
    memcpy(hostsC[i].host, h, n + 1);
  }
  hostsC[i].pedidos++;
  if (codigo != 0 || http >= 400) hostsC[i].falhas++;
  if (codigo == 0) { memcpy(ultOkHost, h, n + 1); ultOkMs = agoraMs(); temUltOk = 1; }
  if (ms) {
    hostsC[i].ms[hostsC[i].kMs] = ms;
    hostsC[i].kMs = (hostsC[i].kMs + 1) % 16;
    if (hostsC[i].nMs < 16) hostsC[i].nMs++;
  }
  pthread_mutex_unlock(&trava);
}

static int cmpU(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return x < y ? -1 : x > y;
}

int rede_hosts_ler(RedeHost *dst, int max) {
  int i, j, n = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nHostsC && n < max; i++) {
    unsigned v[16];
    RedeHost r;
    memcpy(r.host, hostsC[i].host, sizeof r.host);
    r.pedidos = hostsC[i].pedidos; r.falhas = hostsC[i].falhas; r.tipicoMs = 0;
    if (hostsC[i].nMs) {
      memcpy(v, hostsC[i].ms, sizeof v);
      qsort(v, (size_t)hostsC[i].nMs, sizeof *v, cmpU);
      r.tipicoMs = v[hostsC[i].nMs / 2];
    }
    // insercao por pedidos, decrescente
    for (j = n; j > 0 && dst[j - 1].pedidos < r.pedidos; j--) dst[j] = dst[j - 1];
    dst[j] = r;
    n++;
  }
  pthread_mutex_unlock(&trava);
  return n;
}

int rede_saude_resumo(int *falhas, int *hosts, char *ultHost, size_t tam, unsigned *haMs) {
  int tem;
  pthread_mutex_lock(&trava);
  *falhas = global.falhas; *hosts = global.nHosts;
  tem = temUltOk;
  if (ultHost && tam) snprintf(ultHost, tam, "%s", tem ? ultOkHost : "");
  if (haMs) *haMs = tem ? agoraMs() - ultOkMs : 0;
  pthread_mutex_unlock(&trava);
  return tem;
}
