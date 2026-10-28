// Remote room updates must not undo Back; Rejoin uses the latest snapshot.
#define main fluxo_main
#include "juntos_fluxo.c"
#undef main

static void remoteTitle(int seq, const char *ep, const char *title) {
  char json[1024];
  snprintf(json, sizeof json,
    "{\"t\":\"sala\",\"seq\":%d,\"rel\":{\"ep\":\"%s\","
    "\"estado\":\"pausado\",\"pos0\":321,\"t0\":10000},\"cfg\":{\"titulo\":\"%s\"},"
    "\"membros\":[{\"id\":\"guest1\",\"nome\":\"Guest\",\"on\":true}]}", seq, ep, title);
  jsm_receber(&motor, json, now);
  juntos_passo((Uint32)now);
}

int main(void) {
  iniciarMotor();
  fase = JF_DENTRO;
  snprintf(motor.eu, sizeof motor.eu, "guest1");
  remoteTitle(1, "tt1", "First film");
  abrirPedido = escolhendo = 0;
  int before = lobby;
  assert(!juntos_ao_voltar());
  playerOpen = prepared = playing = 0;
  remoteTitle(2, "tt2", "Second film");
  assert(juntos_ativo() && motor.temSala && motor.sala.n == 1);
  assert(!strcmp(tituloSala, "Second film"));
  assert(!abrirPedido && !escolhendo && lobby == before);
  remoteTitle(3, "tt3:2:4", "Latest episode");
  assert(!abrirPedido && !escolhendo && lobby == before);
  assert(!strcmp(tituloSala, "Latest episode"));
  assert(juntos_reentrar());
  char imdb[24], title[160], poster[512]; int season, episode;
  assert(juntos_pediu_abrir(imdb, sizeof imdb, &season, &episode,
                          title, sizeof title, poster, sizeof poster));
  assert(!strcmp(imdb, "tt3") && season == 2 && episode == 4);
  assert(!strcmp(title, "Latest episode"));
  playerOpen = prepared = 1;
  position = 0;
  juntos_passo((Uint32)++now);
  assert(position == 321);
  remoteTitle(4, "tt3:2:5", "Next episode");
  assert(abrirPedido && lobby == before + 1);
  zerar();
  assert(!juntos_ativo());
  puts("PASS: Back retains membership; remote titles/episodes stay outside playback; explicit Rejoin opens latest episode at room position; in-player updates still open");
  return 0;
}
