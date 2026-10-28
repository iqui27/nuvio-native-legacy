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

// Mirror the production player callers: a consumed seek skips local seeking;
// a true preload predicate pauses playback directly, bypassing motor adapters.
static void isolationMatrix(void) {
  enum Path { PLAY, PAUSE, SEEK, NEXT, TITLE, PRELOAD, SCHEDULED, READY,
    REMOTE_PLAY, REMOTE_PAUSE, REMOTE_SEEK, REMOTE_TITLE, REMOTE_LOBBY,
    RATE, FRAME, SOURCE, OPEN, CLOSE };
  const struct { const char *name; enum Path path; } cases[] = {
    {"local play", PLAY}, {"local pause", PAUSE}, {"local seek", SEEK},
    {"local next", NEXT}, {"local title", TITLE}, {"preload", PRELOAD},
    {"scheduled preload", SCHEDULED}, {"ready", READY},
    {"remote play", REMOTE_PLAY}, {"remote pause", REMOTE_PAUSE},
    {"remote seek", REMOTE_SEEK}, {"remote title", REMOTE_TITLE},
    {"remote lobby", REMOTE_LOBBY}, {"sync rate", RATE},
    {"frame/source reporting", FRAME}, {"source selection", SOURCE},
    {"pending open", OPEN}, {"room close", CLOSE}
  };
  int failures = 0;
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    zerar();
    iniciarMotor();
    fase = JF_DENTRO;
    motor.conectado = 1;
    motor.enviar = send;
    snapshot(&motor, 1, 1, "pausado", now);
    snprintf(motor.sala.ep, sizeof motor.sala.ep, "tt1");
    abrirPedido = escolhendo = 0;
    assert(!juntos_ao_voltar());
    ws = (DiscordWs *)&socketPending;
    playerOpen = prepared = playing = 1;
    position = 42;
    syncRate = 1050;
    personalSpeed = 125;
    fonteEscolhida = fonteVista = -1;
    fonteSolicitada = 0;
    snprintf(motor.stApp, sizeof motor.stApp, "procurando");
    sent[0] = 0;
    int before = lobby, sentBefore = socketSends, ok = 1;
    double expectedPosition = 42;
    switch (cases[i].path) {
      case PLAY: ok = !juntos_ao_alternar(0); break;
      case PAUSE: ok = !juntos_ao_alternar(1); break;
      case SEEK:
        if (!juntos_ao_buscar(77)) player_juntos_buscar(77);
        expectedPosition = 77;
        break;
      case NEXT: ok = !juntos_ao_proximo("tt2"); break;
      case TITLE: juntos_escolher_titulo("tt2", 0, 0, "Other film", ""); break;
      case SCHEDULED:
        motor.playArmado = 1;
        motor.sala.rel.estado = JSE_TOCANDO;
        motor.sala.rel.t0 = SDL_GetTicks64() + 60000;
        /* fall through */
      case PRELOAD:
        if (cases[i].path == PRELOAD) motor.sala.rel.estado = JSE_LOBBY;
        if (juntos_precarregando()) player_juntos_pausar(1);
        break;
      case READY: ok = !motor.p.pronto(motor.p.u); break;
      case REMOTE_PLAY: snapshot(&motor, 1, 2, "tocando", now); break;
      case REMOTE_PAUSE: snapshot(&motor, 1, 2, "pausado", now); break;
      case REMOTE_SEEK: motor.p.buscar(motor.p.u, 999); break;
      case REMOTE_TITLE: remoteTitle(2, "tt2", "New title"); break;
      case REMOTE_LOBBY:
        snapshot(&motor, 1, 2, "lobby", now);
        juntos_passo((Uint32)now);
        if (juntos_precarregando() && video_tocando()) player_juntos_pausar(1);
        break;
      case RATE: motor.p.velocidade(motor.p.u, 100); break;
      case FRAME: juntos_passo((Uint32)now); break;
      case SOURCE:
        juntos_fonte_escolhida();
        escolhendo = 1; // Late addon result from the abandoned player.
        ok = !juntos_escolhendo_fonte() && !juntos_aguarda_fonte();
        ok &= juntos_escolher_fonte(&testStream, 1, 20000, 0) == -1;
        escolhendo = 0;
        break;
      case OPEN: {
        char id[24], title[160], poster[512]; int t, e;
        pedirAbrir("tt2", "Late title", "");
        ok = !juntos_pediu_abrir(id, sizeof id, &t, &e, title, sizeof title, poster, sizeof poster);
        break;
      }
      case CLOSE: zerar(); break;
    }
    ok &= position == expectedPosition && playing && syncRate == 1050 && personalSpeed == 125;
    ok &= lobby == before && socketSends == sentBefore && !strstr(sent, "\"t\":\"cmd\"");
    ok &= !abrirPedido && !escolhendo && !fonteSolicitada && fonteVista == -1;
    if (temMotor) ok &= !strcmp(motor.stApp, "procurando");
    if (!ok) { fprintf(stderr, "FAIL isolation: %s\n", cases[i].name); failures++; }
  }
  assert(failures == 0);
  zerar();
  personalSpeed = 100;
}

static int erroFonte, pedFontes, saindo, pediuSair;
static int ehCanal(void) { return 0; }
void player_voltar_a_esperar(void) { erroFonte = 0; prepared = 0; }
static void androidBack(void) {
#define NV_ANDROID
#include "player_back.h"
#undef NV_ANDROID
}

static void sourceRecoveryMatrix(void) {
  const struct { const char *name; int error, exits; } cases[] = {
    {"source error -> Sources -> replacement", 1, 0},
    {"Back past Sources exits session playback", 0, 1}
  };
  for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
    zerar(); iniciarMotor(); fase = JF_DENTRO;
    snapshot(&motor, 1, 1, "pausado", now);
    abrirPedido = escolhendo = 0;
    playerOpen = prepared = playing = 1;
    erroFonte = cases[i].error;
    pedFontes = saindo = pediuSair = 0;
    androidBack();
    assert(saindo == cases[i].exits && pediuSair == cases[i].exits);
    assert(pedFontes == !cases[i].exits);
    prepared = 1; // A working replacement source is selected in Sources.
    if (juntos_no_player() == cases[i].exits) {
      fprintf(stderr, "FAIL recovery: %s\n", cases[i].name);
      assert(0);
    }
    assert(pPronto(NULL) == !cases[i].exits);
    position = 42;
    motor.p.buscar(motor.p.u, 77);
    motor.p.pausar(motor.p.u, 1);
    assert(position == (cases[i].exits ? 42 : 77));
    assert(playing == cases[i].exits);
    assert(juntos_ativo());
  }
  zerar();
}

int main(void) {
  sourceRecoveryMatrix();
  isolationMatrix();
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

  // An unrelated channel is open while room membership remains retained.
  playerOpen = prepared = playing = 1;
  position = 42;
  syncRate = 1050;
  int pausesBefore = pauses, startsBefore = starts;
  remoteTitle(4, "tt3:2:4", "Latest episode");
  assert(position == 42);
  assert(playing && pauses == pausesBefore && starts == startsBefore);
  assert(!pPronto(NULL));
  // All playback mutations must be inert outside, including rate resets.
  motor.p.pausar(motor.p.u, 1);
  motor.p.pausar(motor.p.u, 0);
  motor.p.buscar(motor.p.u, 999);
  motor.p.velocidade(motor.p.u, 100);
  assert(position == 42 && playing && syncRate == 1050);
  assert(pauses == pausesBefore && starts == startsBefore);
  assert(!abrirPedido && !escolhendo && lobby == before);

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
  assert(!playing && pauses > pausesBefore);
  motor.p.velocidade(motor.p.u, 100);
  assert(syncRate == 1000);
  remoteTitle(5, "tt3:2:5", "Next episode");
  assert(abrirPedido && lobby == before + 1);
  zerar();
  assert(!juntos_ativo());
  puts("PASS: Back retains snapshots without touching unrelated playback; Rejoin restores room playback controls and latest episode");
  return 0;
}
