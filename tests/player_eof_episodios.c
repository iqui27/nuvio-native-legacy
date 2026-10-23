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
    "seek in progress then late last episode", "delayed last episode",
    "season zero next episode"
  };
  int falhas = 0;
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); ling_local_legenda("none");
  ajustes_shot_valor("proximoAutoLocal", 0);
  assert(ajustes_proximo_auto());

  for (int caso = 0; caso < 9; caso++) {
    CatItem c = {0}; CatEp eps[2] = {{0}};
    int t = 0, e = 0, ok, temp = caso == 8 ? 0 : 1;
    snprintf(c.imdb, sizeof c.imdb, "tt9000038%d", caso);
    strcpy(c.tipo, "series"); c.temporada = temp; c.episodio = 1;
    cat_definir(&c, 1);
    eps[0].temporada = temp; eps[0].episodio = 1;
    eps[1].temporada = temp; eps[1].episodio = 2;
    if (caso == 1) cat_definir_episodios(0, eps, 1);
    carregando = caso != 3;
    agoraMono = 100 + caso * 100;
    dur = 3600;
    player_abrir(0, fonte);
    if (caso == 0) {
      player_definir_episodio(2, 0); // preencher episodio nao troca uma temporada explicita
      player_episodio_atual(&t, &e); assert(t == 2 && e == 1);
    }
    player_definir_episodio(temp, 1);
    player_episodio_atual(&t, &e); assert(t == temp && e == 1);
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
      if (caso < 2 || caso == 8) cat_definir_episodios(0, eps, 2);
      if (caso >= 5 && caso <= 7) cat_definir_episodios(0, eps, 1);
      carregando = 0;
    }
    for (int i = 0; i < 600; i++) quadro();
    if (caso < 2 || caso == 8) {
      ok = !player_quer_sair() && player_aberto() &&
           posplay_pediu_episodio(&t, &e) && t == temp && e == 2;
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
  {
    CatItem c = {0}; CatEp eps[2] = {{0}};
    SDL_Event ev = {0};
    Uint32 ultimoPedido = 0;
    strcpy(c.imdb, "tt12409194"); strcpy(c.tipo, "series"); c.episodio = 1;
    eps[0].episodio = 1; eps[1].episodio = 2;
    cat_definir(&c, 1); cat_definir_episodios(0, eps, 2); carregando = 0; dur = 3600;
    for (int anterior = 0; anterior < 2; anterior++) {
      int t = -1, e = -1;
      // Pedidos independentes: reabrir conserva o debounce de 400 ms.
      // O relogio SDL e real; a contagem nao usa o clock simulado dos quadros.
      if (anterior) while ((Uint32)(SDL_GetTicks() - ultimoPedido) < 401u) SDL_Delay(1);
      posplay_fechar(); player_abrir(0, fonte); player_definir_episodio(0, anterior ? 2 : 1);
      player_shot_estado(SDL_GetTicks(), 30, 3600, 1, 0, 0, 0); // foco Play
      ev.type = SDL_KEYDOWN; ev.key.keysym.sym = anterior ? SDLK_LEFT : SDLK_RIGHT;
      player_evento(&ev); ev.type = SDL_KEYUP; player_evento(&ev);
      ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_RETURN; player_evento(&ev);
      ultimoPedido = SDL_GetTicks();
      // Series have no Previous button: Left+OK never requests an episode.
      if (anterior) { assert(!player_transporte_pedido_pendente() && !player_pediu_proximo(NULL, NULL)); ev.type = SDL_KEYUP; player_evento(&ev); player_encerrar(); continue; }
      assert(player_transporte_pedido_pendente());
      assert(player_pediu_proximo(&t, &e) && t == 0 && e == 2);
      assert(!player_transporte_pedido_pendente() && !player_pediu_proximo(NULL, NULL));
      ev.type = SDL_KEYUP; player_evento(&ev);
      player_encerrar();
    }
    puts("PASS season zero remote Next delivers once, no Previous");
  }
  SDL_Quit();
  return falhas ? 1 : 0;
}
