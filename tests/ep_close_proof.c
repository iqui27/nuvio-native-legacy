// Real player close path (player.c fecharSessao via player_encerrar) with the
// real catalog, progress store, watched map and home callback. Only the video
// pipeline and the outbound account/network boundary are fakes, renamed in
// player.c by ep_close_proof.sh. Runs under a sandbox that denies network.
#include "player.h"
#include "catalogo.h"
#include "perfis.h"
#include "progresso.h"
#include "vistoep.h"
#include "linguas.h"
#include "dados.h"
#include "ajustes.h"
#include "home.h"
#include "trakt.h"
#include "watchstate_envio.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int video_quadro_visto(void) { return video_pronto(); }
int extplayer_tentar(const char *url, const char *title, int anime, void (*internal)(const char *), void (*exit_player)(void)) {
  (void)url; (void)title; (void)anime; (void)internal; (void)exit_player; return 0;
}

// --- fake pipeline ----------------------------------------------------------
static int ativo, pronto, tocando, ack, terminou;
static double posicao, duracao = 1500, creditos;
static char fonte[256];
int video_tocar_retomada(const char *url, double pos, double pct) {
  (void)pct; posicao = pos; ativo = tocando = 1; pronto = 0; ack = 0; terminou = 0;
  snprintf(fonte, sizeof fonte, "%s", url); return 1;
}
int video_tocar_posicao(const char *url, double pos) { return video_tocar_retomada(url, pos, 0); }
int video_tocar(const char *url) { return video_tocar_posicao(url, 0); }
int video_retomada_inicial_estado(void) { return ack; }
void video_parar(void) { ativo = pronto = tocando = terminou = 0; fonte[0] = 0; }
void video_pausar(int p) { tocando = !p; }
void video_buscar(double pos) { posicao = pos; }
int video_pausa_confirmada(void) { return ativo && pronto && !tocando; }
int video_tocando(void) { return tocando; }
int video_pronto(void) { return pronto; }
int video_superficie_estavel(void) { return 1; }
int video_ativo(void) { return ativo; }
int video_falhou(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
int video_terminou(void) { return terminou; }
double video_creditos(void) { return creditos; }
double video_pos(void) { return posicao; }
double video_duracao(void) { return duracao; }
const char *video_url_atual(void) { return fonte; }

// --- outbound boundary stubs (renamed in player.c only) ----------------------
static int nTrakt, nTraktMarcar, nSimkl, nVistoEp, nVistoTit, nServ, nWs, nRec, nAtiv, nDiario, nSync;
static VistoPar ultimoPar; static char ultimoVistoId[64]; static int ultimoVisto;
int stub_trakt_scrobble_alvo(int ev, const TraktScrobbleAlvo *a, double p, double d, double s, int f) {
  (void)ev; (void)a; (void)p; (void)d; (void)s; (void)f; nTrakt++; return SCR_NADA;
}
void stub_trakt_marcar(const char *i, double p, double d, double s, int f) { (void)i; (void)p; (void)d; (void)s; (void)f; nTraktMarcar++; }
int stub_trakt_scrobble(int ev, const char *i, double p, double d, double s, int f) {
  (void)ev; (void)i; (void)p; (void)d; (void)s; (void)f; nTrakt++; return SCR_NADA;
}
int stub_simkl_scrobble(int ev, const char *i, double p, double d, double s, int f) {
  (void)ev; (void)i; (void)p; (void)d; (void)s; (void)f; nSimkl++; return SCR_NADA;
}
int stub_visto_episodios(const char *imdb, const char *tipo, const VistoPar *pares, int n, int visto, int dest) {
  (void)tipo; (void)dest; nVistoEp++;
  assert(n == 1); ultimoPar = pares[0]; ultimoVisto = visto;
  snprintf(ultimoVistoId, sizeof ultimoVistoId, "%s", imdb); return 1;
}
int stub_visto_titulo(const char *imdb, const char *tipo, const int *t, int nt, int visto, int dest) {
  (void)imdb; (void)tipo; (void)t; (void)nt; (void)visto; (void)dest; nVistoTit++; return 1;
}
void stub_servidores_reproducao_fim(const char *u, double p, double d) { (void)u; (void)p; (void)d; nServ++; }
void stub_wsenvio_emitir(const WsEvento *e) { (void)e; nWs++; }
void stub_recomenda_atividade_fim(const CatItem *c, int f) { (void)c; (void)f; nRec++; }
void stub_atividade_player_saiu(double p, double d, int f) { (void)p; (void)d; (void)f; nAtiv++; }
void stub_diariofontes_player_saiu(double p, double d, int f) { (void)p; (void)d; (void)f; nDiario++; }
void stub_sync_sujar_progresso(void) { nSync++; }
#ifdef NV_EPCLOSE_MUTATE
// Sensitivity: the close path loses its local writes; the cases must fail.
void mut_cat_salvar_progresso_ep(int i, double p, double d, int t, int e, int c) { (void)i; (void)p; (void)d; (void)t; (void)e; (void)c; }
void mut_vistoep_definir(const char *i, int t, int e, int v) { (void)i; (void)t; (void)e; (void)v; }
#endif

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

static void quadros(int n, double passo) {
  for (int i = 0; i < n; i++) {
    SDL_Delay(20);
    if (tocando && pronto && !terminou) posicao = fmin(duracao, posicao + passo);
    player_atualizar(.02f, SDL_GetTicks());
  }
}
static void abrir(const char *id, int t, int e) {
  CatItem c = {0};
  SDL_Delay(300); // let the Continue Watching worker settle (see player_android_retomada.c)
  snprintf(c.imdb, sizeof c.imdb, "%s", id);
  snprintf(c.titulo, sizeof c.titulo, "Fixture");
  snprintf(c.tipo, sizeof c.tipo, "series");
  c.temporada = t; c.episodio = e;
  snprintf(c.meta, sizeof c.meta, "2026 · 25 min");
  cat_definir(&c, 1);
  player_abrir(0, NULL); player_definir_episodio(t, e);
  player_definir_fonte("https://example.invalid/fixture.mp4");
  pronto = 1; ack = 1;
}
static int ler(int perfil, const char *chave, ProgRegistro *r) {
  int antes = perfis_ativo(), ok;
  perfis_definir_ativo(perfil); prog_invalidar(); // read back from disk
  ok = prog_por_chave(chave, r);
  perfis_definir_ativo(antes);
  return ok;
}
static void fechar(void) {
  Uint32 t0 = SDL_GetTicks();
  player_encerrar();
  CHECK(SDL_GetTicks() - t0 < 2000); // bounded close
  CHECK(!player_aberto());
}
static void zerar(void) { nTrakt = nTraktMarcar = nSimkl = nVistoEp = nVistoTit = nServ = nWs = nRec = nAtiv = nDiario = nSync = 0; }

int main(void) {
  ProgRegistro r;
  const char *home = getenv("HOME"), *dados = getenv("NUVIO_DADOS");
  alarm(90);
  CHECK(home && dados && strstr(home, "nuvio-epclose.") && !strncmp(dados, home, strlen(home)));
  { // fail closed: the sandbox must deny sockets
    int s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(9) };
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(s < 0 || (connect(s, (struct sockaddr *)&a, sizeof a) < 0 && errno == EPERM));
    if (s >= 0) close(s);
  }
  CHECK(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("");
  perfis_definir_ativo(1); ling_local_legenda("none");

  // Unrelated state that must survive every close below.
  perfis_definir_ativo(2);
  CHECK(prog_gravar_local("tt9000001", 1, 2, 300, 1500, 1));
  perfis_definir_ativo(1);
  CHECK(prog_gravar_local("tt9000003", 0, 0, 1000, 6000, 1));   // movie
  CHECK(prog_gravar_local("tt9000004", 2, 1, 400, 1500, 1));    // other series
  vistoep_definir("tt9000004", 1, 1, 1);

  // 1. Incomplete episode: resume kept, not watched, no watched call.
  zerar();
  abrir("tt9000001", 1, 2);
  quadros(60, .05); posicao = 600; quadros(10, .05);
  CHECK(player_aberto());
  fechar();
  CHECK(ler(1, "tt9000001_s1e2", &r) && r.posSeg > 590 && r.posSeg < 620 && fabs(r.durSeg - 1500) < 1);
  CHECK(r.pendente == 1);
  CHECK(vistoep_estado("tt9000001", 1, 2) != 1);
  CHECK(nVistoEp == 0 && nSync == 1 && nServ == 1 && nAtiv == 1 && nDiario == 1);
  CHECK(!strcmp(home_retomar_imdb(), "tt9000001"));             // home callback took it
  CHECK(prog_continuar_de_perfil(1, ajustes_cw_concluido(), &r) && !strcmp(r.chave, "tt9000001_s1e2"));
  puts("ok incomplete episode keeps resume and stays unwatched");

  // 2. Natural EOF of the same episode: completed, watched, out of Continue.
  zerar();
  abrir("tt9000001", 1, 2);
  quadros(40, .05);
  CHECK(posicao > 600);                                         // resumed, not restarted
  posicao = duracao; terminou = 1;
  for (int i = 0; i < 100 && !player_quer_sair(); i++) quadros(1, 0);
  CHECK(player_quer_sair());                                    // player asked to leave at EOF
  fechar();
  CHECK(ler(1, "tt9000001_s1e2", &r) && fabs(r.posSeg - r.durSeg) < .5);
  CHECK(vistoep_estado("tt9000001", 1, 2) == 1);
  CHECK(vistoep_estado("tt9000001", 1, 3) != 1);                // next episode untouched
  CHECK(!ler(1, "tt9000001_s1e3", &r));
  CHECK(nVistoEp == 1 && ultimoVisto == 1 && !strcmp(ultimoVistoId, "tt9000001") &&
        ultimoPar.temporada == 1 && ultimoPar.episodio == 2);
  CHECK(nSync == 1);
  CHECK(!home_retomar_imdb()[0]);                               // finished title leaves the Resume card
  CHECK(!prog_continuar_de_perfil(1, ajustes_cw_concluido(), &r) || strcmp(r.chave, "tt9000001_s1e2"));
  puts("ok EOF marks S1E2 watched, clears its Continue entry, S1E3 unmarked");

  // 3. Leaving at the credits marker (86%, under the 90% rule) after playing.
  zerar();
  CHECK(prog_gravar_local("tt9000002", 1, 5, 1286, 1500, 1));
  creditos = 1290;
  abrir("tt9000002", 1, 5);
  for (int i = 0; i < 600 && posicao < 1292; i++) quadros(1, .05);
  CHECK(posicao >= 1290 && posicao < 1350);
  fechar();
  creditos = 0;
  CHECK(ler(1, "tt9000002_s1e5", &r) && fabs(r.posSeg - r.durSeg) < .5);
  CHECK(vistoep_estado("tt9000002", 1, 5) == 1 && vistoep_estado("tt9000002", 1, 6) != 1);
  CHECK(nVistoEp == 1 && ultimoPar.temporada == 1 && ultimoPar.episodio == 5);
  puts("ok credits threshold completes the episode");

  // Unrelated series, movie and profile untouched.
  CHECK(ler(2, "tt9000001_s1e2", &r) && fabs(r.posSeg - 300) < .01);
  CHECK(ler(1, "tt9000003", &r) && fabs(r.posSeg - 1000) < .01);
  CHECK(ler(1, "tt9000004_s2e1", &r) && fabs(r.posSeg - 400) < .01);
  CHECK(vistoep_estado("tt9000004", 1, 1) == 1 && vistoep_estado("tt9000004", 2, 1) != 1);
  CHECK(nVistoTit == 0);
  printf("stubs: trakt=%d traktMarcar=%d simkl=%d ws=%d recomenda=%d\n", nTrakt, nTraktMarcar, nSimkl, nWs, nRec);
  SDL_Quit();
  puts("ep close proof: PASS (real player_encerrar -> progress, watched, home resume)");
  return 0;
}
