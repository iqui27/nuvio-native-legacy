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

static int routerPedido, videoPronto, videoFalhou;
static double retomada;
#define sessao_logada teste_logada
#define desc_sessao_aberta teste_logada
#define detail_aberto teste_detail_aberto
#define detail_indice teste_detail_indice
#define detail_ep_foco teste_detail_ep_foco
#define detail_pediu_reproduzir teste_detail_pedido
#define player_abrir teste_player_abrir
#define video_pronto teste_video_pronto
#define video_falhou teste_video_falhou
#define video_duracao teste_video_duracao
#define player_carregando teste_carregando
#define player_definir_retomada_fonte teste_retomada
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

int teste_logada(void) { return 1; }
int teste_detail_aberto(void) { return routerPedido; }
int teste_detail_indice(void) { return 0; }
int teste_detail_ep_foco(int *t, int *e) { *t = 1; *e = 3; return 1; }
int teste_detail_pedido(void) {
  int p = routerPedido; routerPedido = 0;
  if (p) {
    preparoRodando = fioFonteVivo = 1;
    fonteJob.tipo = FJOB_PREPARO_FALHA;
    fonteJob.sessaoPerfil = nv_perfil_sessao_ler();
    fonteJob.preparoRodada = atomic_load(&preparoGeracao);
    fonteJob.listaGeracao = stream_lista_geracao();
    fonteJob.lembrada = -1;
    strcpy(fonteJob.id, "tt0000513:1:3");
    preparoContexto(&fonteJob.contexto);
    atomic_store(&fonteJob.estado, FJOB_RUNNING);
    assert(preparoEmCurso(fonteJob.id, -1));
  }
  return p;
}
void teste_player_abrir(int i, const char *url) {
  (void)i; (void)url; playerAbertoFlag = 1;
  // The router must reach the cached source despite pending preflight.
  assert(preparoEspera);
}
int teste_video_pronto(void) { return videoPronto; }
int teste_video_falhou(void) { return videoFalhou; }
double teste_video_duracao(void) { return 3600.0; }
int teste_carregando(void) { return 0; }
void teste_retomada(double pos) { retomada = pos; }

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
  gravar("escolherFonteManual 1\nfontePrepararLocal 0\n");
  fontevolta_configurar(ajustes_link_reusar(), ajustes_link_prazo_s());
  fontevolta_guardar("tt0000513:1:3", "conta", 1, &g, 0, SDL_GetTicks());
  stream_definir_alvo("tt0000513:1:3");
  stream_definir_lista(&g, 1);
  playerAbertoFlag = 0;
  routerPedido = 1;
  cwTocarT = 1; cwTocarE = 3;
  perfilSessaoAplicada = perfis_sessao_geracao();
  // The request fixture supplies a pending preflight without network work.
  app_atualizar(0, SDL_GetTicks());
  fioFonteVivo = preparoRodando = 0;
  atomic_store(&fonteJob.estado, FJOB_IDLE);
  assert(!preparoEspera);
  assert(chamadasFonte == 1 && !strcmp(urlDoPlayer, g.url));
  assert(SDL_GetTicks() - started < 100u);
  puts("ok failover: local resume opens within 100 ms before network verification");
  // Successful verification must retain recovery for later CDN errors and
  // expired signed URLs. Both must enter normal provider/add-on selection.
  int recovered = 0;
  for (int failure = 0; failure < 2; failure++) {
    videoPronto = 1; videoFalhou = 0;
    vigiarFonteGuardada();
    fonteFalhaPos = 125.0;
    videoFalhou = 1;
    if (failure) videoPronto = 0; // expired URL after the backend closes
    vigiarFonteGuardada();
    int ok = aguardandoFonte == 1 && retomada == 125.0 && !voltaAtiva;
    printf("%s: %s after successful verification\n", ok ? "PASS" : "FAIL",
           failure ? "expired signed URL" : "later CDN failure");
    recovered += ok;
    aguardandoFonte = 0;
    videoFalhou = videoPronto = 0;
    if (!failure) {
      fontevolta_guardar("tt0000513:1:3", "conta", 1, &g, 0, SDL_GetTicks());
      chamadasFonte = 0;
      assert(app_teste_tocar_guardada());
    }
  }
  fflush(stdout);
  assert(recovered == 2);
  puts("ok recovery: later CDN failure and expired URL preserve position and reselect");
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
