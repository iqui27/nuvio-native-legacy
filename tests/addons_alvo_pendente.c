// #413: changing title while a partial search runs must retire its sources.
// Real addons/streams; the in-flight worker is controlled, no network or TV.
#define ondever_pedir nv_test_ondever_pedir
#include "../src/addons.c"
#undef ondever_pedir
#include <assert.h>
void nv_test_ondever_pedir(const char *id, int serie, long tmdb) {
  (void)id; (void)serie; (void)tmdb;
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  Stream old = {0};
  strcpy(old.url, "https://example.invalid/old.mp4");
  strcpy(old.provedor, "Fixture");
  nAddon = 1; fioVivo = 1; progLigado = 1;
  addons_capturar_escopo(&fioEscopo);   // the in-flight worker owns the current session scope
  strcpy(alvoId, "tt90000413"); strcpy(alvoTipo, "movie");
  stream_definir_alvo(alvoId); stream_definir_lista(&old, 1);
  progPub[0] = (Chegada){0, malloc(sizeof old), 1};
  assert(progPub[0].a); *progPub[0].a = old; progPubN = 1;
  progPublicou = 1;

  // A partial list is still present when the user asks for Film B.
  stream_definir_alvo("tt90000414");
  addons_buscar("tt90000414", "movie");
  assert(!strcmp(pendId, "tt90000414"));
  assert(stream_n() == 0 && stream_atual() == -1);
  assert(!stream_lista_do_alvo("tt90000413"));
  // Further old responses and repeated Play cannot restore Film A.
  progMarcar(0, &old, 1, 2); addons_drenar();
  addons_buscar("tt90000414", "movie");
  assert(stream_n() == 0);

  // Same-target recovery still republishes sources already received.
  pendId[0] = 0; stream_definir_alvo(alvoId);
  addons_buscar(alvoId, "movie");
  assert(stream_n() == 1 && stream_lista_do_alvo(alvoId));
  assert(!strcmp(stream_item(0)->url, old.url));

  // The personal-server path also queues behind an addon worker.
  stream_definir_alvo("jf:fixture:item");
  addons_buscar("jf:fixture:item", "movie");
  assert(stream_n() == 0);
  progLimpar(); stream_definir_lista(NULL, 0); fioVivo = 0;
  SDL_Quit();
  puts("PASS: pending title retires old sources; same-title recovery keeps them");
  return 0;
}
