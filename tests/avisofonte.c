// Movie alerts exercise the real agenda persistence and transition code.
#define stream_filme_consultar fakeMovieProbe
#include "../src/agenda.c"
#undef stream_filme_consultar
#include "streams.h"
#include <assert.h>
#include <unistd.h>

static atomic_int chamadas, liberado;
static pthread_t ui;
int fakeMovieProbe(const AddonsConsulta *c, const char *id, int (*cancelado)(void *), void *ctx) {
  assert(c && id[0] && !pthread_equal(ui, pthread_self()));
  atomic_fetch_add(&chamadas, 1);
  while (!atomic_load(&liberado) && !cancelado(ctx)) usleep(1000);
  return cancelado(ctx) ? -1 : 1;
}
static void esperar(atomic_int *v) {
  for (int i = 0; i < 2000 && !atomic_load(v); i++) usleep(1000);
  assert(atomic_load(v));
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  dados_iniciar(".");
  assert(dir && !strcmp(dir, dados_dir()));
  agenda_iniciar();
  agenda_definir_hoje("2026-10-19");
  assert(agenda_filme_elegivel("2027-07-17", "", -1));
  assert(agenda_filme_elegivel("", "", -1));
  assert(agenda_filme_elegivel("2026-07-17", "2026-11-01", 2));
  assert(agenda_filme_elegivel("2025-01-01", "", 0));
  assert(!agenda_filme_elegivel("2025-01-01", "", 2));
  assert(!agenda_filme_elegivel("2025-01-01", "", -1));
  assert(agenda_filme_alternar("tt33768016", "The Odyssey", "poster.jpg"));
  assert(agenda_lembrete("tt33768016"));
  AgLembrete *l = &lembretes[0];
  assert(filmeVenceu(l, 100000));
  l->ultima = 100000;
  assert(!filmeVenceu(l, 100001));
  assert(!filmeVenceu(l, 121599));
  assert(filmeVenceu(l, 121600));
  assert(!filmeResultado(l, 0));
  assert(!filmeResultado(l, -1));
  assert(filmeResultado(l, 1));
  assert(!filmeResultado(l, 2)); // Once, regardless of later source count.
  assert(!filmeVenceu(l, 200000));
  gravarLembretes();
  nLembretes = 0; perfilCarregado = -1;
  agenda_iniciar();
  assert(nLembretes == 1 && lembretes[0].disponivel);
  assert(lembretes[0].ultima == 100000);
  assert(!strcmp(lembretes[0].titulo, "The Odyssey"));
  AgItem it;
  assert(agenda_filme_devido(&it));
  assert(!strcmp(it.imdb, "tt33768016"));
  agenda_marcar_avisado(it.imdb);
  assert(!agenda_lembrete(it.imdb));
  assert(!agenda_filme_devido(&it));
  // Old four-column series records keep their meaning.
  dados_gravar(arquivo("lembretes"), "tt10986410\t2026-10-19\t0\t2026-10-18\n");
  perfilCarregado = -1; agenda_iniciar();
  assert(nLembretes == 1 && !lembretes[0].filme);
  assert(agenda_lembrete("tt10986410"));
  assert(!agenda_filme_devido(&it));
  Stream s = {0};
  snprintf(s.url, sizeof s.url, "https://example.org/movie.mp4");
  assert(stream_fonte_disponivel(&s));
  s.naoVideo = 1; assert(!stream_fonte_disponivel(&s));
  s.naoVideo = 0; s.foraCache = 1; assert(!stream_fonte_disponivel(&s));
  s.foraCache = 0;
  snprintf(s.url, sizeof s.url, "https://example.org/slate.mp4");
  assert(!stream_fonte_disponivel(&s));
  stream_definir_alvo("tt33768016");
  stream_definir_lista(NULL, 0);
  assert(stream_filme_fontes("tt33768016") == 0);
  assert(stream_filme_fontes("tt00000000") == -1);
  // Real UI scheduler and snapshot; fake only the network boundary.
  ui = pthread_self();
  AddonRemoto addonTeste = {0};
  snprintf(addonTeste.nome, sizeof addonTeste.nome, "Offline fixture");
  snprintf(addonTeste.url, sizeof addonTeste.url, "https://fixture.invalid/manifest.json");
  addonTeste.ativo = 1;
  assert(addons_definir_lista(&addonTeste, 1));
  assert(agenda_filme_alternar("tt33768016", "The Odyssey", "poster.jpg"));
  agenda_filmes_atualizar(); esperar(&chamadas);
  assert(filmesVivo && !atomic_load(&filmesBusca.pronto));
  for (int i = 0; i < 20; i++) agenda_filmes_atualizar();
  assert(atomic_load(&chamadas) == 1);
  atomic_store(&liberado, 1); esperar(&filmesBusca.pronto);
  agenda_filmes_atualizar();
  assert(agenda_filme_devido(&it));
  agenda_filme_aberto(it.imdb);
  assert(!agenda_filme_devido(&it) && !agenda_lembrete(it.imdb));
  assert(agenda_filme_alternar("tt33768016", "The Odyssey", "poster.jpg"));
  // Unavailable response stays armed; restart does not bypass six hours.
  for (int i = 0; i < nLembretes; i++) if (lembretes[i].filme) lembretes[i].ultima = time(NULL);
  gravarLembretes(); perfilCarregado = -1; agenda_iniciar();
  agenda_filmes_atualizar(); assert(!filmesVivo && atomic_load(&chamadas) == 1);
  // A canceled alert rearmed while a request runs must not inherit its result.
  for (int i = 0; i < nLembretes; i++) if (lembretes[i].filme) lembretes[i].ultima = 0;
  atomic_store(&liberado, 0); agenda_filmes_atualizar();
  for (int i = 0; i < 2000 && atomic_load(&chamadas) < 2; i++) usleep(1000);
  assert(atomic_load(&chamadas) == 2);
  assert(!agenda_filme_alternar("tt33768016", "The Odyssey", "poster.jpg"));
  assert(agenda_filme_alternar("tt33768016", "The Odyssey", "poster.jpg"));
  atomic_store(&liberado, 1); esperar(&filmesBusca.pronto);
  // Prevent a replacement probe while inspecting the discarded result.
  addons_esquecer(); agenda_filmes_atualizar();
  assert(!agenda_filme_devido(&it));
  perfis_definir_ativo(2); agenda_iniciar();
  assert(!agenda_lembrete("tt33768016"));
  perfis_definir_ativo(1); agenda_iniciar();
  assert(agenda_lembrete("tt33768016"));
  puts("PASS: eligibility, persistence, transition, throttle, worker, cancel/rearm, profile isolation");
  return 0;
}
