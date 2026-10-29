// Exercise the real guide probe with an endless binary TS response.
#include "../src/streams.c"
#include <assert.h>

int perfis_sessao_atual(uint64_t s) { return s == 0; }
static int active, calls;
char *rede_baixar_medido_controle(const char *url, int seconds,
    const char *const *headers, const RedeControle *ctl, RedeMedida *md) {
  (void)url; (void)seconds;
  assert(headers && !strcmp(headers[0], "Referer: https://fixture.invalid/"));
  assert(ctl && ctl->max_bytes > 0 && ctl->max_bytes <= 65536);
  assert(ctl->cancelado);
  active++; calls++;
  char *body = calloc(1, (size_t)ctl->max_bytes + 1);
  assert(body);
  for (long i = 0; i + 188 <= ctl->max_bytes; i += 188) body[i] = 0x47;
  md->status = 200; md->bytes = ctl->max_bytes; md->limitado = 1;
  active--; // Synchronous network return closes the bounded transfer.
  return body;
}
int main(void) {
  CanalCandidata a = {0};
  unsigned char classe = 0;
  CanalConferencia c = {.n = 1, .candidatas = &a, .classes = &classe};
  pthread_mutex_init(&c.trava, NULL);
  strcpy(a.url, "https://fixture.invalid/live");
  strcpy(a.cabecalhos, "Referer: https://fixture.invalid/");
  fioCanal(&c);
  assert(calls == 1 && active == 0);
  assert(classe == 1);
  assert(a.tsContinuo);
  pthread_mutex_destroy(&c.trava);
  puts("PASS: bounded binary TS probe is alive before playback");
}
