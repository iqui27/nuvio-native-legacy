// LEAVING NUVIO DURING PLAYBACK AND COMING BACK (.tpk), exercised with no TV.
//
// WHY THIS TEST EXISTS. On Home or TV sleep the host disposes the player
// (#412: never a live player in the background). Before this fix nothing told
// the core: the session kept saying "playing", every key went to a player that
// no longer existed and the picture came back black. Now the host reports the
// stop with the exact position (nv_tpk_video_suspenso) and the return
// (nv_tpk_video_voltou). This pins:
//   - the suspended session holds the exact point and sends nothing to the host;
//   - the return reopens the same URL, muted, with an exact seek to the point,
//     pauses, gives the sound back and restores the audio/subtitle choice;
//   - Play pressed during the reopen ends playing, not paused;
//   - a source that fails twice (or never shows a frame) gives up, so the
//     player screen can close instead of showing a black screen;
//   - live channels / trailers (no reconnection allowed) are not touched.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "video.h"
#include <SDL2/SDL.h>
#include <stdlib.h>

int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file;
  if (n) dst[0] = 0;
  return 0;
}
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { (void)c; (void)p; return 0; }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }

void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);
void nv_tpk_video_suspenso(int ms);
void nv_tpk_video_voltou(void);
int video_tpk_fundo(double alvoPlayer);
int video_tpk_retomada_evento(void);
void video_escolher_audio(int i);
void video_escolher_legenda(int i);
void video_bombear(void);

enum { EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_ERRO = 5, EV_BUSCOU = 11 };

// The fake host counts what reaches it.
static int nAbrir, nPausar, ultPausa = -1, nBuscar, nExata, ultExata = -1, nMudo, ultMudo = -1, ultAudio = -1, ultLeg = -1;
static char ultUrl[512], ultCab[2048];
static int fakePosMs;
static void hAbrir(const char *u, const char *c) {
  nAbrir++;
  snprintf(ultUrl, sizeof ultUrl, "%s", u ? u : "");
  snprintf(ultCab, sizeof ultCab, "%s", c ? c : "");
}
static void hSem(void) {}
static void hPausar(int p) { nPausar++; ultPausa = p; }
static void hBuscar(int ms) { nBuscar++; (void)ms; }
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int  hPos(void) { return fakePosMs; }
static void hEscolher(int tipo, int idx) {
  if (tipo == 0) ultAudio = idx;
  else if (tipo == 1) ultLeg = idx;
  else if (tipo == 4) { nExata++; ultExata = idx; }
  else if (tipo == 5) { nMudo++; ultMudo = idx; }
}
static void zerarHost(void) {
  nAbrir = nPausar = nBuscar = nExata = nMudo = 0;
  ultPausa = ultExata = ultMudo = ultAudio = ultLeg = -1;
  ultUrl[0] = ultCab[0] = 0;
}

static int falhas;
static void ok(const char *nome, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FALHA", nome);
  if (!cond) falhas++;
}
static void bombearPor(int ms) { int t; for (t = 0; t < ms; t += 20) { video_bombear(); SDL_Delay(20); } }

// A VOD title playing at `posMs`, 2 audio + 2 subtitle tracks, audio 1 and
// subtitle 0 chosen by the person.
static void tocarFilme(const char *url, int posMs) {
  video_definir_reconexao(1);
  video_tocar(url);
  nv_tpk_video_evento(6, 1920, 1080);
  nv_tpk_video_evento(EV_PRONTO, 5400000, 0);
  nv_tpk_video_faixa(0, 0, "en"); nv_tpk_video_faixa(0, 1, "sv");
  nv_tpk_video_faixa(1, 0, "sv"); nv_tpk_video_faixa(1, 1, "en");
  nv_tpk_video_faixas_fim(0, 1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = posMs;
  bombearPor(100);
  video_escolher_audio(1);
  video_escolher_legenda(0);
  bombearPor(1300);   // the subtitle write waits out the settle window
}

// The new player of the reopen: prepared, tracks listed, first frame.
static void hostReabre(int primeiroQuadroMs) {
  nv_tpk_video_evento(6, 1920, 1080);
  nv_tpk_video_evento(EV_PRONTO, 5400000, 0);
  nv_tpk_video_faixa(0, 0, "en"); nv_tpk_video_faixa(0, 1, "sv");
  nv_tpk_video_faixa(1, 0, "sv"); nv_tpk_video_faixa(1, 1, "en");
  nv_tpk_video_faixas_fim(0, 1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = primeiroQuadroMs;
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);

  // --- 1. Home during playback --------------------------------------------
  tocarFilme("http://x/filme.mp4", 1234000);
  zerarHost();
  nv_tpk_video_suspenso(1234567);           // host: stopped at 1234.567 s
  ok("background: session suspended", video_tpk_fundo(-1.0) == 1);
  ok("background: position is the host's exact stop", fabs(video_pos() - 1234.567) < 0.0005);
  ok("background: not playing", !video_tocando());
  ok("background: duration kept", fabs(video_duracao() - 5400.0) < 0.01);
  video_pausar(0);
  nv_tpk_video_evento(EV_ERRO, -3, 0);      // late dispose report of the old player
  bombearPor(200);
  ok("suspended: nothing sent to the dead player", nPausar == 0 && nAbrir == 0 && nBuscar == 0 && nExata == 0);
  ok("suspended: a late host error does not fail the source", !video_falhou());

  // --- 2. back on screen ----------------------------------------------------
  nv_tpk_video_voltou();
  video_bombear();
  ok("return: player screen told it is reopening", video_tpk_retomada_evento() == 1);
  ok("return: same source reopened", nAbrir == 1 && strstr(ultUrl, "http://x/filme.mp4") != NULL);
  ok("return: reopen starts muted", strstr(ultCab, "X-Nuvio-Resume: 1") != NULL);
  ok("return: bar stays at the exact point while reopening", fabs(video_pos() - 1234.567) < 0.0005);
  hostReabre(40);
  bombearPor(200);
  ok("first frame: exact seek to the point", nExata == 1 && ultExata == 1234567);
  ok("first frame: pause sent", nPausar >= 1 && ultPausa == 1);
  ok("first frame: sound still off", nMudo == 0);
  nv_tpk_video_evento(EV_BUSCOU, 1234567, 0);
  fakePosMs = 1234567;
  nv_tpk_video_evento(EV_PAUSADO, 0, 0);
  bombearPor(3000);   // the subtitle write waits the settle window after the seek
  ok("landed paused: sound given back", nMudo == 1 && ultMudo == 0);
  ok("landed paused: still paused", !video_tocando());
  ok("landed paused: position is the exact point", fabs(video_pos() - 1234.567) < 0.0005);
  ok("landed paused: audio choice restored", ultAudio == 1);
  ok("landed paused: subtitle choice restored", ultLeg == 0);
  ok("landed paused: no give-up", video_tpk_retomada_evento() == 0);

  // --- 3. seek in flight when leaving: the target wins --------------------
  tocarFilme("http://x/voo.mp4", 600000);
  video_buscar(900.0);                       // not confirmed yet
  zerarHost();
  nv_tpk_video_suspenso(600200);
  video_tpk_fundo(-1.0);
  ok("seek in flight: suspended at the seek target", fabs(video_pos() - 900.0) < 0.0005);
  video_parar();

  // --- 4. scrub start from the player screen wins over the clock -----------
  tocarFilme("http://x/scrub.mp4", 300000);
  zerarHost();
  nv_tpk_video_suspenso(300000);
  video_tpk_fundo(250.5);
  ok("scrub: suspended at the player screen's target", fabs(video_pos() - 250.5) < 0.0005);

  // --- 5. Play pressed during the reopen -----------------------------------
  nv_tpk_video_voltou();
  video_bombear();
  (void)video_tpk_retomada_evento();
  video_pausar(0);                          // Play on the paused controls
  hostReabre(40);
  bombearPor(200);
  ok("Play during reopen: exact seek, no pause sent", nExata == 1 && ultExata == 250500 && ultPausa != 1);
  nv_tpk_video_evento(EV_BUSCOU, 250500, 0);
  fakePosMs = 250500;
  bombearPor(200);
  ok("Play during reopen: sound back, playing", nMudo == 1 && ultMudo == 0 && video_tocando());
  video_parar();

  // --- 6. the source fails twice: give up so the screen closes -------------
  tocarFilme("http://x/morto.mp4", 100000);
  nv_tpk_video_suspenso(100000);
  video_tpk_fundo(-1.0);
  nv_tpk_video_voltou();
  video_bombear();
  (void)video_tpk_retomada_evento();
  zerarHost();
  nv_tpk_video_evento(EV_ERRO, 0x1234, 0);
  bombearPor(100);
  ok("one error: one more try", nAbrir == 1 && video_tpk_retomada_evento() == 0);
  nv_tpk_video_evento(EV_ERRO, 0x1234, 0);
  bombearPor(100);
  ok("second error: gives up", video_tpk_retomada_evento() == 2);
  ok("given up: position kept for the save", fabs(video_pos() - 100.0) < 0.0005);
  ok("given up: not reported as a failed source", !video_falhou());
  video_parar();

  // --- 7. no frame at all: give up after the deadline ---------------------
  tocarFilme("http://x/mudo.mp4", 50000);
  nv_tpk_video_suspenso(50000);
  video_tpk_fundo(-1.0);
  nv_tpk_video_voltou();
  video_bombear();
  (void)video_tpk_retomada_evento();
  bombearPor(1800);
  ok("no frame: gives up after the deadline", video_tpk_retomada_evento() == 2);
  video_parar();

  // --- 8. the host's report alone (no background cycle reached the app) -----
  tocarFilme("http://x/reserva.mp4", 70000);
  nv_tpk_video_suspenso(70000);
  bombearPor(700);
  ok("no cycle: suspended by the fallback after 0.5 s", fabs(video_pos() - 70.0) < 0.0005 && !video_tocando());
  video_parar();

  // --- 9. live channel / trailer: not touched -------------------------------
  video_definir_reconexao(0);
  video_tocar("http://x/live.m3u8");
  nv_tpk_video_evento(EV_PRONTO, 0, 0);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  bombearPor(100);
  zerarHost();
  nv_tpk_video_suspenso(5000);
  ok("live: not suspended", video_tpk_fundo(-1.0) == 0);
  nv_tpk_video_voltou();
  bombearPor(100);
  ok("live: nothing reopened", nAbrir == 0 && video_tpk_retomada_evento() == 0);
  video_parar();

  // --- 10. a new title after a suspend starts clean ------------------------
  tocarFilme("http://x/a.mp4", 10000);
  nv_tpk_video_suspenso(10000);
  video_tpk_fundo(-1.0);
  video_parar();                             // person left the player
  tocarFilme("http://x/b.mp4", 20000);
  zerarHost();
  nv_tpk_video_voltou();
  bombearPor(100);
  ok("new title: an old return does not reopen anything", nAbrir == 0 && video_tocando());

  printf("tpk-fundo: %s\n", falhas ? "FALHOU" : "ok");
  return falhas ? 1 : 0;
}
