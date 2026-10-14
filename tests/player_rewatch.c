#define main fixture_main
#include "player_visto_trailer.c"
#undef main
#include "rewatch.h"

static int aguardandoFonte, escolhido, proximo;
static int pedidoProximo(int *t, int *e) {
  if (!proximo) return 0;
  proximo = 0; *t = 2; *e = 9; return 1;
}
static int pedidoEscolhido(int *t, int *e) {
  if (!escolhido) return 0;
  escolhido = 0; *t = 2; *e = 9; return 1;
}
static void diagnostico_cancelar_vazao(void) {}
static void marco(const char *s) { (void)s; }
static void buscarParaPlayer(void) {}
#define episodios_escolheu pedidoEscolhido
#define player_pediu_proximo pedidoProximo
static void avancar(void) {
/* APP_TRANSITIONS */
}
#undef player_pediu_proximo
#undef episodios_escolheu
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); ling_local_legenda("none");
  for (int modo = 0; modo <= 1; modo++) {
    CatItem c = {0}; CatEp eps[2] = {{0}};
    strcpy(c.imdb, modo ? "tt90000902" : "tt90000901");
    strcpy(c.tipo, "series"); c.temporada = 2; c.episodio = 8;
    eps[0].temporada = eps[1].temporada = 2;
    eps[0].episodio = 8; eps[1].episodio = 9;
    cat_definir(&c, 1); cat_definir_episodios(0, eps, 2);
    rewatch_iniciar(c.imdb, RW_ORIGEM_TITULO);
    for (int automatico = 0; automatico <= 1; automatico++) {
      aguardandoFonte = 0; dur = 600; creditos = 0;
      player_abrir(0, fonte); player_rewatch(modo); player_definir_episodio(2, 8);
      quadro(); quadro(); tocar(590, 1);
      if (automatico) {
        // OSD next-episode request, consumed by the actual app branch.
        proximo = 1;
      } else escolhido = 1;
      avancar(); player_definir_fonte(fonte); quadro(); quadro(); tocar(200, 1);
      player_encerrar();
      int t = 0, e = 0, pct = 0; ProgRegistro r;
      int parcial = rewatch_parcial_de(c.imdb, &t, &e, &pct);
      int normal = prog_por_chave(modo ? "tt90000902_s2e9" : "tt90000901_s2e9", &r);
      assert(modo ? (parcial && t == 2 && e == 9 && pct > 0 && !normal)
                  : (normal && r.posSeg >= 200 && !parcial));
    }
    rewatch_encerrar(c.imdb);
  }
  SDL_Quit(); puts("PASS: episode transitions preserve normal and rewatch progress");
  return 0;
}
