// #707: rota IPv6 quebrada — a tentativa em familia qualquer falha e o pedido
// e refeito uma vez forcando IPv4, com o host marcado para as seguintes.
// Sem rede: curl falso como em tests/rede_saude.c.
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
static unsigned relogio = 1000;
static int clockFalso(clockid_t id, struct timespec *t) {
  (void)id; t->tv_sec = relogio / 1000; t->tv_nsec = (relogio % 1000) * 1000000;
  return 0;
}
#define clock_gettime clockFalso
#include "../src/rede.c"

typedef struct {
  size_t (*write)(void *, size_t, size_t, void *);
  void *data;
  long ipresolve;
  int resolveu;
} Fake;
static int chamadas, v4Chamadas, erroDupla, erroV4, http;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static void cleanup(void *c) { free(c); }
static void resetHandle(void *c) { (void)c; }
static int option(void *c, int o, ...) {
  va_list ap;
  va_start(ap, o);
  if (o == OPT_WRITEFUNCTION) ((Fake *)c)->write = va_arg(ap, size_t (*)(void *, size_t, size_t, void *));
  if (o == OPT_WRITEDATA) ((Fake *)c)->data = va_arg(ap, void *);
  if (o == OPT_IPRESOLVE) { ((Fake *)c)->ipresolve = va_arg(ap, long); ((Fake *)c)->resolveu = 1; }
  va_end(ap); return 0;
}
// Rota v6 morta: a tentativa normal (ipresolve=0) cai com erroDupla; em IPv4 a
// conexao abre e o corpo chega — a nao ser que erroV4 diga o contrario.
static int perform(void *c) {
  Fake *f = c;
  chamadas++;
  assert(f->resolveu);   // performV4 sempre escolhe a familia antes de chamar
  if (f->ipresolve != 1) {
    if (erroDupla == 28 && http == 500) return 28;
    return erroDupla;
  }
  v4Chamadas++;
  if (erroV4) return erroV4;
  http = 200;
  f->write("{}", 1, 2, f->data);
  return 0;
}
static int info(void *c, int o, ...) {
  va_list ap;
  (void)c;
  va_start(ap, o);
  if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = http;
  if (o == INFO_NUM_CONNECTS) *va_arg(ap, long *) = 1;
  if (o == INFO_TEMPO_TCP) *va_arg(ap, double *) = 0;
  va_end(ap); return 0;
}
int main(void) {
  char *r;
  int st;
  pronto = 1; curl_init = init; curl_cleanup = cleanup; curl_reset = resetHandle;
  curl_setopt_f = option; curl_perform = perform; curl_getinfo = info;

  // Connect recusado (7) na familia qualquer: refaz em v4 e entrega o corpo.
  erroDupla = 7;
  r = rede_baixar_st("https://cdn.example/fonte.mkv", 10, NULL, &st);
  assert(r && !strcmp(r, "{}") && st == 200);
  free(r);
  assert(chamadas == 2 && v4Chamadas == 1);
  assert(v4Marcado("https://cdn.example/fonte.mkv", relogio));

  // Host marcado: o pedido seguinte sai direto em IPv4, sem a tentativa v6.
  chamadas = v4Chamadas = http = 0;
  r = rede_baixar_st("https://cdn.example/outra.mkv", 10, NULL, &st);
  assert(r && st == 200 && chamadas == 1 && v4Chamadas == 1);
  free(r);

  // Prazo sem resposta (28, http=0) tambem recua: connect v6 engolido.
  chamadas = v4Chamadas = http = 0; erroDupla = 28;
  r = rede_baixar_st("https://lento.example/meta", 10, NULL, &st);
  assert(r && st == 200 && chamadas == 2 && v4Chamadas == 1);
  free(r);
  assert(v4Marcado("https://lento.example/meta", relogio));

  // Prazo depois de o servidor JA ter respondido nao e rota: nao recua.
  memset(v4Host, 0, sizeof v4Host);
  chamadas = v4Chamadas = 0; erroDupla = 28; http = 500;
  r = rede_baixar_st("https://meio.example/meta", 10, NULL, &st);
  assert(!r && v4Chamadas == 0);
  assert(!v4Marcado("https://meio.example/meta", relogio));
  http = 0;

  // Falha com a conexao JA aberta nao e familia: TLS (35) nao recua.
  chamadas = v4Chamadas = 0; erroDupla = 35;
  r = rede_baixar_st("https://tls.example/meta", 10, NULL, &st);
  assert(!r && v4Chamadas == 0);
  assert(!v4Marcado("https://tls.example/meta", relogio));

  // V4 tambem falhando: o erro volta a quem chamou; o host so e marcado quando
  // o IPv4 conecta de verdade.
  chamadas = v4Chamadas = 0; erroDupla = 7; erroV4 = 7;
  r = rede_baixar_st("https://morto.example/meta", 10, NULL, &st);
  assert(!r && v4Chamadas >= 1);
  assert(!v4Marcado("https://morto.example/meta", relogio));
  erroV4 = 0;

  // Marca caduca: passado o prazo, o host volta a tentar dual-stack.
  relogio += REDE_V4_MARCA_MS + 1;
  assert(!v4Marcado("https://morto.example/meta", relogio));

  puts("PASS: recuo IPv6->IPv4 por host (7 e 28 sem resposta), marca e caducidade");
  return 0;
}
