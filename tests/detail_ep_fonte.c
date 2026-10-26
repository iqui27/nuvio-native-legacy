// ISSUE #513: short OK on an episode, with "Escolher a fonte ao reproduzir"
// on, must not start the saved source. The sheet flag is set in detail.c.
// The router still called tocarFonteGuardada() and handed the live
// fontevolta URL to the player; the sheet only drew on top.
//
// This compiles src/app.c and calls that function (app_teste_tocar_guardada
// is tocarFonteGuardada). player_definir_fonte is the only override: the
// assertion is the URL the real function gives the player.
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char urlDoPlayer[128];
static int chamadasFonte, playerAbertoFlag, playerIndiceFlag, epTFlag, epEFlag;

#define fontevolta_conferir teste_conferir
#define player_definir_fonte teste_definir_fonte
#define player_aberto teste_player_aberto
#define player_indice teste_player_indice
#define player_episodio_atual teste_player_episodio_atual
#define sessao_usuario teste_sessao_usuario
#define perfis_ativo teste_perfis_ativo
#include "../src/app.c"
#undef fontevolta_conferir
#undef player_definir_fonte
#undef player_aberto
#undef player_indice
#undef player_episodio_atual
#undef sessao_usuario
#undef perfis_ativo

void teste_conferir(const char *url, const char *cab) {
  (void)url; (void)cab;
  // Even scheduling network verification must follow the local player open.
  assert(chamadasFonte == 1);
}

void teste_definir_fonte(const char *u) {
  chamadasFonte++;
  snprintf(urlDoPlayer, sizeof urlDoPlayer, "%s", u ? u : "");
}
int teste_player_aberto(void) { return playerAbertoFlag; }
int teste_player_indice(void) { return playerIndiceFlag; }
void teste_player_episodio_atual(int *t, int *e) { *t = epTFlag; *e = epEFlag; }
const char *teste_sessao_usuario(void) { return "conta"; }
int teste_perfis_ativo(void) { return 1; }

static char dir[NVT_MAX], caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(txt, f);
  fclose(f);
  ajustes_dir(dir);
}

int main(void) {
  CatItem c;
  Stream g;

  nvt_set(dir, sizeof dir, "/nuvio-detail-epfonte-XXXXXX");
  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  ajustes_iniciar();

  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "series");
  snprintf(c.imdb, sizeof c.imdb, "tt0000513");
  c.progresso = 40; c.temporada = 1; c.episodio = 3;
  cat_definir(&c, 1);

  // Same key alvoPlayer builds for T1E3 of this title, live (< 3h):
  // lembrarFonte (player.c) stores cat_id_stream = "tt:T:E" for an IMDb
  // series with T/E set — never the bare imdb. Zero first:
  // fontevolta_guardar stores nothing when url[0] == 0, and a dirty stack
  // byte there used to make the probe pass on junk.
  memset(&g, 0, sizeof g);
  snprintf(g.url, sizeof g.url, "https://saved.example/tt0000513-t1e3.mp4");
  fontevolta_guardar("tt0000513:1:3", "conta", 1, &g, 0, SDL_GetTicks());

  // Player open on the episode that left the saved source: that is the
  // state when the sheet path used to start the URL.
  playerAbertoFlag = 1; playerIndiceFlag = 0; epTFlag = 1; epEFlag = 3;

  // Gate ON: the saved URL must not reach the player, AND the lookup must
  // be what refused it. If the key/episode wiring were wrong, the first
  // assert below (the direct lookup) already fails — so a red here means
  // the guard, not an empty slot.
  gravar("escolherFonteManual 0\n");
  assert(ajustes_fonte_manual());
  { Stream probe;
    // Non-destructive: wrong-target lookups do not consume the entry.
    assert(fontevolta_pegar("tt0000513:1:3", "conta", 1, SDL_GetTicks(), &probe));
    assert(!strcmp(probe.url, g.url));
  }
  urlDoPlayer[0] = 0; chamadasFonte = 0;
  assert(!app_teste_tocar_guardada());
  assert(chamadasFonte == 0 && !urlDoPlayer[0]);
  puts("ok manual: saved source does not start");

  // V_LIGA 1 = Desligado. Resume, including Continue Watching, still reuses it.
  gravar("escolherFonteManual 1\n");
  assert(!ajustes_fonte_manual());
  urlDoPlayer[0] = 0; chamadasFonte = 0;
  assert(app_teste_tocar_guardada());
  assert(chamadasFonte == 1 && !strcmp(urlDoPlayer, g.url));
  puts("ok auto: resume still plays the saved source");

  fontefalha_dir(dir);
  assert(fontefalha_definir_provedores(1));
  assert(fontefalha_definir_addons(1));
  assert(fontefalha_ativa());
  urlDoPlayer[0] = 0; chamadasFonte = 0;
  Uint32 started = SDL_GetTicks();
  preparoEspera = 1;
  assert(app_teste_tocar_guardada());
  assert(!preparoEspera);
  assert(chamadasFonte == 1 && !strcmp(urlDoPlayer, g.url));
  assert(SDL_GetTicks() - started < 100u);
  puts("ok failover: local resume opens within 100 ms before network verification");
  gravar("escolherFonteManual 0\n");
  chamadasFonte = 0;
  assert(!app_teste_tocar_guardada() && chamadasFonte == 0);
  gravar("escolherFonteManual 1\n");

  // Wrong target in the player: another episode's saved source does not
  // leak into this one.
  fontevolta_guardar("tt0000513:1:3", "conta", 1, &g, 0, SDL_GetTicks());
  epTFlag = 1; epEFlag = 4;
  urlDoPlayer[0] = 0; chamadasFonte = 0;
  assert(!app_teste_tocar_guardada());
  assert(chamadasFonte == 0 && !urlDoPlayer[0]);
  puts("ok auto: saved source of another episode does not play");

  puts("detail_ep_fonte: saved source waits for the sheet when choose-on-play is on");
  return 0;
}
