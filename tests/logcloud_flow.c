// The real upload thread and redactor, with the HTTP boundary replaced by a
// local assertion. No sockets, accounts or real uploads.
#define _POSIX_C_SOURCE 200809L
#include "logcloud.h"
#include "rede.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
static char source[1024];
static atomic_int calls;
static int response_status = 200;
const char *registro_arquivo(void) { return source; }
int rede_pedir(const RedePedido *q, RedeResposta *r) {
  assert(!strcmp(q->metodo, "POST") && !strcmp(q->url, "https://fixture.invalid/v1/loglink"));
  assert(!strcmp(q->cabecalhos[0], "x-nuvio-auth: loglink"));
  assert(!strcmp(q->cabecalhos[1], "Authorization: Bearer synthetic-upload-only"));
  assert(!strcmp(q->cabecalhos[2], "Content-Type: application/gzip"));
  assert(!q->seguir && q->max_bytes == 4096 && q->prazo_ms == 180000);
  char text[8192]; z_stream z = {0};
  assert(inflateInit2(&z, 31) == Z_OK);
  z.next_in = (Bytef *)q->corpo; z.avail_in = (uInt)q->n_corpo;
  z.next_out = (Bytef *)text; z.avail_out = sizeof text - 1;
  assert(inflate(&z, Z_FINISH) == Z_STREAM_END);
  text[z.total_out] = 0; inflateEnd(&z);
  assert(strstr(text, "FLOW_MARKER") && strstr(text, "REDACTED"));
  assert(!strstr(text, "PRIVATE_FIXTURE") && !strstr(text, "synthetic-upload-only"));
  r->status = response_status;
  r->corpo = strdup("{\"url\":\"https://fixture.invalid/v1/loglink/0123456789abcdef0123456789abcdef\"}");
  atomic_fetch_add(&calls, 1); return 1;
}
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); }
static void wait_done(void) {
  const struct timespec tick = {0, 10000000};
  for (int i = 0; i < 200 && logcloud_estado() == LOGCLOUD_ENVIANDO; i++) nanosleep(&tick, NULL);
  assert(logcloud_estado() != LOGCLOUD_ENVIANDO);
}
int main(void) {
  snprintf(source, sizeof source, "%s/cloud-flow-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  int fd = mkstemp(source); assert(fd >= 0);
  const char *raw = "[app] FLOW_MARKER\n[net] password=PRIVATE_FIXTURE\n";
  assert(write(fd, raw, strlen(raw)) == (ssize_t)strlen(raw)); close(fd);
  assert(logcloud_estado() == LOGCLOUD_PARADO && !atomic_load(&calls));
  assert(logcloud_enviar()); wait_done();
  assert(logcloud_estado() == LOGCLOUD_PRONTO && atomic_load(&calls) == 1);
  char result[256]; logcloud_url(result, sizeof result); assert(strstr(result, "/v1/loglink/"));
  logcloud_esquecer();
  response_status = 503; assert(logcloud_enviar()); wait_done();
  assert(logcloud_estado() == LOGCLOUD_FALHOU && atomic_load(&calls) == 2);
  logcloud_motivo(result, sizeof result); assert(!strcmp(result, "HTTP 503"));
  logcloud_url(result, sizeof result); assert(!result[0]);
  logcloud_esquecer(); unlink(source);
  puts("PASS: real upload thread redacts before gzip/HTTP; narrow auth and capability-safe errors");
  return 0;
}
