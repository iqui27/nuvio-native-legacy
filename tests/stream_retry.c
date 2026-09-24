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

  // FORA DE CACHE NO DEBRID (o "⏳" do AIOStreams, registro 1163): o
  // automatico prefere a cacheada, mesmo de resolucao menor, e a fora de cache
  // continua na fila como ultima opcao.
  fonte(&lista[0], "https://aio.invalid/p/4k", 2160, 1);
  lista[0].foraCache = 1;
  fonte(&lista[1], "https://aio.invalid/p/1080", 1080, 1);
  fonte(&lista[2], "https://aio.invalid/p/720", 720, 1);
  lista[2].foraCache = 1;
  stream_definir_lista(lista, 3);
  assert(stream_automatico() == 1);
  assert(stream_automatico_excluir(1) == 1);
  assert(stream_automatico() == 0);     // sobrou so fora de cache: a de maior resolucao
  puts("stream_retry: automatico prefere a cacheada; fora de cache fica por ultimo");

  // ESCOLHA MANUAL (stream_resolver_escolhida) sem rede: linha com url volta a
  // url na hora; lista trocada no meio devolve -1 e nao grava nada.
  { char url[4096], serv[32]; int pct = 0;
    unsigned g = stream_lista_geracao();
    assert(stream_resolver_escolhida(1, g, url, sizeof url, serv, sizeof serv, &pct) == 1);
    assert(!strcmp(url, "https://aio.invalid/p/1080"));
    stream_definir_lista(lista, 3);
    assert(stream_resolver_escolhida(1, g, url, sizeof url, serv, sizeof serv, &pct) == -1);
    assert(!url[0]);
    assert(stream_resolver_escolhida(7, stream_lista_geracao(), url, sizeof url,
                                     serv, sizeof serv, &pct) == -1);
  }
  puts("stream_retry: escolha manual com url pronta nao vai ao debrid; lista trocada nao grava");
  return 0;
}
