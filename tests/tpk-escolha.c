// TRACK SELECTION ON THE .tpk: the deferred write, exercised with no TV.
//
// WHY THIS TEST EXISTS. The first track assignment after opening a file is
// recorded by the player and never applied (measured on the TV: the write at
// 0.00 s came back as "already on 2" while the demuxer kept track 0). The
// fix holds every choice until playback has really started, and lets the
// subtitle wait out the player's settle window. This pins the split: audio
// out on the first tick, the subtitle out after the settle, and a change
// during playback going out immediately.
//
// The subtitle window counts from the first FRAME, not from EV_TOCANDO (which
// arrives while the buffer is still filling), so the fake host has to be able to
// report a position - a fixed 0 could never satisfy it.
#include <stdio.h>
#include <string.h>
#include "video.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <unistd.h>

const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { (void)c; (void)p; return 0; }
const char *ling_audio(void) { return ""; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }


// Stubs for the dependencies #206 added to video_tpk.c (MKV header probe);
// not the subject of this test.
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }

void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);
void video_escolher_audio(int i);
void video_escolher_legenda(int i);
void video_bombear(void);
void nv_tpk_video_legenda(const char *, int);
static int cueWorker(void *unused) { (void)unused;nv_tpk_video_legenda("wrong pending cue",3000);return 0; }

static void hAbrir(const char *u, const char *c) { (void)u; (void)c; }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
// The position is the proof of a frame (LEG_QUADRO_S in video_tpk.c).
static int fakePosMs;
static int  hPos(void) { return fakePosMs; }

// The fake host counts what actually reaches it.
static int nAudios, nLegs, ultAudio, ultLeg;
static int nVels, ultVel;   // #202: escolher(3, centesimos)
static void hEscolher(int tipo, int idx) {
  if (tipo == 0) { nAudios++; ultAudio = idx; }
  else if (tipo == 1) { nLegs++; ultLeg = idx;nv_tpk_video_legenda("old synchronous cue",3000); }
  else if (tipo == 3) { nVels++; ultVel = idx; }
}

// #269: the diagnostics are log lines; stdout goes to a file while they run.
static int saidaFd = -1;
static char saidaCam[512];
static void capturar(void) {
  const char *d = getenv("TMPDIR");
  snprintf(saidaCam, sizeof saidaCam, "%s/nuvio-tpk-escolha-log.txt", d && *d ? d : "/tmp");
  fflush(stdout); saidaFd = dup(1);
  if (!freopen(saidaCam, "w", stdout)) saidaFd = -1;
}
static char *soltar(void) {
  static char buf[16384]; FILE *f; size_t n;
  fflush(stdout);
  if (saidaFd >= 0) { dup2(saidaFd, 1); close(saidaFd); saidaFd = -1; }
  buf[0] = 0;
  if ((f = fopen(saidaCam, "r"))) { n = fread(buf, 1, sizeof buf - 1, f); buf[n] = 0; fclose(f); }
  remove(saidaCam);
  return buf;
}
static void bombearPor(int ms) { int t; for (t = 0; t < ms; t += 50) { video_bombear(); SDL_Delay(50); } }

static int falhas;
static void ok(const char *nome, int cond) {
  printf("%s %s\n", cond ? "ok  " : "FALHA", nome);
  if (!cond) falhas++;
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hInt, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tocar("http://x/filme.mkv");
  nv_tpk_video_evento(6, 1920, 1080);
  nv_tpk_video_evento(1, 100000, 0);
  // 1 audio (ko) and 2 subtitles (ru, en); the file opens on its own default.
  nv_tpk_video_faixa(0, 0, "ko");
  nv_tpk_video_faixa(1, 0, "ru");
  nv_tpk_video_faixa(1, 1, "en");
  nv_tpk_video_faixas_fim(0, 0);

  // --- 1. the app picks before playback starts ------------------------------
  video_escolher_audio(0);
  video_escolher_legenda(1);
  video_bombear();
  ok("choices before playing stay in the app", nAudios == 0 && nLegs == 0);

  // --- 2. audio leaves on the first tick ------------------------------------
  nv_tpk_video_evento(2, 0, 0);          // playback starts (EV_TOCANDO)
  video_bombear();
  ok("audio leaves on the first tick", nAudios == 1 && ultAudio == 0);
  // EV_TOCANDO alone is not a frame.
  ok("the subtitle does not leave on EV_TOCANDO alone", nLegs == 0);
  // The position advancing is the frame.
  fakePosMs = 300;   // 0.30 s > LEG_QUADRO_S (0.25)
  video_bombear();
  ok("the subtitle waits for the settle", nLegs == 0);

  char cue[1024];
  SDL_Thread *worker=SDL_CreateThread(cueWorker,"cue",NULL);SDL_WaitThread(worker,NULL);
  ok("host cues stay hidden while subtitle is pending", !video_legenda_nativa(cue,sizeof cue));

  // A newer immediate choice must cancel the older pending write before pump.
  SDL_Delay(2600);
  video_escolher_legenda(0);
  ok("latest immediate choice reaches host",nLegs==1 && ultLeg==0);
  video_bombear();
  ok("older pending subtitle never overwrites new choice",nLegs==1 && ultLeg==0);
  ok("cached and synchronous old cues are cleared at dispatch",!video_legenda_nativa(cue,sizeof cue));
  nv_tpk_video_legenda("current cue",3000);
  ok("current dispatched cue can render",video_legenda_nativa(cue,sizeof cue)&&!strcmp(cue,"current cue"));
  video_escolher_legenda(-1);
  nv_tpk_video_legenda("off cue",3000);
  ok("off clears and suppresses native cues",!video_legenda_nativa(cue,sizeof cue));
  video_escolher_audio(0);ok("playback audio changes immediately",nAudios==2);

  // New session resets settle state. Superseded deferred choice uses latest.
  // A new session means a new buffer, so the position starts at 0 again.
  fakePosMs = 0;
  video_tocar("http://x/new.mkv");nv_tpk_video_evento(1,100000,0);
  nv_tpk_video_faixa(1,0,"ru");nv_tpk_video_faixa(1,1,"en");nv_tpk_video_faixas_fim(0,0);
  video_escolher_legenda(0);video_escolher_legenda(1);
  int legAntes = nLegs;
  nv_tpk_video_evento(2,0,0);video_bombear();
  // No sleep here, and that is the test: an inherited frame would make the window
  // already expired and the write would go out on this first pump. Sleeping would
  // measure the same thing but depend on SDL_Delay not overshooting.
  ok("new session waits for its own frame",nLegs==legAntes);
  fakePosMs = 300;   // now the first frame of THIS session
  video_bombear();
  ok("new session does not reuse previous settle time",nLegs==legAntes);
  SDL_Delay(2600);video_bombear();
  ok("latest deferred subtitle leaves after settle",nLegs==legAntes+1&&ultLeg==1);
  ok("cue emitted during deferred host dispatch is hidden",!video_legenda_nativa(cue,sizeof cue));
  nv_tpk_video_legenda("new session cue",3000);
  ok("new session cue renders after dispatch",video_legenda_nativa(cue,sizeof cue));

  video_tocar("http://x/stopped.mkv");nv_tpk_video_evento(1,100000,0);
  nv_tpk_video_faixa(1,0,"ru");nv_tpk_video_faixas_fim(0,0);
  video_escolher_legenda(0);video_parar();nv_tpk_video_legenda("stopped cue",3000);video_bombear();
  ok("stop cancels deferred choices and cached cues",nLegs==2&&!video_legenda_nativa(cue,sizeof cue));
  video_tocar("http://x/reset.mkv");nv_tpk_video_evento(1,100000,0);
  nv_tpk_video_faixa(1,0,"ru");nv_tpk_video_faixas_fim(0,0);
  video_escolher_legenda(0);video_escolher_legenda(-1);nv_tpk_video_evento(2,0,0);video_bombear();
  SDL_Delay(2600);video_bombear();
  ok("off also cancels a deferred choice in new session",nLegs==2&&!video_legenda_nativa(cue,sizeof cue));

  // #202 SPEED: goes to the host as escolher(3, cents) only with the player
  // prepared, once per value, again on a new host Player (it starts at 1x);
  // the host's refusal (EV 8, b = 0) hides the row and pins 1x.
  video_tocar("http://x/rapido.mkv");
  ok("speed row offered with a host", video_velocidade_suportada());
  video_velocidade(150); video_bombear();
  ok("speed waits for the prepare", nVels == 0);
  nv_tpk_video_evento(1, 100000, 0); video_bombear(); video_bombear();
  ok("speed sent once after the prepare", nVels == 1 && ultVel == 150);
  nv_tpk_video_evento(8, 150, 1);
  ok("accepted speed stays", video_velocidade_atual() == 150 && video_velocidade_suportada());
  video_tocar("http://x/outra-fonte.mkv"); nv_tpk_video_evento(1, 100000, 0); video_bombear();
  ok("new host Player gets the speed again", nVels == 2 && ultVel == 150);
  nv_tpk_video_evento(8, 150, 0);
  ok("refusal hides the row and pins 1x", !video_velocidade_suportada() && video_velocidade_atual() == 100);
  video_velocidade(200); video_bombear();
  ok("nothing more is sent after a refusal", nVels == 2);
  ok("tpk never blocks for passthrough", !video_velocidade_bloqueada());

  // #269 MKV PROBE STATE: the subtitle sheet waits for 1 before routing a text
  // track to the app overlay. It used to be a constant 2 ("not MKV") here.
  video_definir_mp4(0);
  video_tocar("http://x/sonda.mkv");
  ok("probe pending on a fresh MKV source", video_mkv_sondado() == 0);
  video_definir_mp4(1);
  ok("MP4 source has no probe", video_mkv_sondado() == 2);
  video_definir_mp4(0);

  // #269 NATIVE CUE DIAGNOSTICS (LEG_SEM_CUE_MS shortened by the .sh).
  fakePosMs = 0;
  video_tocar("http://x/diag.mkv"); nv_tpk_video_evento(1, 100000, 0);
  nv_tpk_video_faixa(1, 0, "en"); nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(2, 0, 0); nv_tpk_video_evento(7, 100, 0);
  fakePosMs = 300; video_bombear();
  SDL_Delay(2600); video_bombear();
  { int legAntes = nLegs; char *log;
    video_escolher_legenda(0);
    ok("diag: playback choice reaches host", nLegs == legAntes + 1);
    capturar();
    bombearPor(700);
    nv_tpk_video_evento(3, 0, 0);          // paused: does not count as silence
    bombearPor(1500);
    log = soltar();
    ok("diag: paused time is not silence", !strstr(log, "no subtitle cue"));
    capturar();
    nv_tpk_video_evento(2, 0, 0);
    bombearPor(1500);
    log = soltar();
    ok("diag: silent track is reported once with track and codec",
       strstr(log, "[video] tpk: no subtitle cue in") && strstr(log, "track=0") && strstr(log, "codec=unknown") &&
       !strstr(strstr(log, "no subtitle cue") + 1, "no subtitle cue"));
    capturar();
    video_escolher_legenda(0);              // new write: a new proof window
    nv_tpk_video_legenda("hello", 2000);
    bombearPor(1500);
    log = soltar();
    ok("diag: first cue after the write is logged with its size",
       strstr(log, "[video] tpk: first subtitle cue after") && strstr(log, "5 chars") && strstr(log, "track=0"));
    ok("diag: no silence report once a cue arrived", !strstr(log, "no subtitle cue")); }

  printf(falhas ? "tpk-escolha: %d falha(s)\n" : "tpk-escolha: ok\n", falhas);
  return falhas != 0;
}
