// Reuse the host video pipeline and playback fixture; no window or network.
#define main visto_trailer_main
#include "player_visto_trailer.c"
#undef main
#include "posplay.h"

int main(void) {
  static const char *const nomes[] = {
    "delayed next episode", "partial list with delayed next episode",
    "failed/empty response", "already finished empty response",
    "loading deadline", "rewind then late last episode",
    "seek in progress then late last episode", "delayed last episode"
  };
  int falhas = 0;
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); ling_local_legenda("none");
  ajustes_shot_valor("proximoAutoLocal", 0);
  assert(ajustes_proximo_auto());

  for (int caso = 0; caso < 8; caso++) {
    CatItem c = {0}; CatEp eps[2] = {{0}};
    int t = 0, e = 0, ok;
    snprintf(c.imdb, sizeof c.imdb, "tt9000038%d", caso);
    strcpy(c.tipo, "series"); c.temporada = c.episodio = 1;
    cat_definir(&c, 1);
    eps[0].temporada = eps[0].episodio = 1;
    eps[1].temporada = 1; eps[1].episodio = 2;
    if (caso == 1) cat_definir_episodios(0, eps, 1);
    carregando = caso != 3;
    agoraMono = 100 + caso * 100;
    dur = 3600;
    player_abrir(0, fonte); player_definir_episodio(1, 1);
    quadro(); quadro();
    pos = dur; terminou = 1;
    quadro();
    if (caso != 3) {
      assert(!player_quer_sair() && player_aberto());
      assert(!posplay_pediu_episodio(&t, &e));
    }

    // Just before the deadline, an outstanding request must still wait.
    if (caso == 4) {
      agoraMono += 7.999; quadro();
      assert(!player_quer_sair() && player_aberto());
      agoraMono += .001;
    } else if (caso == 5) {
      // webOS keeps terminou=1 when video_buscar rewinds.
      video_buscar(100);
    } else if (caso == 6) {
      // The backend can still report EOF while the user chooses a seek target.
      player_shot_buscando(1);
    }

    if (caso != 4) {
      if (caso < 2) cat_definir_episodios(0, eps, 2);
      if (caso >= 5) cat_definir_episodios(0, eps, 1);
      carregando = 0;
    }
    for (int i = 0; i < 600; i++) quadro();
    if (caso < 2) {
      ok = !player_quer_sair() && player_aberto() &&
           posplay_pediu_episodio(&t, &e) && t == 1 && e == 2;
      assert(!posplay_pediu_episodio(NULL, NULL));
    } else if (caso == 5 || caso == 6) {
      ok = !player_quer_sair() && player_aberto();
      // A cancelled EOF wait must not close later at its old deadline either.
      agoraMono += 10; quadro();
      ok = ok && !player_quer_sair() && player_aberto();
    } else {
      ok = player_quer_sair() && !player_aberto();
    }
    printf("%s: %s\n", ok ? "PASS" : "FAIL", nomes[caso]);
    falhas += !ok;
    player_shot_buscando(0);
    player_encerrar();
  }
  SDL_Quit();
  return falhas ? 1 : 0;
}
