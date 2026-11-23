#include "redesaude.h"
#include "farolanel.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

static void *pedidosOk(void *ctx) {
  (void)ctx;
  for (int i = 0; i < 1000; i++)
    rede_saude_nota(0, "https://privado.test/caminho?token=segredo");
  return NULL;
}

int main(int argc, char **argv) {
  RedeSaude s = {0};
  FarolEvento e[8];
  farolanel_zerar();
  for (unsigned i = 0; i < 1000; i++)
    assert(!rede_saude_passo(&s, 1, 1, i));
  assert(farolanel_ler(e, 8, 0) == 0);
  assert(!rede_saude_passo(&s, 0, 1, 2000));
  assert(!rede_saude_passo(&s, 0, 2, 3000));
  assert(rede_saude_passo(&s, 0, 1, 5000));
  for (unsigned i = 5001; i < 6000; i++)
    assert(!rede_saude_passo(&s, 0, 2, i));
  assert(rede_saude_passo(&s, 1, 2, 6000));
  assert(farolanel_ler(e, 8, 0) == 2);
  assert(e[0].ev == FE_REDE_ESTADO && e[0].mod == FM_REDE);
  assert(e[0].a == 1 && e[0].b == 3 && e[0].sev == 1);
  assert(!strcmp(e[0].tok, "recusa"));
  assert(e[1].ev == FE_REDE_ESTADO && e[1].a == 0 && e[1].b == 0);
  assert(e[1].sev == 0);
  for (unsigned i = 6001; i < 7001; i++)
    assert(!rede_saude_passo(&s, 1, 1, i));
  assert(farolanel_ler(e, 8, 0) == 2);
  farolanel_zerar();
  pthread_t fios[4];
  for (int i = 0; i < 4; i++) assert(!pthread_create(&fios[i], NULL, pedidosOk, NULL));
  for (int i = 0; i < 4; i++) assert(!pthread_join(fios[i], NULL));
  assert(!farolanel_ler(e, 8, 0));
  int codigo = argc > 1 ? atoi(argv[1]) : 35;
  struct timespec t;
  assert(!clock_gettime(CLOCK_MONOTONIC, &t));
  unsigned inicio = (unsigned)((unsigned long long)t.tv_sec * 1000 + t.tv_nsec / 1000000);
  rede_saude_nota_desde(codigo, "https://a.test/segredo", inicio);
  rede_saude_nota_desde(codigo, "https://b.test/segredo", inicio);
  rede_saude_nota_desde(codigo, "https://a.test/segredo", inicio);
  struct timespec espera = {3, 10000000};
  nanosleep(&espera, NULL);
  assert(rede_saude_offline());
  for (int i = 0; i < 1000; i++) { assert(rede_saude_offline()); rede_saude_seq(); }
  rede_saude_nota(0, "https://a.test/segredo");
  assert(farolanel_ler(e, 8, 0) == 2);
  const char *classe = codigo == 6 ? "dns" : codigo == 28 ? "prazo" : codigo == 35 ? "tls" : "recusa";
  assert(e[0].a == 1 && e[1].a == 0 && !strcmp(e[0].tok, classe));
  assert(!strcmp(e[1].tok, classe));
  return 0;
}
