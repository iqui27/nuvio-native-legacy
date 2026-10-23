// #488: A VOLTA PEDIDA NA TROCA DE PERFIL ESPERA O CICLO DA CONTA.
// A espera do arranque e uma por execucao; sem rearmar na troca, a volta de
// desc_repetir() lia a lista de addons ainda vazia e publicava um catalogo ralo.
// Reaproveita os dubles de tests/montagem_cedo.c (main dele renomeado).
//
//   bash tests/troca_perfil_espera_conta.sh
#include <pthread.h>
#include <stdatomic.h>
#define main montagem_cedo_main
#include "montagem_cedo.c"
#undef main

// Dubles que montagem_cedo.c ainda nao tem (descoberta.c cresceu).
void addons_manifesto_lido_base(int i, const char *base, const char *destino,
                                unsigned versao, const char *corpo) {
  (void)i; (void)base; (void)destino; (void)versao; (void)corpo; }
void ws_destino(const char *base, const char *usuario, int perfil, char out[20]) {
  (void)base; (void)usuario; (void)perfil; out[0] = 0; }

static atomic_int conta, terminou;   // conta: o que esperaAddons devolve (2 = ciclo no ar)
static unsigned geracaoDaVolta;
static int fakeEspera(void) { return atomic_load(&conta); }
static void *volta(void *u) {
  (void)u;
  esperarConta(geracaoDaVolta);
  atomic_store(&terminou, 1);
  return NULL;
}
static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHOU: "); falhas++; } else printf("ok: "); \
                             printf(__VA_ARGS__); printf("\n"); } while (0)
// Larga uma volta e diz se ela ainda esta esperando depois de 300 ms.
static int voltaPresa(pthread_t *t, unsigned g) {
  atomic_store(&terminou, 0);
  geracaoDaVolta = g;
  pthread_create(t, NULL, volta, NULL);
  usleep(300 * 1000);
  return !atomic_load(&terminou);
}

int main(void) {
  pthread_t t;
  setvbuf(stdout, NULL, _IOLBF, 0);
  desc_espera_addons_definir(fakeEspera);
  atomic_store(&conta, 2);

  CONFERE(voltaPresa(&t, montagemGeracao), "arranque: a primeira volta espera a conta");
  atomic_store(&conta, 0); pthread_join(t, NULL);

  atomic_store(&conta, 2);
  CONFERE(!voltaPresa(&t, montagemGeracao), "uma espera por execucao: a segunda volta nao espera");
  pthread_join(t, NULL);

  desc_esperar_conta();
  CONFERE(voltaPresa(&t, montagemGeracao), "troca de perfil: a volta nova espera de novo a conta");
  atomic_store(&conta, 0); pthread_join(t, NULL);

  // a volta velha (condenada) pega o pedido primeiro: o pedido passa para a substituta
  atomic_store(&conta, 2);
  desc_esperar_conta();
  CONFERE(!voltaPresa(&t, montagemGeracao - 1), "volta condenada nao fica presa");
  pthread_join(t, NULL);
  CONFERE(voltaPresa(&t, montagemGeracao), "a substituta da condenada espera a conta");
  atomic_store(&conta, 0); pthread_join(t, NULL);

  printf(falhas ? "troca_perfil_espera_conta: %d falha(s)\n" : "troca_perfil_espera_conta: ok\n", falhas);
  return falhas ? 1 : 0;
}
