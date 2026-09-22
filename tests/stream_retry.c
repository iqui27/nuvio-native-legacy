// Regressao do fallback automatico: uma fonte que travou nao pode ser
// escolhida de novo enquanto ainda ha candidatas melhores na mesma lista.
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void fonte(Stream *s, const char *url, int altura, int mp4) {
  memset(s, 0, sizeof *s);
  snprintf(s->url, sizeof s->url, "%s", url);
  snprintf(s->rotulo, sizeof s->rotulo, "Fonte %d", altura);
  s->altura = altura;
  s->mp4 = mp4;
}

int main(void) {
  Stream lista[3];
  fonte(&lista[0], "https://fonte.invalid/4k.mp4", 2160, 1);
  fonte(&lista[1], "https://fonte.invalid/1080.mp4", 1080, 1);
  fonte(&lista[2], "https://fonte.invalid/720.mp4", 720, 1);
  stream_definir_lista(lista, 3);
  assert(stream_automatico() == 0);
  assert(stream_automatico_excluir(0) == 1);
  assert(stream_automatico() == 1);
  assert(stream_automatico_excluir(1) == 1);
  assert(stream_automatico() == 2);
  assert(stream_automatico_excluir(2) == 1);
  assert(stream_automatico() == -1);
  stream_definir_lista(lista, 3);
  assert(stream_automatico() == 0);
  puts("stream_retry: fonte falha e a proxima candidata assume");
  return 0;
}
