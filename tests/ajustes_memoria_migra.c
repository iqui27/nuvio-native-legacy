// 2.0.5: highcache migrates memory without an explicit choice once per device.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

static int falhas;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond); falhas++; } } while (0)

static void escreve(const char *dir, const char *linha) {
  char p[1200]; FILE *f;
  snprintf(p, sizeof p, "%s/ajustes.txt", dir);
  f = fopen(p, "w"); assert(f); fputs(linha, f); assert(!fclose(f));
}
static void dispositivo(char *dir, size_t tamanho) {
  snprintf(dir, tamanho, "%s/nuvio-memoria-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  memoriaEscolhida = 0;
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  // Older migrations must not accidentally persist the memory migration.
  assert(dados_gravar(RES_ARQ_MIGRADA, "1\n"));
  assert(dados_gravar("envio-151.txt", "1\n"));
  assert(dados_gravar("ajustesescala-203.txt", "1\n"));
  assert(dados_gravar("perfilfundo-20.txt", "1\n"));
  assert(dados_gravar("aparencia-20.txt", "1\n"));
}
static void recarrega(const char *dir) {
  memcpy(valor, valorPadrao, sizeof valor);
  ajustes_dir(dir);
}
static void marca(int esperada) {
  char *m = dados_ler("memoriaimagens-205.txt");
  CHECK((m != NULL) == esperada);
  free(m);
}
int main(int argc, char **argv) {
  char dir[1024], outro[1024];
  if (argc == 3 && !strcmp(argv[1], "--reload")) {
    setenv("NUVIO_DADOS", argv[2], 1); dados_iniciar(argv[2]);
    recarrega(argv[2]); CHECK(valor[AJ_TEX_MB] == 4); marca(1);
    printf("normal reload: %s\n", falhas ? "FAIL" : "ok (300 MB)");
    return falhas != 0;
  }
#ifdef NV_TEX_MB_FIXO
  const int migrado = 4, temMarca = 1;
#else
  const int migrado = 0, temMarca = 0;
#endif
  assert(valorPadrao[AJ_TEX_MB] == 0);
  // Saving only log delivery in the old app also wrote the default zero.
  dispositivo(dir, sizeof dir);
  memcpy(valor, valorPadrao, sizeof valor);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  ajustes_definir_envio_auto(0);
  recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == migrado); marca(temMarca);
  CHECK(valor[AJ_ENVIO_AUTO] == 1);
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado);
  // A bare legacy zero is ambiguous; a nonzero value proves a choice.
  dispositivo(dir, sizeof dir);
  escreve(dir, "texturasMB 0\n"); recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == migrado); marca(temMarca);
  // A failed migration save must remain pending and retry on the next load.
  dispositivo(dir, sizeof dir);
  escreve(dir, "texturasMB 0\n");
  { char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dir);
    assert(!mkdir(tmp, 0700));
    recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado); marca(0);
    assert(!rmdir(tmp));
    recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado); marca(temMarca);
    recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado);
  }
  // Confirming Automatic even when already selected records explicit intent.
  dispositivo(dir, sizeof dir);
  memcpy(valor, valorPadrao, sizeof valor);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  uxAbrirEditor(AJ_TEX_MB); uxAplicar();
  CHECK(!uxEditor);
  { char *t = dados_ler("ajustes.txt");
    CHECK(t && strstr(t, "texturasEscolhaLocal 1\n")); free(t); }
  recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == 0); marca(temMarca);
  // A failed save must not leave an explicit-choice marker in memory.
  dispositivo(dir, sizeof dir);
  memcpy(valor, valorPadrao, sizeof valor);
  snprintf(dirAjustes, sizeof dirAjustes, "%s/absent", dir);
  CHECK(!definirValorDireto(AJ_TEX_MB, 0));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  ajustes_definir_envio_auto(0);
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado);
  // Opening, browsing and cancelling the memory selector is not a choice.
  dispositivo(dir, sizeof dir);
  escreve(dir, "texturasMB 0\n");
  uxAbrirEditor(AJ_TEX_MB); uxEvento(SDLK_DOWN); uxEvento(SDLK_ESCAPE);
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado);
  dispositivo(dir, sizeof dir);
  escreve(dir, "texturasMB 1\n"); recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == 1); marca(temMarca);
  // An existing file without the key migrates and survives a disk reload.
  dispositivo(dir, sizeof dir);
  escreve(dir, "envioAuto 0\n"); recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == migrado); marca(temMarca);
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == migrado);
  // Changing back to Automatic after migration survives another reload.
  CHECK(definirValorDireto(AJ_TEX_MB, 0));
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == 0);
  // Even an absent key cannot rerun a completed device migration.
  escreve(dir, "envioAuto 0\n"); recarrega(dir);
  CHECK(valor[AJ_TEX_MB] == 0);
  // A second device has its own marker; no settings file must migrate too.
  dispositivo(outro, sizeof outro);
  marca(0); recarrega(outro);
  CHECK(valor[AJ_TEX_MB] == migrado); marca(temMarca);
  recarrega(outro); CHECK(valor[AJ_TEX_MB] == migrado);
  // Returning to the first device retains its one-time marker.
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  recarrega(dir); CHECK(valor[AJ_TEX_MB] == 0); marca(temMarca);
#ifdef NV_TEX_MB_FIXO
  if (argc == 2) {
    FILE *f = fopen(argv[1], "w"); assert(f);
    fprintf(f, "%s\n", outro); assert(!fclose(f));
  }
#endif
  printf("ajustes_memoria_migra: %s (%d failures)\n", falhas ? "FAIL" : "ok", falhas);
  return falhas != 0;
}
