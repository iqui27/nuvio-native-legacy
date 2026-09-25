#include "cwordem.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int cwo_futuro(const CwoItem *it, long long agoraMs) {
  return it && it->aSeguir && it->estreiaMs != CWO_SEM_DATA && it->estreiaMs > agoraMs;
}

int cwo_ordenar(const CwoItem *v, int n, int modo, long long agoraMs, int *perm) {
  int i, k, w = 0, principal;
  if (!v || !perm || n <= 0) return 0;
  for (i = 0; i < n; i++) perm[i] = i;
  if (modo != CWO_STREAMING && modo != CWO_SEPARAR) return n;
  // Particao estavel: exibidos na ordem que chegaram (a do instante), futuros
  // depois. n <= 36 (tres fontes de 12): um vetor na pilha basta.
  { int fut[64], nf = 0;
    if (n > 64) n = 64;
    for (i = 0; i < n; i++) {
      if (cwo_futuro(&v[i], agoraMs)) fut[nf++] = i;
      else perm[w++] = i;
    }
    principal = w;
    // Insercao estavel pela estreia, a mais proxima primeiro.
    for (i = 1; i < nf; i++) {
      int t = fut[i];
      for (k = i - 1; k >= 0 && v[fut[k]].estreiaMs > v[t].estreiaMs; k--) fut[k + 1] = fut[k];
      fut[k + 1] = t;
    }
    for (i = 0; i < nf; i++) perm[w++] = fut[i]; }
  return principal;
}

void cwo_corte(int principal, int nFut, int max, int *nPrincipal, int *nFuturos) {
  int reserva, mp, mf;
  if (principal < 0) principal = 0;
  if (nFut < 0) nFut = 0;
  if (max < 0) max = 0;
  reserva = max / 3 > 0 ? max / 3 : 1;
  if (reserva > nFut) reserva = nFut;
  if (reserva > max) reserva = max;
  mp = principal < max - reserva ? principal : max - reserva;
  mf = nFut < max - mp ? nFut : max - mp;
  if (nPrincipal) *nPrincipal = mp;
  if (nFuturos) *nFuturos = mf;
}

// --- Datas de estreia --------------------------------------------------------
// 96: a fileira tem ate 12 itens por fonte e o Trakt guarda ate 64 "a seguir"
// (TK_ULT_MAX). Cheia, a mais velha e sobrescrita em roda — o que importa e a
// rodada atual.
#define CWO_EST_MAX 96
static struct { char id[40]; long long ms; } est[CWO_EST_MAX];
static int nEst, proxEst;
static pthread_mutex_t estTrava = PTHREAD_MUTEX_INITIALIZER;

void cwo_marcar_estreia(const char *id, long long ms) {
  int i;
  if (!id || !id[0]) return;
  pthread_mutex_lock(&estTrava);
  for (i = 0; i < nEst; i++)
    if (!strcmp(est[i].id, id)) { est[i].ms = ms; pthread_mutex_unlock(&estTrava); return; }
  i = nEst < CWO_EST_MAX ? nEst++ : proxEst;
  proxEst = (i + 1) % CWO_EST_MAX;
  snprintf(est[i].id, sizeof est[i].id, "%s", id);
  est[i].ms = ms;
  pthread_mutex_unlock(&estTrava);
}

long long cwo_estreia(const char *id) {
  long long ms = CWO_SEM_DATA;
  int i;
  if (!id || !id[0]) return ms;
  pthread_mutex_lock(&estTrava);
  for (i = 0; i < nEst; i++)
    if (!strcmp(est[i].id, id)) { ms = est[i].ms; break; }
  pthread_mutex_unlock(&estTrava);
  return ms;
}

// --- Fileira de futuros ------------------------------------------------------
#define CWO_FUT_MAX 36
static char fut[CWO_FUT_MAX][64];
static int nFut;
static pthread_mutex_t futTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned futRev;

void cwo_publicar_futuros(const char *const *ids, int n) {
  static char novo[CWO_FUT_MAX][64];
  int i, nNovo = 0;
  for (i = 0; ids && i < n && nNovo < CWO_FUT_MAX; i++)
    if (ids[i] && ids[i][0]) snprintf(novo[nNovo++], sizeof novo[0], "%s", ids[i]);
  pthread_mutex_lock(&futTrava);
  if (nNovo != nFut || memcmp(novo, fut, sizeof fut[0] * (size_t)nNovo)) {
    memcpy(fut, novo, sizeof fut[0] * (size_t)nNovo);
    nFut = nNovo;
    futRev++;
  }
  pthread_mutex_unlock(&futTrava);
}

unsigned cwo_revisao(void) {
  unsigned r;
  pthread_mutex_lock(&futTrava);
  r = futRev;
  pthread_mutex_unlock(&futTrava);
  return r;
}

int cwo_e_futuro(const char *id) {
  int i, sim = 0;
  if (!id || !id[0]) return 0;
  pthread_mutex_lock(&futTrava);
  for (i = 0; i < nFut && !sim; i++) sim = !strcmp(fut[i], id);
  pthread_mutex_unlock(&futTrava);
  return sim;
}

// --- Rotulo da estreia -------------------------------------------------------
int cwo_data_curta(long long estreiaMs, long long agoraMs, int ingles, int maiusc,
                   char *dst, size_t cap) {
  static const char *EN[] = { "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec" };
  static const char *PT[] = { "jan","fev","mar","abr","mai","jun","jul","ago","set","out","nov","dez" };
  struct tm e, a;
  time_t te, ta;
  char mes[4];
  size_t i;
  if (!dst || cap < 1) return 0;
  dst[0] = 0;
  if (estreiaMs == CWO_SEM_DATA) return 0;
  // Pelo calendario UTC, como o `released` do Cinemeta (ver agenda.h, ponto
  // 3): o fuso de uma TV nem sempre esta certo, e com ele a estreia andaria
  // um dia de aparelho para aparelho.
  te = (time_t)(estreiaMs / 1000LL);
  ta = (time_t)(agoraMs / 1000LL);
  if (!gmtime_r(&te, &e) || !gmtime_r(&ta, &a)) return 0;
  if (e.tm_mon < 0 || e.tm_mon > 11) return 0;
  snprintf(mes, sizeof mes, "%s", ingles ? EN[e.tm_mon] : PT[e.tm_mon]);
  if (maiusc)
    for (i = 0; mes[i]; i++) if (mes[i] >= 'a' && mes[i] <= 'z') mes[i] = (char)(mes[i] - 32);
  // O ano so quando nao e o corrente, como as datas curtas de noticias.c.
  if (e.tm_year != a.tm_year) {
    if (ingles) snprintf(dst, cap, "%s %d, %d", mes, e.tm_mday, e.tm_year + 1900);
    else        snprintf(dst, cap, "%d %s %d", e.tm_mday, mes, e.tm_year + 1900);
  } else {
    if (ingles) snprintf(dst, cap, "%s %d", mes, e.tm_mday);
    else        snprintf(dst, cap, "%d %s", e.tm_mday, mes);
  }
  return 1;
}
