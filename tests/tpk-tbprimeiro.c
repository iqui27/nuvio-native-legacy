// TORBOX FIRST, FASTER (owner 18/11), exercised with no TV: video_tpk.c
// against a fake host, with burn-in possible (Pro + "Burn in ASS with TorBox").
//   - the file's header not read yet at the open: the player is held for it
//     (up to TB_CAB_ESPERA_MS) instead of opening the original first;
//   - the header arrives -> TorBox is asked, and its burned stream is the
//     first thing the host opens, at the saved point (X-Nuvio-Start);
//   - no prefetch, Off remembered for the show, or time up -> the original.
// The fake host/stubs are tpk-fundo.c's.
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
int video_pausa_confirmada(void);
void video_tpk_tela_publicar(int tela);
void video_tpk_log_host(const char *linha);
void video_escolher_audio(int i);
void video_escolher_legenda(int i);
void video_bombear(void);

enum { EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_ERRO = 5, EV_BUSCOU = 11, EV_RETOMADA = 13, EV_INICIO = 14 };

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

// The new player of the reopen as the host now runs it: prepared, tracks,
// seek before Start, Start, pause at the point (if asked), EV_RETOMADA.
static void hostReabre(int pousouMs, int pausado) {
  nv_tpk_video_evento(6, 1920, 1080);
  nv_tpk_video_evento(EV_PRONTO, 5400000, 0);
  nv_tpk_video_faixa(0, 0, "en"); nv_tpk_video_faixa(0, 1, "sv");
  nv_tpk_video_faixa(1, 0, "sv"); nv_tpk_video_faixa(1, 1, "en");
  nv_tpk_video_faixas_fim(0, 1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = pousouMs >= 0 ? pousouMs : 40;
  if (pausado) nv_tpk_video_evento(EV_PAUSADO, 1, 0);
  nv_tpk_video_evento(EV_RETOMADA, pousouMs, pausado);
}
static void sairEVoltar(int hostMs, double alvoTela) {
  nv_tpk_video_suspenso(hostMs);
  video_tpk_fundo(alvoTela);
  nv_tpk_video_voltou();
  video_bombear();
  (void)video_tpk_retomada_evento();
}


#include "tbburn.h"
#define MF_ESPERA_MS 4000u   // video_tpk.c
#ifndef RET_PRAZO_MS
#define RET_PRAZO_MS 20000u
#endif
int video_tpk_queimar_voltou(void);
// ---- burn-in possible; the account answers what the test says --------------
static int fase = 1, memoria, pedidos, resultado = TBB_BUSCANDO;
static unsigned char cabMkv[256]; static long cabMkvN; static int temCab;
int  tbburn_pode(void) { return 1; }
int  ajustes_leg_queimar(void) { return 1; }
int  mkvass_prebusca_fase(void) { return fase; }
int  tbburn_memoria(TbFaixa *m) { (void)m; return memoria; }
int  tbburn_escolher_auto(const TbFaixa *f, int n, int mem, const TbFaixa *m, const char *p) {
  (void)f; (void)m; (void)p; return mem == 1 || n <= 0 ? -1 : 0;
}
void tbburn_pedir(const char *u, int s, int a) { (void)u; (void)s; (void)a; pedidos++; resultado = TBB_BUSCANDO; }
int  tbburn_resultado(char *m, unsigned n, char *mo, unsigned mn) {
  if (m && n) snprintf(m, n, "%s", resultado == TBB_PRONTO ? "https://flux.tb-cdn.io/stream/1/playlist.m3u8" : "");
  if (mo && mn) mo[0] = 0;
  return resultado;
}
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url;
  if (!temCab) return 0;
  if (!buf || !n) return 1;
  *buf = malloc((size_t)cabMkvN); memcpy(*buf, cabMkv, (size_t)cabMkvN); *n = cabMkvN;
  return 1;
}
// An MKV start: EBML header, Tracks = video 1 (AVC) + subtitle 2 (S_TEXT/ASS, eng).
static void montarCabecalho(void) {
  static const unsigned char c[] = {
    0x1A, 0x45, 0xDF, 0xA3, 0x80,                                 // EBML header (empty)
    0x16, 0x54, 0xAE, 0x6B, 0x80 | 52,                            // Tracks
    0xAE, 0x80 | 23,                                              // TrackEntry: video
    0xD7, 0x81, 0x01, 0x83, 0x81, 0x01,
    0x86, 0x80 | 15, 'V','_','M','P','E','G','4','/','I','S','O','/','A','V','C',
    0xAE, 0x80 | 25,                                              // TrackEntry: ASS
    0xD7, 0x81, 0x02, 0x83, 0x81, 0x11,
    0x86, 0x80 | 10, 'S','_','T','E','X','T','/','A','S','S',
    0x22, 0xB5, 0x9C, 0x83, 'e','n','g',
    0xEC, 0x80 | 20, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };   // Void: the probe wants 64 bytes
  memcpy(cabMkv, c, sizeof c); cabMkvN = (long)sizeof c;
}
static void abrir(const char *url, double ponto) {
  zerarHost(); pedidos = 0; resultado = TBB_BUSCANDO;
  video_definir_reconexao(1);
  if (ponto > 0) video_tocar_retomada(url, ponto, 0.0); else video_tocar(url);
}

// A burned stream playing (header known, TorBox ready), started at `ponto`.
static void queimadoTocando(const char *url, double ponto) {
  temCab = 1; fase = 2; memoria = 0;
  abrir(url, ponto);
  resultado = TBB_PRONTO;
  bombearPor(40);
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  if (ponto > 0) nv_tpk_video_evento(14, (int)(ponto * 1000.0), 0);
  nv_tpk_video_faixa(0, 0, "ja");
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = (int)(ponto * 1000.0);
  bombearPor(100);
  (void)video_tpk_queimar_voltou();
  zerarHost(); pedidos = 0;
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_tela_publicar(1);
  montarCabecalho();

  // 1. Header late, prefetch running: held, then TorBox first at the saved point.
  temCab = 0; fase = 1; memoria = 0;
  abrir("http://x/ep1.mkv", 600.0);
  ok("header not read: the player is held (nothing opened)", nAbrir == 0);
  bombearPor(200);
  ok("header not read: still held while the prefetch runs", nAbrir == 0 && pedidos == 0);
  temCab = 1;
  bombearPor(40);
  ok("header arrived: TorBox asked, the original still not opened", pedidos == 1 && nAbrir == 0);
  resultado = TBB_PRONTO;
  bombearPor(40);
  ok("TorBox ready: its burned stream is the first open", nAbrir == 1 && strstr(ultUrl, "tb-cdn.io") != NULL);
  ok("TorBox ready: it opens at the saved point", strstr(ultCab, "X-Nuvio-Start: 600000") != NULL);
  ok("TorBox ready: the app waits for the host's answer", video_retomada_inicial_estado() == 0);
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  nv_tpk_video_evento(14, 600000, 0);   // EV_INICIO
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  bombearPor(40);
  ok("burned stream started at the point by the host", video_retomada_inicial_estado() == 1 && nBuscar == 0 && nExata == 0);

  // 2. Header already there at the open: TorBox first at once (no hold).
  temCab = 1; fase = 2;
  abrir("http://x/ep2.mkv", 0);
  ok("header known: TorBox asked at the open", pedidos == 1 && nAbrir == 0);
  resultado = TBB_PRONTO;
  bombearPor(40);
  ok("header known, no saved point: burned stream from the start", nAbrir == 1 && strstr(ultCab, "X-Nuvio-Start") == NULL);

  // 3. No prefetch for this source: the original opens on the next frame.
  temCab = 0; fase = 0;
  abrir("http://x/ep3.mkv", 300.0);
  ok("no prefetch: held for one frame only", nAbrir == 0);
  bombearPor(40);
  ok("no prefetch: the original opens (at the saved point)", nAbrir == 1 && strstr(ultUrl, "http://x/ep3.mkv") &&
     strstr(ultCab, "X-Nuvio-Start: 300000") != NULL && pedidos == 0);

  // 4. Off remembered for the show: no waiting.
  temCab = 0; fase = 1; memoria = 1;
  abrir("http://x/ep4.mkv", 0);
  bombearPor(40);
  ok("Off remembered: the original opens at once", nAbrir == 1 && strstr(ultUrl, "http://x/ep4.mkv") && pedidos == 0);
  memoria = 0;

  // 5. Prefetch running but slow: the hold ends at TB_CAB_ESPERA_MS.
  temCab = 0; fase = 1;
  abrir("http://x/ep5.mkv", 0);
  bombearPor(TB_CAB_ESPERA_MS - 200);
  ok("slow header: still held before the limit", nAbrir == 0);
  bombearPor(400);
  ok("slow header: the original opens at the limit", nAbrir == 1 && strstr(ultUrl, "http://x/ep5.mkv") && pedidos == 0);
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  nv_tpk_video_faixa(1, 0, "en");
  nv_tpk_video_faixas_fim(-1, -1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  temCab = 1;
  bombearPor(1500);   // sondaMkv looks every 500 ms; then TorBox's switch is asked as before
  ok("slow header: the later switch is still asked once it arrives", pedidos == 1);

  // 6. Prefetch ended without the header: no waiting.
  temCab = 0; fase = 2;
  abrir("http://x/ep6.mkv", 0);
  bombearPor(40);
  ok("prefetch ended without header: the original opens at once", nAbrir == 1 && pedidos == 0);

  // 7. Home during burn-in, back: the burned stream reopens at the point. It
  //    fails -> TorBox is asked once for a fresh stream at the point; that fails
  //    too -> the original file at the point (Nuvio draws), not a closed player.
  { int n0;
    queimadoTocando("http://x/ep7.mkv", 600.0);
    sairEVoltar(700000, -1.0);
    ok("reopen: the burned stream, at the point, paused and muted", nAbrir == 1 && strstr(ultUrl, "tb-cdn.io") &&
       strstr(ultCab, "X-Nuvio-Resume: 700000,1") != NULL);
    nv_tpk_video_evento(EV_ERRO, -5, 0);
    bombearPor(40);
    ok("reopen error: TorBox asked for a fresh stream, nothing closed", pedidos == 1 && video_tpk_retomada_evento() == 0);
    resultado = TBB_PRONTO;
    bombearPor(40);
    ok("fresh stream: reopened at the point, paused and muted", nAbrir == 2 && strstr(ultUrl, "tb-cdn.io") &&
       strstr(ultCab, "X-Nuvio-Resume: 700000,1") != NULL);
    nv_tpk_video_evento(EV_ERRO, -5, 0);
    n0 = nAbrir;
    bombearPor(MF_ESPERA_MS + 600);   // the original may wait for the local fix's probe
    ok("fresh stream fails too: the original file at the point, still paused", nAbrir == n0 + 1 &&
       strstr(ultUrl, "http://x/ep7.mkv") && strstr(ultCab, "X-Nuvio-Resume: 700000,1") != NULL);
    ok("fresh stream fails too: burn-in gave up (toast, ASS back in Main)", video_tpk_queimar_voltou() == 1);
    ok("fresh stream fails too: the player is not closed", video_tpk_retomada_evento() == 0);
    hostReabre(700000, 1);
    bombearPor(100);
    ok("the original resumed at the point, paused", !video_tocando() && fabs(video_pos() - 700.0) < 0.5); }

  // 8. TorBox has no fresh stream: straight to the original.
  queimadoTocando("http://x/ep8.mkv", 0);
  sairEVoltar(300000, -1.0);
  nv_tpk_video_evento(EV_ERRO, -5, 0);
  bombearPor(40);
  resultado = TBB_FALHOU;
  bombearPor(MF_ESPERA_MS + 600);
  ok("TorBox failed: the original file at the point", pedidos == 1 && strstr(ultUrl, "http://x/ep8.mkv") &&
     strstr(ultCab, "X-Nuvio-Resume: 300000,1") != NULL && video_tpk_retomada_evento() == 0);

  // 9. The burned stream shows no picture (no error): the same path after RET_PRAZO_MS.
  queimadoTocando("http://x/ep9.mkv", 0);
  sairEVoltar(400000, -1.0);
  bombearPor(RET_PRAZO_MS + 200);
  ok("no picture: TorBox asked for a fresh stream instead of closing", pedidos == 1 && video_tpk_retomada_evento() == 0);

  printf("tpk-tbprimeiro: %s\n", falhas ? "FALHOU" : "ok");
  return falhas ? 1 : 0;
}
