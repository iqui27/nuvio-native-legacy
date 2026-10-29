// Exercise the public trail entry without GL or network. Reuse the existing
// account/profile storage and clock doubles from achievement regressions.
#define main conquistas_regressao_main
#include "conquistas_regressao.c"
#undef main
#include "conquistas.h"
#include "trilhas.h"
#include "visto.h"

static int marcou;
int ajustes_animacoes_reduzidas(void) { return 1; }
int visto_destinos(void) { return 0; }
int visto_titulo(const char *id, const char *tipo, const int *temps, int n, int visto, int destinos) {
  (void)temps; (void)n; (void)destinos;
  assert(id && !strcmp(tipo, "movie")); marcou += visto; return 1;
}
static void tecla_trilha(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  trilhas_evento(&e);
}
int main(void) {
  prog_definir_relogio(relogio);
  cat_historico_contexto(conta, perfil);
  assert(!conquistas_abrir_trilha(NULL));
  assert(!conquistas_abrir_trilha("trilha:missing"));
  // This assertion fails on the original stub, before any subsequent checks.
  assert(conquistas_abrir_trilha("trilha:oscar-melhor-filme"));
  assert(trilhas_aberta() && trilhas_passos() == 98 && trilhas_vistos() == 0 && trilhas_passo() == 0);
  assert(!cqd_trilhas_de("tt0018578", "series", NULL, 0));
  tecla_trilha(SDLK_ESCAPE);
  cqd_concluir_local("tt0018578");
  cat_historico_definir_id("tt0019729", "movie", 1);
  assert(conquistas_abrir_trilha("trilha:oscar-melhor-filme"));
  assert(trilhas_vistos() == 2 && trilhas_passo() == 2);
  tecla_trilha(SDLK_RETURN);
  CqPasso p;
  assert(trilhas_pediu_titulo(&p) && !strcmp(p.imdb, "tt0020629"));
  assert(!trilhas_pediu_titulo(&p));
  trilhas_voltar_do_titulo();
  assert(trilhas_aberta() && trilhas_passo() == 2);
  tecla_trilha(SDLK_DOWN); tecla_trilha(SDLK_RIGHT); tecla_trilha(SDLK_RETURN);
  assert(marcou == 1 && trilhas_vistos() == 3);
  tecla_trilha(SDLK_ESCAPE);
  assert(trilhas_abrir_titulo("tt30144839", "movie"));
  assert(trilhas_passo() == 97 && trilhas_primeiro_visivel() == 90);
  tecla_trilha(SDLK_RETURN);
  assert(trilhas_pediu_titulo(&p) && !strcmp(p.imdb,"tt30144839"));
  trilhas_voltar_do_titulo();
  tecla_trilha(SDLK_UP); tecla_trilha(SDLK_RETURN);
  assert(trilhas_pediu_titulo(&p) && !strcmp(p.imdb,"tt0021746"));
  trilhas_voltar_do_titulo();
  tecla_trilha(SDLK_ESCAPE);
  // International trail uses the same durable completion ledger.
  const CqTrilha *internacional = cqd_trilha(1);
  assert(internacional && internacional->n == 71);
  cqd_concluir_local(internacional->p[0].imdb);
  assert(conquistas_abrir_trilha(internacional->id));
  assert(trilhas_vistos() >= 1 && trilhas_passo() >= 1);
  tecla_trilha(SDLK_ESCAPE);
  perfil = 2; cat_historico_contexto(conta, perfil);
  assert(conquistas_abrir_trilha("trilha:oscar-melhor-filme"));
  assert(trilhas_vistos() == 0 && trilhas_passo() == 0);
  puts("PASS: trail open/resume, durable history, all steps, contextual title and profile isolation");
  return 0;
}
