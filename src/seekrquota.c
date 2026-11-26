// The Samsung .tpk, LG and Android make their thumbnails on the device
// (miniatura.c serves the same seekr_* calls there, miniplat.h); the Seekr API
// is not built into them.
#include "miniplat.h"
#ifndef NV_MINIATURAS
#include "seekrquota.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifndef __EMSCRIPTEN__
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <emscripten.h>
#endif

#define QUOTA_ARQUIVO "seekrquota-v1.txt"
#define QUOTA_RELOGIO_MIN 1577836800LL // 2020-01-01; an unset TV clock is unsafe.
typedef struct { long long dia, ultimoUtc; int usadas; } Registro;
static pthread_mutex_t quotaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t leituraTrava = PTHREAD_MUTEX_INITIALIZER;
static Registro registro = {-1, 0, 0};
static Registro publicado = {-1, 0, 0};
static int publicadoOk = 1;
static int carregado, armazenamentoOk = 1;

static void publicar(void) {
  pthread_mutex_lock(&leituraTrava);
  publicado = registro; publicadoOk = armazenamentoOk;
  pthread_mutex_unlock(&leituraTrava);
}

// IDBFS rename alone is not durable: its asynchronous sync may happen after
// the request or never happen after a crash. This tiny, non-secret ledger uses
// synchronous localStorage on the main runtime before dispatch, as the app's
// exit marker already does. All workers serialize through quotaTrava.
static int lerRegistro(Registro *out) {
  char *texto = NULL;
#ifdef __EMSCRIPTEN__
  char buf[160];
  int n = MAIN_THREAD_EM_ASM_INT({
    try {
      var value = localStorage.getItem("nv-seekrquota-v1");
      if (value === null) return 0;
      if (lengthBytesUTF8(value) >= $1) return -1;
      stringToUTF8(value, $0, $1);
      return 1;
    } catch (e) { return -1; }
  }, buf, sizeof buf);
  if (n <= 0) return n;
  texto = strdup(buf);
#else
  char caminho[600];
  struct stat st;
  if (!dados_caminho(caminho, sizeof caminho, QUOTA_ARQUIVO)) return -1;
  if (stat(caminho, &st) != 0) return errno == ENOENT ? 0 : -1;
  if (st.st_size <= 0 || st.st_size > 150) return -1;
  texto = dados_ler(QUOTA_ARQUIVO);
#endif
  if (!texto) return -1;
  int versao, usadas, fim = 0;
  long long dia, ultimo;
  int ok = sscanf(texto, "%d %lld %lld %d %n", &versao, &dia, &ultimo, &usadas, &fim) == 4 &&
           versao == 1 && texto[fim] == 0 && usadas >= 0 && usadas <= SEEKR_QUOTA_DIA &&
           ultimo >= QUOTA_RELOGIO_MIN && dia == ultimo / 86400;
  free(texto);
  if (!ok) return -1; // Corruption never grants a fresh budget.
  *out = (Registro){dia, ultimo, usadas};
  return 1;
}

static int gravarRegistro(const Registro *r) {
  char texto[160];
  snprintf(texto, sizeof texto, "1 %lld %lld %d\n", r->dia, r->ultimoUtc, r->usadas);
#ifdef __EMSCRIPTEN__
  return MAIN_THREAD_EM_ASM_INT({
    try {
      var value = UTF8ToString($0);
      localStorage.setItem("nv-seekrquota-v1", value);
      return localStorage.getItem("nv-seekrquota-v1") === value ? 1 : 0;
    } catch (e) { return 0; }
  }, texto);
#else
  char caminho[600];
  if (!dados_gravar(QUOTA_ARQUIVO, texto) ||
      !dados_caminho(caminho, sizeof caminho, QUOTA_ARQUIVO)) return 0;
  int arquivo = open(caminho, O_RDONLY), diretorio;
  if (arquivo < 0) return 0;
  int ok = fsync(arquivo) == 0;
  close(arquivo);
  diretorio = open(dados_dir(), O_RDONLY);
  if (diretorio < 0) return 0;
  if (fsync(diretorio) != 0) ok = 0; // Persist the atomic replacement too.
  close(diretorio);
  return ok;
#endif
}

static void carregar(void) {
  if (carregado) return;
  carregado = 1;
  armazenamentoOk = lerRegistro(&registro) >= 0;
  publicar();
}

void seekrquota_uso(long long utc, SeekrQuotaUso *uso) {
  if (!uso) return;
  // A worker may be waiting for synchronous main-thread localStorage. The UI
  // must never wait on that worker's mutex. The last published snapshot also
  // avoids making a native UI frame wait on fsync.
  Registro atual;
  int ok;
  if (pthread_mutex_trylock(&quotaTrava) != 0) {
    pthread_mutex_lock(&leituraTrava);
    atual = publicado; ok = publicadoOk;
    pthread_mutex_unlock(&leituraTrava);
  } else {
    carregar();
    atual = registro; ok = armazenamentoOk;
    pthread_mutex_unlock(&quotaTrava);
  }
  long long dia = atual.dia;
  int usadas = atual.usadas;
  int atrasado = utc < QUOTA_RELOGIO_MIN || (atual.ultimoUtc && utc < atual.ultimoUtc);
  if (!atrasado && utc / 86400 > dia) { dia = utc / 86400; usadas = 0; }
  if (!ok) usadas = SEEKR_QUOTA_DIA;
  *uso = (SeekrQuotaUso){usadas, SEEKR_QUOTA_DIA - usadas, SEEKR_QUOTA_DIA,
                        atrasado, ok, dia < 0 ? 0 : (dia + 1) * 86400};
}

int seekrquota_reservar(long long utc) {
  pthread_mutex_lock(&quotaTrava);
  carregar();
#ifndef __EMSCRIPTEN__
  // Also serialize two native app processes sharing this installation.
  char caminho[600];
  int lock = dados_caminho(caminho, sizeof caminho, "seekrquota-v1.lock") ?
    open(caminho, O_RDWR | O_CREAT, 0600) : -1;
  if (lock < 0 || flock(lock, LOCK_EX) != 0) {
    if (lock >= 0) close(lock);
    armazenamentoOk = 0;
    publicar();
    pthread_mutex_unlock(&quotaTrava);
    return SEEKR_QUOTA_ARMAZENAMENTO;
  }
#endif
  Registro atual = registro;
  int lido = lerRegistro(&atual), resultado;
  if (lido < 0) { armazenamentoOk = 0; resultado = SEEKR_QUOTA_ARMAZENAMENTO; }
  else if (utc < QUOTA_RELOGIO_MIN && atual.dia < 0) resultado = SEEKR_QUOTA_RELOGIO;
  else {
    // Keep the high-water window when the clock goes backwards. A rollback
    // can consume its remaining calls, but can never reset the counter.
    if (utc >= QUOTA_RELOGIO_MIN && utc / 86400 > atual.dia)
      atual = (Registro){utc / 86400, utc, 0};
    if (atual.usadas >= SEEKR_QUOTA_DIA) resultado = SEEKR_QUOTA_LIMITE;
    else {
      atual.usadas++;
      if (utc > atual.ultimoUtc) atual.ultimoUtc = utc;
      // If persistence fails after rename, conservatively retain the charged
      // call in memory; no HTTP request may follow an unconfirmed reservation.
      registro = atual;
      armazenamentoOk = gravarRegistro(&atual);
      resultado = armazenamentoOk ? SEEKR_QUOTA_OK : SEEKR_QUOTA_ARMAZENAMENTO;
    }
    if (resultado == SEEKR_QUOTA_LIMITE) registro = atual;
  }
#ifndef __EMSCRIPTEN__
  flock(lock, LOCK_UN); close(lock);
#endif
  publicar();
  pthread_mutex_unlock(&quotaTrava);
  return resultado;
}
#else
typedef int seekrquota_fora_do_tpk;   // ISO C: a translation unit must not be empty
#endif
