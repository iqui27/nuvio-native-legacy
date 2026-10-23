// #382: exercita os pedidos reais sem rede, com curl e relogio falsos.
#include <assert.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
static unsigned relogio = 1000;
static int rajada;
static _Thread_local int rodada;
static pthread_mutex_t rajadaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t rajadaCond = PTHREAD_COND_INITIALIZER;
static int chegaram;
static void esperarRajada(void) {
  pthread_mutex_lock(&rajadaTrava);
  chegaram++;
  pthread_cond_broadcast(&rajadaCond);
  while (chegaram < rodada * 3) pthread_cond_wait(&rajadaCond, &rajadaTrava);
  pthread_mutex_unlock(&rajadaTrava);
}
static int clockFalso(clockid_t id, struct timespec *t) {
  unsigned ms = rajada ? 1000u + (unsigned)rodada * 5000u : relogio;
  (void)id; t->tv_sec = ms / 1000; t->tv_nsec = (ms % 1000) * 1000000;
  return 0;
}
#define clock_gettime clockFalso
#define REDE_POST_ESPERA_MS 1
#include "../src/rede.c"
#include "../src/redesaude.c"

typedef struct {
  size_t (*write)(void *, size_t, size_t, void *);
  void *data;
  long prazo, conexao;
  char *err;
} Fake;
static int chamadas, notas, erro, http, recuperar, dnsLento, falhasAteOk;
static void *init(void) { return calloc(1, sizeof(Fake)); }
static void cleanup(void *c) { free(c); }
static int option(void *c, int o, ...) {
  va_list ap;
  va_start(ap, o);
  if (o == OPT_WRITEFUNCTION) ((Fake *)c)->write = va_arg(ap, size_t (*)(void *, size_t, size_t, void *));
  if (o == OPT_WRITEDATA) ((Fake *)c)->data = va_arg(ap, void *);
  if (o == OPT_ERRORBUFFER) ((Fake *)c)->err = va_arg(ap, char *);
  if (o == OPT_TIMEOUT_MS) ((Fake *)c)->prazo = va_arg(ap, long);
  if (o == OPT_CONNECTTIMEOUT_MS) ((Fake *)c)->conexao = va_arg(ap, long);
  va_end(ap); return 0;
}
static int perform(void *c) {
  Fake *f = c;
  if (rajada) {
    assert(f->prazo == (rodada ? 25000 : 30000));
    assert(f->conexao == 10000);
    rodada++;
    __atomic_add_fetch(&chamadas, 1, __ATOMIC_RELAXED);
    esperarRajada();
    return 28;
  }
  chamadas++;
  if (dnsLento) {   // resolvedor lento do boot: curl 28 sem resolver
    relogio += 5000;
    if (falhasAteOk && chamadas > falhasAteOk) {
      http = 200;
      f->write("{}", 1, 2, f->data);
      return 0;
    }
    if (f->err) snprintf(f->err, 256, "Resolving timed out after 5001 milliseconds");
    return 28;
  }
  if (recuperar && chamadas == 2) {
    // Duas falhas finais anteriores, mas este pedido ainda pode recuperar.
    assert(!rede_saude_offline());
    http = 200;
    f->write("{}", 1, 2, f->data);
    return 0;
  }
  relogio += 2000;
  return erro;
}
static int info(void *c, int o, ...) {
  va_list ap;
  (void)c;
  va_start(ap, o);
  if (o == INFO_RESPONSE_CODE) *va_arg(ap, long *) = http;
  if (o == INFO_NUM_CONNECTS) *va_arg(ap, long *) = 0;
  va_end(ap); return 0;
}
static void nota(int codigo, const char *url, unsigned primeiraMs) {
  __atomic_add_fetch(&notas, 1, __ATOMIC_RELAXED);
  rede_saude_nota_desde(codigo, url, primeiraMs);
}
static void reset(void) {
  memset(&global, 0, sizeof global);
  chamadas = notas = http = recuperar = dnsLento = falhasAteOk = 0;
  relogio = 1000;
}
static void falhasAnteriores(void) {
  rede_saude_nota(6, "https://a.example/meta");
  relogio += 2000;
  rede_saude_nota(6, "https://b.example/meta");
}
static void *getRajada(void *url) {
  int st;
  assert(!rede_baixar_st(url, 30, NULL, &st) && st == 0);
  return NULL;
}
static void testarRajada(void) {
  pthread_t fios[3];
  const char *urls[] = { "https://a.example/meta", "https://b.example/meta", "https://c.example/meta" };
  reset(); rajada = 1;
  chegaram = 0;
  for (int i = 0; i < 3; i++) assert(!pthread_create(&fios[i], NULL, getRajada, (void *)urls[i]));
  for (int i = 0; i < 3; i++) assert(!pthread_join(fios[i], NULL));
  rajada = 0; relogio = 11000;
  assert(chamadas == 6 && notas == 3);
  assert(global.falhas == 3 && global.nHosts == 3);
  assert(global.primeira == 6000 && global.offline);
  assert(rede_saude_offline() && rede_saude_seq() == 1);
  puts("PASS: 3 GETs paralelos, curl28 em 5 s + retry de 5 s, offline aos 10 s.");
}
static void testarTick(void) {
  reset();
  rede_saude_nota(28, "https://a.example/meta");
  rede_saude_nota(28, "https://b.example/meta");
  rede_saude_nota(28, "https://c.example/meta");
  relogio = 3999;
  assert(!rede_saude_offline() && rede_saude_seq() == 0);
  relogio = 4000;
  assert(rede_saude_seq() == 1 && rede_saude_offline());
  assert(global.falhas == 3 && global.nHosts == 3);
  // Carencia tambem vence pelo tick, sem contar mais pedidos.
  rede_saude_nota(0, "https://a.example/meta");
  relogio = 5000;
  rede_saude_nota(28, "https://a.example/meta");
  rede_saude_nota(28, "https://b.example/meta");
  rede_saude_nota(28, "https://c.example/meta");
  relogio = 13999;
  assert(rede_saude_seq() == 2 && !rede_saude_offline());
  relogio = 14000;
  assert(rede_saude_seq() == 3 && rede_saude_offline());

  // Sucesso concorrente impede retroceder a janela para antes dele.
  reset(); relogio = 9000;
  rede_saude_nota(0, "https://ok.example/meta");
  relogio = 11000;
  rede_saude_nota_desde(28, "https://a.example/meta", 6000);
  rede_saude_nota_desde(28, "https://b.example/meta", 6000);
  rede_saude_nota_desde(28, "https://c.example/meta", 6000);
  assert(!rede_saude_offline() && global.primeira == 9000);
  relogio = 12000;
  assert(rede_saude_seq() == 1 && rede_saude_offline());
  puts("PASS: tick reavalia janela/carencia sem novas notas e respeita sucesso concorrente.");
}
static void testarDnsLento(void) {
  int st;
  char *r;
  // Rajada de timeouts de DNS logo depois do boot, em hosts distintos: nao e queda.
  reset(); dnsLento = 1;
  assert(!rede_baixar_st("https://a.example/m", 10, NULL, &st));
  assert(!rede_baixar_st("https://b.example/m", 10, NULL, &st));
  assert(!rede_baixar_st("https://c.example/m", 10, NULL, &st));
  assert(!rede_saude_offline() && global.falhas == 0 && rede_saude_seq() == 0);
  // O envio do registro repete e termina bem quando o DNS volta na 3a tentativa.
  chamadas = 0; falhasAteOk = 2;
  r = rede_postar_repetindo("https://d.example/v1/registro", 30, NULL, "{}", &st, 3);
  assert(r && st == 200 && chamadas == 3);
  free(r);
  assert(!rede_saude_offline() && global.temOk);
  // Passado o arranque sem nenhum sucesso, timeout de DNS volta a valer.
  reset(); dnsLento = 1;
  assert(!rede_baixar_st("https://a.example/m", 10, NULL, &st));
  relogio += REDE_SAUDE_ARRANQUE;
  assert(!rede_baixar_st("https://a.example/m", 10, NULL, &st));
  assert(!rede_baixar_st("https://b.example/m", 10, NULL, &st));
  assert(!rede_baixar_st("https://c.example/m", 10, NULL, &st));
  assert(rede_saude_offline());
  // Resposta HTTP do servidor nao e repetida.
  reset(); http = 503; erro = 0; recuperar = 0;
  r = rede_postar_repetindo("https://d.example/v1/registro", 30, NULL, "{}", &st, 3);
  assert(chamadas == 1);
  free(r);
  puts("PASS: timeouts de DNS no arranque nao declaram offline; envio repete e conclui.");
}
int main(int argc, char **argv) {
  char *r;
  int st;
  pronto = 1; curl_init = init; curl_cleanup = cleanup;
  curl_setopt_f = option; curl_perform = perform; curl_getinfo = info;
  rede_avisar_saude(nota);
  reset(); falhasAnteriores(); erro = 28;
  if (argc > 1 && !strcmp(argv[1], "http")) {
    // HTTP recebido + corpo lento continua falha do pedido, mas ha internet.
    http = 503;
    r = rede_postar_st("https://c.example/meta", 10, NULL, "{}", &st);
    assert(!r && chamadas == 1 && notas == 1);
    assert(!rede_saude_offline());
    assert(global.falhas == 0);
    puts("PASS: resposta HTTP com curl28 nao declara offline.");
    return 0;
  }
  recuperar = 1;
  r = rede_baixar_st("https://c.example/meta", 10, NULL, &st);
  assert(r && st == 200 && chamadas == 2 && notas == 1);
  free(r);
  assert(!rede_saude_offline() && rede_saude_seq() == 0);

  // Sem resposta de varios hosts: resultados finais ainda declaram offline.
  reset(); falhasAnteriores(); erro = 28;
  r = rede_baixar_st("https://c.example/meta", 10, NULL, &st);
  assert(!r && st == 0 && chamadas == 2 && notas == 1);
  assert(rede_saude_offline() && rede_saude_seq() == 1);
  // HTTP4xx/5xx basta para reconhecer a volta, mesmo sem corpo completo.
  erro = 56; http = 503;
  r = rede_postar_st("https://c.example/meta", 10, NULL, "{}", &st);
  assert(!r);
  assert(!rede_saude_offline() && rede_saude_seq() == 2);
  puts("PASS: retry recuperado, falhas finais distintas e volta por HTTP.");
  testarRajada();
  testarTick();
  testarDnsLento();
  return 0;
}
