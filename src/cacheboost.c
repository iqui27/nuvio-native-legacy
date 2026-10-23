// F07: seek cache and volume boost state. See cacheboost.h.
#include "cacheboost.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

// Reports arrive from the Android main thread (JNI); the UI reads on the SDL
// thread. One short mutex, never held across a call out of this file.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static CbCache cache;
static int avisoPendente;          // 1 = a notice-worthy report not yet shown
static int avisoMostrado;          // 1 = this session already showed its notice
static int volume = CB_VOL_NORMAL;
static int ganho = CB_GANHO_DESCONHECIDO;
static int simulado = -1;

int cacheboost_suportado(void) {
  if (simulado >= 0) return simulado;
#ifdef NV_ANDROID
  return 1;
#else
  return 0;
#endif
}
void cacheboost_simular_suporte(int s) { simulado = s < 0 ? -1 : s ? 1 : 0; }

int cacheboost_cache_mb(int opcao) {
  static const int MB[CB_CACHE_OPCOES] = { 0, 256, 512, 1024 };
  return opcao >= 0 && opcao < CB_CACHE_OPCOES ? MB[opcao] : 0;
}

static int limitarMb(int v) { return v < 0 ? 0 : v > 1024 * 64 ? 1024 * 64 : v; }

void cacheboost_cache_relato(int estado, int pedidoMb, int limiteMb, int usadoMb) {
  if (estado < 0 || estado >= CB_CACHE_N) return;
  pthread_mutex_lock(&trava);
  if ((estado == CB_CACHE_POUCO_ESPACO || estado == CB_CACHE_DISCO_CHEIO) &&
      cache.estado != estado && !avisoMostrado)
    avisoPendente = 1;
  cache.estado = estado;
  cache.pedidoMb = limitarMb(pedidoMb);
  cache.limiteMb = estado == CB_CACHE_ATIVO ? limitarMb(limiteMb) : 0;
  cache.usadoMb = estado == CB_CACHE_ATIVO ? limitarMb(usadoMb) : 0;
  if (cache.usadoMb > cache.limiteMb) cache.usadoMb = cache.limiteMb;
  pthread_mutex_unlock(&trava);
}

CbCache cacheboost_cache(void) {
  CbCache c;
  pthread_mutex_lock(&trava);
  c = cache;
  pthread_mutex_unlock(&trava);
  return c;
}

const char *cacheboost_cache_aviso(void) {
  const char *r = NULL;
  pthread_mutex_lock(&trava);
  if (avisoPendente) {
    avisoPendente = 0; avisoMostrado = 1;
    r = cache.estado == CB_CACHE_DISCO_CHEIO ? "Cache de seek desligado: o disco encheu"
                                             : "Cache de seek desligado: pouco espaço livre";
  }
  pthread_mutex_unlock(&trava);
  return r;
}

void cacheboost_cache_texto(char *b, size_t n) {
  CbCache c = cacheboost_cache();
  if (!b || !n) return;
  switch (c.estado) {
    case CB_CACHE_ATIVO:         snprintf(b, n, "%d / %d MB", c.usadoMb, c.limiteMb); break;
    case CB_CACHE_POUCO_ESPACO:  snprintf(b, n, "%s", "Pouco espaço"); break;
    case CB_CACHE_DISCO_CHEIO:   snprintf(b, n, "%s", "Disco cheio"); break;
    case CB_CACHE_NAO_SE_APLICA: snprintf(b, n, "%s", "Não se aplica"); break;
    case CB_CACHE_FALHOU:        snprintf(b, n, "%s", "Falhou"); break;
    default:                     snprintf(b, n, "%s", "Desligado"); break;
  }
}

void cacheboost_sessao(void) {
  pthread_mutex_lock(&trava);
  memset(&cache, 0, sizeof cache);
  avisoPendente = avisoMostrado = 0;
  volume = CB_VOL_NORMAL;
  ganho = CB_GANHO_DESCONHECIDO;
  pthread_mutex_unlock(&trava);
}

int cacheboost_volume(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = volume;
  pthread_mutex_unlock(&trava);
  return v;
}

static int tetoLocked(void) { return ganho == CB_GANHO_PASSTHROUGH ? CB_VOL_NORMAL : CB_VOL_MAX; }

int cacheboost_volume_teto(void) {
  int t;
  pthread_mutex_lock(&trava);
  t = tetoLocked();
  pthread_mutex_unlock(&trava);
  return t;
}

int cacheboost_volume_passo(int dir) {
  int v, t;
  pthread_mutex_lock(&trava);
  t = tetoLocked();
  v = volume + (dir > 0 ? CB_VOL_PASSO : dir < 0 ? -CB_VOL_PASSO : 0);
  // Snap to the step grid: a value clamped by passthrough stays on it anyway.
  v = (v / CB_VOL_PASSO) * CB_VOL_PASSO;
  if (v < CB_VOL_MIN) v = CB_VOL_MIN;
  if (v > t) v = t;
  volume = v;
  pthread_mutex_unlock(&trava);
  return v;
}

int cacheboost_volume_acima(void) { return cacheboost_volume() > CB_VOL_NORMAL; }

void cacheboost_ganho_relato(int estado) {
  if (estado < CB_GANHO_DESCONHECIDO || estado > CB_GANHO_PASSTHROUGH) return;
  pthread_mutex_lock(&trava);
  ganho = estado;
  if (estado == CB_GANHO_PASSTHROUGH && volume > CB_VOL_NORMAL) volume = CB_VOL_NORMAL;
  pthread_mutex_unlock(&trava);
}

int cacheboost_ganho_estado(void) {
  int g;
  pthread_mutex_lock(&trava);
  g = ganho;
  pthread_mutex_unlock(&trava);
  return g;
}

double cacheboost_volume_db(int pct) {
  if (pct <= 0) return -120.0;
  return 20.0 * log10((double)pct / 100.0);
}

int cacheboost_buffer_mb(int opcao, long livreMb) {
  int pedido = opcao >= 1 && opcao <= 3 ? 32 << opcao : 0, mb = pedido;
  // DTS keeps one active 4 MiB range in addition to the queued buffer.
  while (mb && (livreMb < 0 || livreMb - mb - 4 < CB_BUFFER_PISO_MB)) mb = mb > 64 ? mb / 2 : 0;
  if (mb != pedido)
    printf("[buffer] %d MiB pedido, %d MiB usado (livre %ld MiB, piso %d)\n", pedido, mb, livreMb, CB_BUFFER_PISO_MB);
  return mb;
}

#ifndef NV_ANDROID
void cacheboost_backend_ganho(int pct) { (void)pct; }
void cacheboost_backend_cache(int mb) { (void)mb; }
void cacheboost_backend_titulo(const char *id) { (void)id; }
void cacheboost_backend_descartar(void) { }
static volatile int bufferMb;
void cacheboost_backend_buffer(int mb) { bufferMb = mb; }
int cacheboost_backend_buffer_mb(void) { return bufferMb; }
#endif
