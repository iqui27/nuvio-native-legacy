// Corrida da primeira inicializacao do ID da instalacao (cliente.txt): o
// worker real de add-ons chega em dados_cliente_id pelo pseudonimo do farol
// (pseudonimoAddon) ao mesmo tempo que o fio de sincronizacao de progresso o
// chama direto. Sonda da revisao r2 do Farol 10
// (evidencias/sol-farol4-10r2/07-sal-corrida.c) transformada em teste.
//
// O "portao" no __wrap_fopen segura o PRIMEIRO fio que ve cliente.txt ausente
// por um prazo curto, esperando um concorrente. Sem a correcao os dois fios
// chegam e inicializam juntos — o TSan aponta a corrida e os IDs podem
// divergir. Com a inicializacao unica so um fio chega ao fopen, o prazo
// apenas expira, e os dois leem o mesmo ID publicado.
#define main teste_autoplay_original
#include "tests/autoplay_alvo.c"
#undef main
#include "dados.h"
#include "farolanel.h"

static atomic_int aberturasCliente;   // fios que viram cliente.txt ausente
static atomic_int iniciouSync;
static pthread_t fioSync;
static char idWorker[64], idSync[64];

// dados.c compila com _FILE_OFFSET_BITS=64: seu fopen vira fopen64 e os dois
// simbolos precisam do wrap para o portao existir de verdade.
static FILE *portao(FILE *f, const char *path, const char *mode) {
  if (getenv("FAROL_TESTE_BARREIRA") && !f && !strcmp(mode, "rb") &&
      strstr(path, "/cliente.txt") &&
      atomic_fetch_add(&aberturasCliente, 1) == 0) {
    for (int k = 0; k < 3000 && atomic_load(&aberturasCliente) < 2; k++)
      usleep(1000);
  }
  return f;
}
// O portao entra por -Wl,--wrap=fopen/--wrap=fopen64 no ELF; o ld64 do
// Mach-O nao tem --wrap, entao no __APPLE__ o proprio binario define fopen
// (interposicao para as chamadas dos nossos objetos) e o RTLD_NEXT alcanca
// o fopen de verdade da libc — a mesma receita do srand interposto em
// tests/uuid_concorrente.c. fopen64 nao existe no Mach-O (e o
// _FILE_OFFSET_BITS do dados.c nao renomeia nada la): o fopen dos nossos
// objetos ja basta para o portao existir de verdade.
#if defined(__APPLE__)
#include <dlfcn.h>
FILE *fopen(const char *path, const char *mode) {
  FILE *(*real)(const char *, const char *);
  real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
  return portao(real(path, mode), path, mode);
}
#else
FILE *__real_fopen(const char *path, const char *mode);
FILE *__real_fopen64(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode) {
  return portao(__real_fopen(path, mode), path, mode);
}
FILE *__wrap_fopen64(const char *path, const char *mode) {
  return portao(__real_fopen64(path, mode), path, mode);
}
#endif

static void *syncInicial(void *unused) {
  const char *id;
  (void)unused;
  id = dados_cliente_id();
  assert(id[0]);
  snprintf(idSync, sizeof idSync, "%s", id);
  return NULL;
}

static int pluginMudo(const char *id, const char *tipo, int (*cancelado)(void *),
                      void *ctx, OrigemAviso aviso, void *avisoU, void *saida) {
  (void)id; (void)tipo; (void)cancelado; (void)ctx; (void)saida;
  assert(!pthread_create(&fioSync, NULL, syncInicial, NULL));
  atomic_store(&iniciouSync, 1);
  // fase 3 = respondeu=0: avisoExtra -> registrarLatencia -> pseudonimoAddon
  // -> dados_cliente_id() neste mesmo fio do worker.
  aviso(avisoU, 0, "plugin-sintetico", "mudo", 1, NULL, 0);
  aviso(avisoU, 0, "plugin-sintetico", "mudo", 3, NULL, 0);
  snprintf(idWorker, sizeof idWorker, "%s", dados_cliente_id());
  return 0;
}

int main(void) {
  char caminho[600], lido[128] = "";
  const char *idMain;
  FILE *f;
  int linhas = 0;

  setvbuf(stdout, NULL, _IONBF, 0);
  assert(getenv("NUVIO_DADOS") && *getenv("NUVIO_DADOS"));
  dados_iniciar(NULL);

  addons_definir_origem_extra(pluginMudo, pluginAtivo);
  addons_buscar("tt9:1:1", "series");
  for (int k = 0; k < 3000 && addons_estado() == ADD_BUSCANDO; k++) usleep(1000);
  assert(atomic_load(&iniciouSync));
  addons_encerrar();
  assert(!pthread_join(fioSync, NULL));

  idMain = dados_cliente_id();
  CONFERE(idWorker[0] && idSync[0], "worker e fio de sync obtiveram um ID");
  CONFERE(!strcmp(idWorker, idSync),
          "worker e sync leem o mesmo ID (\"%s\" vs \"%s\")", idWorker, idSync);
  CONFERE(!strcmp(idMain, idSync), "leitura posterior devolve o mesmo ID (\"%s\")", idMain);
  CONFERE(atomic_load(&aberturasCliente) == 1,
          "so um fio viu cliente.txt ausente (%d aberturas)", atomic_load(&aberturasCliente));

  snprintf(caminho, sizeof caminho, "%s/cliente.txt", dados_dir());
  f = fopen(caminho, "rb");
  assert(f);
  while (fgets(lido, sizeof lido, f)) if (lido[0] && lido[0] != '\n') linhas++;
  fclose(f);
  CONFERE(linhas == 1, "cliente.txt gravado uma unica vez (%d linhas)", linhas);
  lido[strcspn(lido, "\r\n")] = 0;
  CONFERE(!strcmp(lido, idMain), "cliente.txt tem o ID lido pelos fios (\"%s\")", lido);

  if (falhas) { printf("cliente_corrida: FALHA (%d)\n", falhas); return 1; }
  puts("cliente_corrida: PASSA");
  return 0;
}
