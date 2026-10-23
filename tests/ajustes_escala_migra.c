// Interface defaults to 85%; migrate legacy defaults and preserve versioned choices.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void escreve(const char *dir, const char *linha) {
  char p[1200]; FILE *f;
  snprintf(p, sizeof p, "%s/ajustes.txt", dir);
  f = fopen(p, "w"); assert(f); fputs(linha, f);
  if (strstr(linha, "tamanhoAjustesLocal"))
    fputs("escalaTextoLocal 2\nescalaTextoVersaoLocal 1\n", f);
  fclose(f);
}
static void apaga_marca(const char *dir) {
  char p[1200];
  snprintf(p, sizeof p, "%s/ajustesescala-203.txt", dir); unlink(p);
  snprintf(p, sizeof p, "%s/ajustesescala-205.txt", dir); unlink(p);
}
int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-escala-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  assert(valorPadrao[AJ_TAMANHO_AJUSTES] == 2);
  ajustes_dir(dir);
  assert(ajustes_escala_texto() == 0.85f);                     // fresh interface
  assert(somenteDesteAparelho(AJ_ESCALA_TEXTO));
  for (int v = -1; v <= 6; v++) {
    static const float scales[] = { .85f, .85f, .9f, 1.f, 1.1f, 1.2f, 1.3f, .85f };
    valor[AJ_ESCALA_TEXTO] = v;
    assert(ajustes_escala_texto() == scales[v + 1]);
  }
  escreve(dir, "escalaTextoLocal 2\nvidroOpacLocal 1\n"); ajustes_dir(dir);
  assert(ajustes_escala_texto() == .85f);                      // unrelated save persisted the legacy default
  ajustes_dir(dir); assert(ajustes_escala_texto() == .85f);     // migration persists
  for (int v = 0; v < 6; v++) {
    assert(definirValorDireto(AJ_ESCALA_TEXTO, v));
    assert(gravar());
    ajustes_dir(dir); assert(valor[AJ_ESCALA_TEXTO] == v);     // saved choices, including 100%
  }
  for (int v = -1; v <= 6; v++) {
    char linha[64];
    snprintf(linha, sizeof linha, "escalaTextoLocal %d\n", v);
    escreve(dir, linha); ajustes_dir(dir);
    assert(valor[AJ_ESCALA_TEXTO] == (v >= 0 && v < 6 && v != 2 ? v : 0));
    ajustes_dir(dir);
    assert(valor[AJ_ESCALA_TEXTO] == (v >= 0 && v < 6 && v != 2 ? v : 0));
  }
  escreve(dir, "escalaTextoVersaoLocal 1\nescalaTextoLocal 2\n"); ajustes_dir(dir);
  assert(ajustes_escala_texto() == 1.f);                       // key order does not matter
  escreve(dir, "escalaTextoLocal 2\nescalaTextoVersaoLocal 1\n"); ajustes_dir(dir);
  assert(ajustes_escala_texto() == 1.f);
  char *merged = NULL;
  const char *blob = "{\"escalaTextoLocal\":4,\"escala_texto_local\":5}";
  ajustes_aplicar_blob(blob); assert(ajustes_escala_texto() == 1.f);
  assert(ajustes_mesclar_blob(blob, &merged) > 0 && merged);
  assert(strstr(merged, "c_settings_v1") && strstr(merged, "c_devices_v1"));
  assert(ajustes_escala_texto() == 1.f);
  free(merged);
  escreve(dir, ""); ajustes_dir(dir);
  assert(ajustes_escala_texto() == .85f);                      // absent key resets stale state
  char path[1200]; snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  unlink(path); ajustes_dir(dir); assert(ajustes_escala_texto() == .85f);
  // Settings-size assertions below require an explicit 100% text baseline.
  assert(definirValorDireto(AJ_ESCALA_TEXTO, 2));
  assert(ajustes_tamanho_ajustes() == 1.0f);
  for (int v = -1; v <= 3; v++) {
    static const float escalas[] = { 1.0f, 0.8f, 0.9f, 1.0f, 1.0f };
    valor[AJ_TAMANHO_AJUSTES] = v;
    assert(ajustes_tamanho_ajustes() == escalas[v + 1]);
  }
  apaga_marca(dir);
  escreve(dir, "tamanhoAjustesLocal 1\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 2);                       // pre-2.0.3 default
  for (int v = 0; v < 3; v++) {
    valor[AJ_TAMANHO_AJUSTES] = v; assert(gravar());
    ajustes_dir(dir); assert(valor[AJ_TAMANHO_AJUSTES] == v);   // choices after migration
  }
  apaga_marca(dir);
  assert(dados_gravar("ajustesescala-203.txt", "1\n"));
  escreve(dir, "tamanhoAjustesLocal 0\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 2);                       // 2.0.3 factory default
  ajustes_dir(dir); assert(valor[AJ_TAMANHO_AJUSTES] == 2);      // persisted
  apaga_marca(dir);
  assert(dados_gravar("ajustesescala-203.txt", "1\n"));
  escreve(dir, "tamanhoAjustesLocal 1\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 1);                       // explicit 90% after 2.0.3
  apaga_marca(dir); escreve(dir, "tamanhoAjustesLocal 0\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 2);                       // unmarked 80%
  apaga_marca(dir); escreve(dir, "tamanhoAjustesLocal 2\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 2);                       // explicit 100%
  printf("ajustes_escala_migra: ok\n");
  return 0;
}
