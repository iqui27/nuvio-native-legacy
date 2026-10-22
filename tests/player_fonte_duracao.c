// Player comum real, pipeline falso: nenhuma janela, TV ou rede.
#include "player.h"
#include "catalogo.h"
#include "dados.h"
#include "linguas.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
// Permite rodar contra a base anterior: o primeiro assert prova o aviso ausente.
__attribute__((weak)) int player_pediu_proxima_fonte(void) { return 0; }

static double dur, pos;
static int ativo;
int video_tocar(const char *url) { (void)url; ativo = 1; pos = 0; return 1; }
void video_parar(void) { ativo = 0; }
void video_pausar(int p) { (void)p; }
void video_buscar(double p) { pos = p; }
int video_pausa_confirmada(void) { return 0; }
int video_tocando(void) { return ativo; }
int video_pronto(void) { return ativo; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
double video_pos(void) { return pos; }
double video_duracao(void) { return dur; }
const char *video_url_atual(void) { return "https://example.invalid/test.mp4"; }

static void quadro(void) { player_atualizar(.016f, SDL_GetTicks()); }
static int aviso(void) {
  const char *t = player_shot_toast_texto(SDL_GetTicks());
  return strstr(t, "outra obra") != NULL || strstr(t, "different title") != NULL;
}
static void abrir(int esperado, int real, int automatico, int filme) {
  CatItem c = {0}; CatEp ep = {0};
  // Sem IMDb: fixture local nao pede metadados nem intro pela rede.
  strcpy(c.titulo, "Fixture"); strcpy(c.tipo, filme ? "movie" : "series");
  if (filme && esperado) snprintf(c.meta, sizeof c.meta, "%d min", esperado);
  cat_definir(&c, 1); ep.temporada = ep.episodio = 1;
  if (esperado) snprintf(ep.duracao, sizeof ep.duracao, "%d min", esperado);
  cat_definir_episodios(0, &ep, 1);
  player_abrir(0, NULL); player_definir_episodio(filme ? 0 : 1, filme ? 0 : 1);
  player_definir_tentativa(automatico, automatico ? 3 : 0);
  dur = real * 60; player_definir_fonte(video_url_atual()); quadro();
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0); dados_iniciar(""); ling_local_legenda("none");
  abrir(24, 64, 1, 0); assert(aviso());
  assert(player_pediu_proxima_fonte() == 2); assert(!player_pediu_proxima_fonte());
  quadro(); assert(!player_pediu_proxima_fonte());
  // Nova fonte do mesmo episodio: avisa, mas o salto automatico ja foi gasto.
  player_definir_fonte("https://example.invalid/next.mp4"); quadro();
  assert(aviso()); assert(!player_pediu_proxima_fonte());
  abrir(24, 64, 0, 0); assert(aviso()); assert(!player_pediu_proxima_fonte());
  SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = SDLK_RETURN;
  player_evento(&e); assert(player_pediu_proxima_fonte() == 1);
  abrir(24, 26, 1, 0); assert(!aviso()); assert(!player_pediu_proxima_fonte());
  abrir(120, 180, 1, 1); assert(!aviso()); assert(!player_pediu_proxima_fonte());
  abrir(0, 64, 1, 0); assert(!aviso()); assert(!player_pediu_proxima_fonte());
  abrir(24, 48, 1, 0); assert(!aviso()); // razao EXATA 2 nao dispara
  abrir(24, 12, 1, 0); assert(!aviso()); // razao EXATA .5 nao dispara
  abrir(12, 2, 1, 0); assert(!aviso()); // diferenca EXATA 10 min nao dispara
  abrir(24, 8, 1, 0); assert(aviso()); assert(player_pediu_proxima_fonte() == 2);
  abrir(24, 0, 1, 0); assert(!aviso());
  dur = 64 * 60; player_atualizar(.016f, SDL_GetTicks() + 11000);
  assert(aviso()); assert(!player_pediu_proxima_fonte()); // tardio so avisa
  abrir(0, 64, 1, 0);
  CatEp corrente = {.temporada=1,.episodio=1}; strcpy(corrente.duracao, "24 min");
  cat_definir_episodios(0, &corrente, 1); quadro(); assert(aviso());
  assert(player_pediu_proxima_fonte() == 2);
  abrir(0, 64, 1, 1);
  CatItem filme = {0}; strcpy(filme.titulo, "Fixture"); strcpy(filme.tipo, "movie");
  strcpy(filme.meta, "24 min"); cat_definir(&filme, 1); quadro();
  assert(aviso()); assert(player_pediu_proxima_fonte() == 2);
  // Metadados chegam depois da duracao real; vizinhos concordantes sao fallback.
  abrir(0, 64, 1, 0);
  CatEp viz[2] = {{.temporada=1,.episodio=0}, {.temporada=1,.episodio=2}};
  strcpy(viz[0].duracao, "24 min"); strcpy(viz[1].duracao, "25 min");
  cat_definir_episodios(0, viz, 2); quadro(); assert(aviso());
  assert(player_pediu_proxima_fonte() == 2);
  abrir(0, 64, 1, 0); strcpy(viz[1].duracao, "60 min");
  cat_definir_episodios(0, viz, 2); quadro(); assert(!aviso());
  puts("PASS: fonte/duracao, manual, automatico uma vez, limiares, metadados tardios e vizinhos");
  return 0;
}
