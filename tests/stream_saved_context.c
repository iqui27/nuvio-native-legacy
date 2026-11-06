// Saved-link resumes keep their real metadata without creating a sheet row.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/streams.c"

int main(void) {
  Stream saved = {0}, normal = {0};
  snprintf(saved.url, sizeof saved.url, "https://example.invalid/source?token=one");
  snprintf(saved.cabecalhos, sizeof saved.cabecalhos, "Authorization: saved");
  snprintf(saved.rotulo, sizeof saved.rotulo, "2160p HEVC 80 Mbps");
  saved.altura = 2160; saved.fileIdx = -1;
  normal = saved;
  snprintf(normal.url, sizeof normal.url, "https://example.invalid/normal");
  setenv("NUVIO_SAVED_SOURCE_CONTEXT", "1", 1);
  stream_definir_lista(NULL, 0);
  stream_contexto_guardado(&saved);
  const Stream *actual = stream_fonte_reproducao(saved.url, saved.cabecalhos);
  assert(actual && actual != &saved && actual->altura == 2160);
  assert(stream_atual() == -1 && stream_kbps(actual) == 80000);
  saved.altura = 720; // snapshot owns its metadata, not caller stack memory
  assert(actual->altura == 2160);
  assert(!stream_fonte_reproducao("https://example.invalid/source?token=two", saved.cabecalhos));
  assert(!stream_fonte_reproducao(saved.url, "Authorization: different"));
  assert(!stream_fonte_reproducao(saved.url, NULL));
  setenv("NUVIO_SAVED_SOURCE_CONTEXT", "0", 1);
  assert(!stream_fonte_reproducao(saved.url, saved.cabecalhos));
  unsetenv("NUVIO_SAVED_SOURCE_CONTEXT"); // default enabled
  assert(stream_fonte_reproducao(saved.url, saved.cabecalhos));
  stream_definir_lista(&normal, 1); // background discovery must not erase it
  assert(stream_fonte_reproducao(saved.url, saved.cabecalhos));
  stream_definir_atual(0); // explicit source switch clears the saved context
  assert(stream_fonte_reproducao(normal.url, normal.cabecalhos) == stream_item(0));
  setenv("NUVIO_SAVED_SOURCE_CONTEXT", "0", 1);
  assert(stream_fonte_reproducao(normal.url, normal.cabecalhos) == stream_item(0));
  stream_definir_atual(-1);
  unsetenv("NUVIO_SAVED_SOURCE_CONTEXT");
  assert(!stream_fonte_reproducao(saved.url, saved.cabecalhos));
  stream_contexto_guardado(&saved);
  stream_contexto_guardado(NULL); // VOD/profile teardown
  assert(!stream_fonte_reproducao(saved.url, saved.cabecalhos));
  stream_definir_lista(NULL, 0);
  puts("ok: saved source copy, URL/headers, flag, discovery and source teardown");
  return 0;
}
