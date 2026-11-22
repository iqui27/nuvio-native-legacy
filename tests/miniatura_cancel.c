// The CANCEL PATH OF THE REAL MOTOR (src/miniatura_motor.c) under the thread
// sanitizer.
//
// The worker thread runs mini_motor_quadro() — which polls *cancelar between
// packets and keyframe probes — while the main thread flips the flag with the
// same release-store the provider uses (prov_desligar). A plain read next to
// that store is a TSan data race (the review measured rc 66); the atomic load
// keeps rc 0.
//
// The reader callback runs on BOTH the worker thread (the demuxer's I/O) and
// the motor's own prefetch thread — so it pread()s: no shared FILE state of
// its own. Usage: miniatura_cancel <arquivo.ts>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "miniatura_motor.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int falhas;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FALHOU: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

static volatile int cancelar;          // the same volatile the provider hands the motor
static volatile int fioPronto;
static volatile int chamadas;          // quadro calls started by the worker
static int arquivo = -1;
static MiniMotor *motor;
static volatile int quadrosFeitos, quadrosParados;

static long lerArquivo(void *usuario, int64_t ini, int64_t fim, unsigned char *dst) {
  int fd = *(int *)usuario;
  int64_t quer = fim - ini + 1, lido = 0;
  while (lido < quer) {
    ssize_t n = pread(fd, dst + lido, (size_t)(quer - lido), ini + lido);
    if (n <= 0) break;
    lido += n;
  }
  return (long)lido;
}

static long lerPagina(void *usuario, int64_t ini, int64_t fim, unsigned char *dst) {
  const char *p = usuario;
  int64_t n = fim - ini + 1;
  memcpy(dst, p + ini, (size_t)n);
  return (long)n;
}

static void *fio(void *u) {
  (void)u;
  char erro[128];
  MiniQuadro q;
  __atomic_store_n(&fioPronto, 1, __ATOMIC_RELEASE);
  // Cancelled calls return in microseconds; under TSan the whole grid would
  // burn inside the flip window. Retry until a few tiles decode for real.
  for (int tent = 0; quadrosFeitos < 6 && tent < 400; tent++) {
    __atomic_fetch_add(&chamadas, 1, __ATOMIC_ACQ_REL);
    long alvo = (tent % 25) * 1600;
    unsigned char *jpg = NULL;
    int nj = 0;
    int r = mini_motor_quadro(motor, alvo, 3000, 1, &cancelar, &jpg, &nj, &q, erro, sizeof erro);
    free(jpg);
    if (r < 0 && strstr(erro, "cancelled")) __atomic_fetch_add(&quadrosParados, 1, __ATOMIC_ACQ_REL);
    else if (!r) __atomic_fetch_add(&quadrosFeitos, 1, __ATOMIC_ACQ_REL);
    usleep(5000);                                         // the worker sleeps between tiles
  }
  return NULL;
}

int main(int argc, char **argv) {
  if (argc != 2) { fprintf(stderr, "uso: %s <arquivo.ts>\n", argv[0]); return 2; }
  arquivo = open(argv[1], O_RDONLY);
  if (arquivo < 0) { perror("open"); return 2; }
  off_t tam = lseek(arquivo, 0, SEEK_END);
  CHECK(tam > 65536 * 2, "fonte muito pequena (%lld)", (long long)tam);

  char erro[128];
  motor = mini_motor_abrir(lerArquivo, &arquivo, tam, erro, sizeof erro);
  CHECK(motor != NULL, "mini_motor_abrir: %s", erro);
  if (!motor) return 1;
  CHECK(!mini_motor_decoder_aberto(motor), "decoder nasce fechado");

  pthread_t t;
  if (pthread_create(&t, NULL, fio, NULL)) return 2;
  while (!__atomic_load_n(&fioPronto, __ATOMIC_ACQUIRE)) sched_yield();
  // The provider's write: release-store on the same flag the motor reads.
  // Short pulses — the way prov_desligar cancels a running worker and the
  // next job clears it — each one timed on a call already in flight, so the
  // read/write pair exists even when the startup is slow under sanitizers.
  int vistos = 0;
  for (int i = 0; i < 12; i++) {
    for (int k = 0; k < 2000 && __atomic_load_n(&chamadas, __ATOMIC_ACQUIRE) <= vistos &&
         __atomic_load_n(&quadrosFeitos, __ATOMIC_ACQUIRE) < 6; k++)
      usleep(1000);                                       // wait for a call in flight
    vistos = __atomic_load_n(&chamadas, __ATOMIC_ACQUIRE);
    if (__atomic_load_n(&quadrosFeitos, __ATOMIC_ACQUIRE) >= 6) break;
    __atomic_store_n(&cancelar, 1, __ATOMIC_RELEASE);
    usleep(2000);                                         // the call notices and bails
    __atomic_store_n(&cancelar, 0, __ATOMIC_RELEASE);
    usleep(4000);                                         // let the next call start clean
  }
  pthread_join(t, NULL);

  CHECK(quadrosFeitos > 0, "o motor decodificou quadros de verdade (feitos=%d)", quadrosFeitos);
  CHECK(quadrosParados > 0, "o cancelamento foi visto e abortou chamadas (parados=%d)", quadrosParados);
  CHECK(mini_motor_decoder_aberto(motor), "decoder aberto depois de decodificar");

  mini_motor_soltar_decoder(motor);
  CHECK(!mini_motor_decoder_aberto(motor), "soltar_decoder fecha o decoder");

  // Cancelled up front: the call returns "cancelled" without decoding.
  cancelar = 1;
  unsigned char *jpg = NULL;
  int nj = 0;
  MiniQuadro q;
  CHECK(mini_motor_quadro(motor, 5000, 3000, 1, &cancelar, &jpg, &nj, &q, erro, sizeof erro) < 0 &&
        strstr(erro, "cancelled"), "cancelado antes de comecar: %s", erro);
  free(jpg);

  mini_motor_fechar(motor);
  close(arquivo);
  arquivo = -1;

  // A refused source (an error page where the URL and a token live in the
  // body): the error text carries a short reason, never the bytes.
  {
    static const char pagina[] =
      "HTTP/1.1 403 Forbidden\r\n\r\n"
      "<html>https://test.io/only-synthetic-token Authorization: Bearer SYNTHETIC_HEADER_MARKER</html>";
    erro[0] = 0;
    MiniMotor *r = mini_motor_abrir(lerPagina, (void *)pagina, (int64_t)sizeof pagina - 1, erro, sizeof erro);
    CHECK(r == NULL, "pagina de erro nao vira motor");
    if (r) mini_motor_fechar(r);
    CHECK(strstr(erro, "container"), "o motivo curto fica (%s)", erro);
    CHECK(!strstr(erro, "first bytes") && !strstr(erro, "synthetic") && !strstr(erro, "SYNTHETIC") && !strstr(erro, "48 54"),
          "o erro nao vaza bytes do corpo nem a URL (%s)", erro);
  }
  if (falhas) printf("FAIL: %d assercoes\n", falhas);
  else printf("PASS: cancelar atomico no motor real (%d quadros, %d canceladas)\n", quadrosFeitos, quadrosParados);
  return falhas ? 1 : 0;
}
