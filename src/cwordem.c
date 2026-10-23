#include "cwordem.h"
#include "idiomacod.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int cwo_futuro(const CwoItem *it, long long agoraMs) {
  return it && it->aSeguir && it->estreiaMs != CWO_SEM_DATA && it->estreiaMs > agoraMs;
}

int cwo_ordenar(const CwoItem *v, int n, int modo, long long agoraMs, int *perm) {
  int i, w = 0, principal;
  if (!v || !perm || n <= 0) return 0;
  // Default preserves last-watched order across paused and next-up items.
  if (modo == CWO_PADRAO) {
    for (i = 0; i < n; i++) perm[i] = i;
    return n;
  }
  for (i = 0; i < n; i++) if (!v[i].aSeguir) perm[w++] = i;
  for (i = 0; i < n; i++)
    if (v[i].aSeguir && (modo == CWO_PADRAO || !cwo_futuro(&v[i], agoraMs))) perm[w++] = i;
  principal = w;
  if (modo != CWO_PADRAO)
    for (i = 0; i < n; i++) if (cwo_futuro(&v[i], agoraMs)) perm[w++] = i;
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
// 200: a fileira tem ate 40 itens por fonte (3 fontes = 120) e o Trakt guarda
// ate 64 "a seguir" (TK_ULT_MAX). Cheia, a mais velha e sobrescrita em roda.
#define CWO_EST_MAX 200
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
// Teto da fileira (CONT_MAX / CW_TETO). cwo_corte com 0 pausados devolve ate
// max futuros; a tabela tem de caber todos, senão os ultimos ficam em
// "Continuar assistindo" e nunca viram "Proximos episodios".
#define CWO_FUT_MAX CWO_FILEIRA_MAX
_Static_assert(CWO_FUT_MAX >= 40 && CWO_FUT_MAX == CWO_FILEIRA_MAX,
               "futuros publicados cabem na fileira");
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
int cwo_data_curta(long long estreiaMs, long long agoraMs, int idioma, int maiusc,
                   char *dst, size_t cap) {
  struct tm e, a;
  time_t te, ta;
  // 16: o mes em cirilico ocupa 2 bytes por letra, e a caixa alta mantem o tamanho.
  char mes[16];
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
  snprintf(mes, sizeof mes, "%s", idioma_mes_curto(idioma, e.tm_mon));
  if (maiusc) { char cx[16]; idioma_maiusc_em(idioma, cx, sizeof cx, mes); snprintf(mes, sizeof mes, "%s", cx); }
  // O ano so quando nao e o corrente, como as datas curtas de noticias.c. A
  // ORDEM (dia mes, mes dia, ano primeiro) e do idioma: ver idioma_data_curta.
  idioma_data_curta(idioma, e.tm_mday, mes,
                    e.tm_year != a.tm_year ? e.tm_year + 1900 : 0, dst, cap);
  return 1;
}

// --- "A seguir" da conta -----------------------------------------------------
// PROX_MAX_BUSCAS (24) e o teto de sementes por rodada; 32 da folga.
#define CWO_CONTA_MAX 32
static char conta[CWO_CONTA_MAX][40];
static int nConta;
static pthread_mutex_t contaTrava = PTHREAD_MUTEX_INITIALIZER;

void cwo_conta_definir(const char *const *ids, int n) {
  int i;
  pthread_mutex_lock(&contaTrava);
  nConta = 0;
  for (i = 0; ids && i < n && nConta < CWO_CONTA_MAX; i++)
    if (ids[i] && ids[i][0]) snprintf(conta[nConta++], sizeof conta[0], "%s", ids[i]);
  pthread_mutex_unlock(&contaTrava);
}

// Os "a seguir" que a DESMARCACAO moveu (descoberta.c, ajustarASeguir): o id
// novo ("tt:2:7") nao esta na lista do Trakt/Simkl/conta, que falam do episodio
// do remoto. Entram aqui e contam como "a seguir" para todo mundo que pergunta.
static char ajust[CWO_CONTA_MAX][40];
static int nAjust;

void cwo_ajustados_definir(const char *const *ids, int n) {
  int i;
  pthread_mutex_lock(&contaTrava);
  nAjust = 0;
  for (i = 0; ids && i < n && nAjust < CWO_CONTA_MAX; i++)
    if (ids[i] && ids[i][0]) snprintf(ajust[nAjust++], sizeof ajust[0], "%s", ids[i]);
  pthread_mutex_unlock(&contaTrava);
}

int cwo_conta_a_seguir(const char *id) {
  int i, sim = 0;
  if (!id || !id[0]) return 0;
  pthread_mutex_lock(&contaTrava);
  for (i = 0; i < nConta && !sim; i++) sim = !strcmp(conta[i], id);
  for (i = 0; i < nAjust && !sim; i++) sim = !strcmp(ajust[i], id);
  pthread_mutex_unlock(&contaTrava);
  return sim;
}

void cwo_conta_trocar(const char *velho, const char *novo) {
  int i;
  if (!velho || !novo || !novo[0]) return;
  pthread_mutex_lock(&contaTrava);
  for (i = 0; i < nConta; i++)
    if (!strcmp(conta[i], velho)) { snprintf(conta[i], sizeof conta[0], "%s", novo); break; }
  pthread_mutex_unlock(&contaTrava);
}

int cwo_virada_aceita(long long estreiaMs, long long agoraMs) {
  const long long DIA = 24LL * 60LL * 60LL * 1000LL;
  long long hoje;
  if (estreiaMs == CWO_SEM_DATA) return 0;
  if (estreiaMs <= agoraMs) return 1;
  // Dia da estreia menos o dia de hoje, os dois no calendario UTC.
  hoje = agoraMs - (agoraMs % DIA);
  return (estreiaMs - hoje) / DIA <= 7;
}
