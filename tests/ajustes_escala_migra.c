// 2.0.3: o tamanho dos Ajustes volta a 80% UMA vez para quem ficou no padrao
// antigo (90%); 100% escolhido fica; uma troca depois da marca fica.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

static void escreve(const char *dir, const char *linha) {
  char p[1200]; FILE *f;
  snprintf(p, sizeof p, "%s/ajustes.txt", dir);
  f = fopen(p, "w"); assert(f); fputs(linha, f); fclose(f);
}
static void apaga_marca(const char *dir) {
  char p[1200]; snprintf(p, sizeof p, "%s/ajustesescala-203.txt", dir); unlink(p);
}
int main(void) {
  char dir[1024];
  snprintf(dir, sizeof dir, "%s/nuvio-escala-XXXXXX", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  assert(valorPadrao[AJ_TAMANHO_AJUSTES] == 0);                 // fabrica: 80%
  escreve(dir, "tamanhoAjustesLocal 1\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 0);                       // 90% antigo -> 80%
  valor[AJ_TAMANHO_AJUSTES] = 1; assert(gravar());               // escolheu 90% depois
  ajustes_dir(dir); assert(valor[AJ_TAMANHO_AJUSTES] == 1);      // a marca segura
  apaga_marca(dir); escreve(dir, "tamanhoAjustesLocal 2\n"); ajustes_dir(dir);
  assert(valor[AJ_TAMANHO_AJUSTES] == 2);                       // 100% escolhido fica
  printf("ajustes_escala_migra: ok\n");
  return 0;
}
