// Primeira geracao de UUID em instalacao nova (sem cliente.txt): o worker
// real de add-ons chega em dados_uuid por pseudonimoAddon -> dados_cliente_id
// -> clienteIdIniciar ao mesmo tempo que a tela de Diagnostico o chama direto
// (diagnostico.c:1960). Sonda da revisao r3 do Farol 10
// (evidencias/sol-farol4-10r3/07-uuid-concorrente.c) transformada em teste.
//
// O portao no __wrap_srand segura a primeira semeadura por um prazo curto
// esperando um concorrente, sobrepondo as primeiras chamadas nos dois fios.
// Sem a correcao o estatico semeado e o srand/rand globais correm sem
// sincronizacao entre os chamadores e o TSan da rc 66 (2cdee1560). Na ponta,
// dados_uuid nao toca estado global sem trava — semeadura por pthread_once
// proprio e rand_r sobre semente local — e o rc e 0.
#define main teste_autoplay_original
#include "tests/autoplay_alvo.c"
#undef main
#include "dados.h"

static atomic_int chamadasSrand;
static atomic_int iniciouDiag;
static pthread_t fioDiag;
static char uuidDiag[64];

void __real_srand(unsigned seed);
void __wrap_srand(unsigned seed) {
  if (getenv("FAROL_TESTE_BARREIRA")) {
    atomic_fetch_add_explicit(&chamadasSrand, 1, memory_order_relaxed);
    for (int i = 0; i < 1500 &&
         atomic_load_explicit(&chamadasSrand, memory_order_relaxed) < 2; ++i)
      usleep(1000);
  }
  __real_srand(seed);
}

// Abrir Diagnostico durante a primeira busca de add-ons: a tela chama
// dados_uuid direto, sem passar por dados_cliente_id.
static void *abrirDiagnostico(void *arg) {
  (void)arg;
  dados_uuid(uuidDiag, sizeof uuidDiag);
  atomic_store(&iniciouDiag, 1);
  return NULL;
}

static int pluginMudo(const char *id, const char *tipo, int (*cancelado)(void *),
                      void *ctx, OrigemAviso aviso, void *avisoU, void *saida) {
  (void)id; (void)tipo; (void)cancelado; (void)ctx; (void)saida;
  assert(!pthread_create(&fioDiag, NULL, abrirDiagnostico, NULL));
  // fase 3 = respondeu=0: avisoExtra -> registrarLatencia -> pseudonimoAddon
  // -> dados_cliente_id() -> clienteIdIniciar -> dados_uuid neste fio.
  aviso(avisoU, 0, "plugin-sintetico", "mudo", 1, NULL, 0);
  aviso(avisoU, 0, "plugin-sintetico", "mudo", 3, NULL, 0);
  return 0;
}

static int uuidValido(const char *u) {
  return strlen(u) == 36 && u[8] == '-' && u[13] == '-' && u[18] == '-' &&
         u[23] == '-' && u[14] == '4' && strchr("89ab", u[19]) != NULL;
}

int main(void) {
  char caminho[600], lido[128] = "";
  const char *idMain;
  FILE *f;

  setvbuf(stdout, NULL, _IONBF, 0);
  assert(getenv("NUVIO_DADOS") && *getenv("NUVIO_DADOS"));
  dados_iniciar(NULL);

  addons_definir_origem_extra(pluginMudo, pluginAtivo);
  addons_buscar("tt9:1:1", "series");
  for (int i = 0; i < 3000 && addons_estado() == ADD_BUSCANDO; ++i) usleep(1000);
  addons_encerrar();
  assert(!pthread_join(fioDiag, NULL));
  assert(atomic_load(&iniciouDiag));

  CONFERE(uuidValido(uuidDiag), "UUID do Diagnostico valido (\"%s\")", uuidDiag);
  idMain = dados_cliente_id();
  CONFERE(uuidValido(idMain), "ID da instalacao valido (\"%s\")", idMain);
  CONFERE(strcmp(uuidDiag, idMain) != 0,
          "UUID do Diagnostico difere do ID da instalacao");

  snprintf(caminho, sizeof caminho, "%s/cliente.txt", dados_dir());
  f = fopen(caminho, "rb");
  assert(f);
  assert(fgets(lido, sizeof lido, f));
  fclose(f);
  lido[strcspn(lido, "\r\n")] = 0;
  CONFERE(!strcmp(lido, idMain), "cliente.txt tem o ID publicado (\"%s\")", lido);

  printf("chamadas a srand: %d\n", atomic_load(&chamadasSrand));
  if (falhas) { printf("uuid_concorrente: FALHA (%d)\n", falhas); return 1; }
  puts("uuid_concorrente: PASSA");
  return 0;
}
