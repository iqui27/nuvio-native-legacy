// #179: /scrobble/start ao tocar, pause ao pausar, stop >= 80% ao sair (como o
// NuvioTV oficial) — sem
// spam e com o corpo certo para filme e episodio.
#include "traktscrobble.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  ScrobbleEstado s = { "", 0 };
  char c[400];
  const char *ep = "tt0944947:5:3";

  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 10) == SCR_NADA);  // sem start
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, ep, 1) == SCR_START);
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, ep, 2) == SCR_NADA);  // sem spam
  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 30) == SCR_PAUSE);
  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 30) == SCR_NADA);  // ja pausado
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, ep, 30) == SCR_START); // retomou
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, "tt0944947:5:4", 0) == SCR_START); // outro ep
  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 50) == SCR_NADA);   // id velho
  // Regras do proprio Trakt (registros 10162-10172, HTTP 422): pause com menos
  // de 1% ("Progress should be at least 1.0% to pause") e pause a partir de 80%
  // ("Progress is 93.1%. Use stop to scrobble") sao recusados. Nao mandar.
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 50.0) == SCR_PAUSE);
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 79.9) == SCR_PAUSE);
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 0.5) == SCR_NADA);
  // Paridade com o oficial: a saida a partir de 80% e o stop de conclusao.
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 80.0) == SCR_STOP);
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 89.9) == SCR_STOP);
  assert(scrobble_decidir(&s, SCR_EV_SAIU, "tt0944947:5:4", 90.0) == SCR_STOP);
  // Visto LOCAL: posicao >= 90% (WatchProgress.isCompleted), ou o fim de Legacy.
  assert(!scrobble_concluiu(1300, 1449, 0));   // 89,7%
  assert(scrobble_concluiu(1305, 1449, 0));    // 90,1%
  assert(scrobble_concluiu(1364.5, 1449.451, 0));  // o 94% do relato (Code Geass S1E3)
  assert(scrobble_concluiu(600, 1449, 1));     // marcador de creditos de Legacy
  assert(!scrobble_concluiu(5, 0, 0));
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, ep, 0.2) == SCR_START);   // start aceita < 1%
  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 0.2) == SCR_NADA);
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, ep, 0.3) == SCR_START);   // a pausa pulada tambem encerra o "tocando"
  assert(scrobble_decidir(&s, SCR_EV_PAUSOU, ep, 93.1) == SCR_NADA);
  assert(scrobble_decidir(&s, SCR_EV_TOCANDO, "", 0) == SCR_NADA);

  scrobble_corpo(c, sizeof c, ep, 42.456);
  assert(!strcmp(c, "{\"show\":{\"ids\":{\"imdb\":\"tt0944947\"}},"
                    "\"episode\":{\"season\":5,\"number\":3},\"progress\":42.46}"));
  { ScrobbleEpIds ids = { "10600123", "5234501", "9923456" };
    scrobble_corpo_ep(c, sizeof c, "tt29422052:2:1", 10.0, &ids);
    assert(!strcmp(c, "{\"episode\":{\"ids\":{\"tvdb\":10600123,\"trakt\":9923456}},\"progress\":10.00}")); }
  scrobble_corpo(c, sizeof c, "tt1375666", 120.0);
  assert(!strcmp(c, "{\"movie\":{\"ids\":{\"imdb\":\"tt1375666\"}},\"progress\":100.00}"));
  assert(!strcmp(scrobble_nome(SCR_START), "start"));

  puts("trakt scrobble: PASS");
  return 0;
}
