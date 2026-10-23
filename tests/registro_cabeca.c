// Test the actual native upload read block; no SDL, HTTP, or server changes.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/avisos_corte.inc"

#define AV_REGISTRO_MAX (200 * 1024)
#define AV_REGISTRO_AUTO_MAX (64 * 1024)
static const char *logPath, *prevPath;
#define AV_LOG_ANTERIOR prevPath
static const char *registro_arquivo(void) { return logPath; }

static char *readUpload(int manual, int automatico, int comAnterior, size_t *size) {
  char *texto = NULL;
  size_t nTexto = 0;
#include "upload.inc"
  if (automatico && texto)
    nTexto = cortarCabecaCauda(texto, nTexto, AV_REGISTRO_AUTO_MAX);
  *size = nTexto;
  return texto;
}

int main(int argc, char **argv) {
  const char *header = "Nuvio 2.0.6\n[tv] webOS model=WR9768\n"
                       "[p2p] engine available\n[ajustes] snapshot\n"
                       "[t] 1200 primeiro quadro na tela\n";
  const char *tail = "[error] final diagnostic\n";
  size_t size;
  char *text;
  FILE *f;
  int i;
  assert(argc == 2);
  logPath = argv[1];
  prevPath = argv[1];
  f = fopen(logPath, "wb");
  assert(f);
  fputs(header, f);
  for (i = 0; i < 10000; i++)
    fputs("[swap] FPS=60 [quadro] [gpu-modos] [ponteiro-diag]\n", f);
  fputs(tail, f);
  fclose(f);
  for (i = 0; i < 2; i++) {
    text = readUpload(!i, i, 0, &size);
    assert(text && !strncmp(text, header, strlen(header)));
    assert(!strncmp(text + strlen(header), "\n[...] ", 7));
    assert(strstr(text, " KB omitidos\n"));
    assert(strstr(text, tail));
    assert(size == strlen(text));
    assert(size <= (i ? AV_REGISTRO_AUTO_MAX : AV_REGISTRO_MAX));
    free(text);
  }
  // Startup longer than the cap, with no completion marker yet.
  f = fopen(logPath, "wb");
  assert(f);
  fputs("Nuvio 2.0.6\n", f);
  for (i = 0; i < 10000; i++)
    fputs("[startup] subsystem initialization in progress\n", f);
  fputs(tail, f);
  fclose(f);
  text = readUpload(1, 0, 0, &size);
  assert(text && !strncmp(text, "Nuvio 2.0.6\n", 12));
  assert(strstr(text, "\n[...] ") && strstr(text, tail));
  assert((size_t)(strstr(text, "\n[...] ") - text) <= 16 * 1024);
  assert(size <= AV_REGISTRO_MAX && size == strlen(text));
  free(text);
  f = fopen(logPath, "wb");
  assert(f);
  fputs(header, f);
  fclose(f);
  text = readUpload(1, 0, 0, &size);
  assert(text && !strcmp(text, header) && size == strlen(header));
  free(text);
  { FILE *a = fopen("prev.log", "wb");
    const char *oldTail = "[player] saida: video parar 1 ms\n[queda] sinal fatal: relato gravado\n";
    assert(a);
    for (i = 0; i < 4000; i++) fputs("[swap] old session\n", a);
    fputs(oldTail, a);
    fclose(a);
    prevPath = "prev.log";
    text = readUpload(1, 0, 1, &size);
    assert(text && strstr(text, header) && strstr(text, "--- sessao anterior ---"));
    assert(strstr(text, "[queda] sinal fatal"));
    assert(strstr(text, header) && size == strlen(text));
    free(text);
    prevPath = NULL;
  }
  puts("registro_cabeca: ok");
  return 0;
}
