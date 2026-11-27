// BURN-IN, ORIGINAL FIRST (owner 24/11/2026: back to 16/11), exercised with no
// TV: video_tpk.c against a fake host, with burn-in possible.
//   - the original always opens first (at the saved point when there is one);
//     TorBox is asked once the file's header is known;
//   - TorBox ready -> the burned stream opens at 0, then the seek after ready;
//   - no picture TB_LENTO_MS after the switch -> the "slow to start" notice;
//     OK goes back to the original at the point; the notice goes when it starts;
//   - a burned stream that never played, item in the account -> the original +
//     remove and re-add; not in the account -> the 16/11 path;
//   - Home during burn-in (7f26ac3e9/89c3931ed) as before.
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
__attribute__((unused)) static void tocarFilme(const char *url, int posMs) {
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
int video_tpk_queimar_aviso(void);
int video_tpk_queimar_tardio_ok(void);
int video_tpk_queimar_abrindo(void);
double video_tpk_mf_alvo(int reabrindo, double posMantida, int reconMs, unsigned buscaIdadeMs, double buscaAlvo,
                         double agora, const char **de);
static int itemConta, readicoes;
int  tbburn_item_na_conta(const char *u) { (void)u; return itemConta; }
void tbburn_readicionar(const char *u, int s, int a) { (void)u; (void)s; (void)a; readicoes++; }
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

// The original plays at `ponto` (header known): TorBox gets asked.
static void originalTocando(const char *url, double ponto) {
  temCab = 1; fase = 2; memoria = 0;
  abrir(url, ponto);
  bombearPor(MF_ESPERA_MS + 300);   // the local fix checks an ASS file before the TV opens it
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  if (ponto > 0) nv_tpk_video_evento(14, (int)(ponto * 1000.0), 0);
  nv_tpk_video_faixa(0, 0, "ja");
  nv_tpk_video_faixa(1, 0, "en");
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = (int)(ponto * 1000.0);
  bombearPor(1500);   // sondaMkv looks every 500 ms; then TorBox is asked
}
// The switch: TorBox ready -> the burned stream opens (nothing prepared yet).
static void trocar(void) {
  resultado = TBB_PRONTO;
  bombearPor(40);
}
// The burned stream prepares and plays.
static void queimadoPrepara(void) {
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  nv_tpk_video_faixa(0, 0, "ja");
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  bombearPor(100);
}
// A burned stream playing, switched to at `ponto`.
static void queimadoTocando(const char *url, double ponto) {
  originalTocando(url, ponto);
  trocar();
  fakePosMs = (int)(ponto * 1000.0) + 1000;   // the burned stream shows a frame near the point
  queimadoPrepara();
  bombearPor(200);
  (void)video_tpk_queimar_voltou(); (void)video_tpk_queimar_aviso();
  zerarHost(); pedidos = 0;
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_tpk_tela_publicar(1);
  montarCabecalho();

  // 1. Header known at the open, saved point: the original opens first, at the point.
  temCab = 1; fase = 2; memoria = 0;
  abrir("http://x/ep1.mkv", 600.0);
  ok("original first: nothing burned opens while the local fix checks", nAbrir == 0 || !strstr(ultUrl, "tb-cdn.io"));
  bombearPor(MF_ESPERA_MS + 300);
  ok("original first: the original is the first open", nAbrir == 1 && strstr(ultUrl, "http://x/ep1.mkv") != NULL);
  ok("original first: at the saved point (host seek before Start)", strstr(ultCab, "X-Nuvio-Start: 600000") != NULL);
  ok("original first: TorBox not asked before the open", pedidos == 0);
  zerarHost();
  nv_tpk_video_evento(EV_PRONTO, 1425000, 0);
  nv_tpk_video_evento(14, 600000, 0);
  nv_tpk_video_faixa(0, 0, "ja"); nv_tpk_video_faixa(1, 0, "en");
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(EV_TOCANDO, 0, 0);
  fakePosMs = 600000;
  bombearPor(1500);
  ok("original playing: TorBox asked once, nothing reopened", pedidos == 1 && nAbrir == 0);
  fakePosMs = 612000;
  trocar();
  ok("TorBox ready: the burned stream opens", nAbrir == 1 && strstr(ultUrl, "tb-cdn.io") != NULL);
  ok("TorBox ready: no saved point with it (opens at 0)", strstr(ultCab, "X-Nuvio-Start") == NULL);
  ok("switching: the source watchdog waits (burned stream not playing yet)", video_tpk_queimar_abrindo());
  queimadoPrepara();
  ok("burned stream ready: the app seeks to where the original was", nBuscar >= 1);
  ok("burned stream playing: no slow-start notice", video_tpk_queimar_aviso() == 0 && !video_tpk_queimar_abrindo());

  // 2. No picture after TB_LENTO_MS: the notice; OK -> the original at the point.
  originalTocando("http://x/ep2.mkv", 0);
  fakePosMs = 300000;
  bombearPor(40);
  trocar();
  ok("slow start: burned stream opened", nAbrir == 2 && strstr(ultUrl, "tb-cdn.io") != NULL);
  bombearPor(TB_LENTO_MS - 300);
  ok("slow start: no notice before TB_LENTO_MS", video_tpk_queimar_aviso() == 0);
  bombearPor(500);
  ok("slow start: the notice is up", video_tpk_queimar_aviso() == 3);
  ok("slow start: OK goes back", video_tpk_queimar_tardio_ok() == 1);
  bombearPor(MF_ESPERA_MS + 600);   // the original may wait for the local fix's probe
  ok("slow start, OK: the original reopens", nAbrir == 3 && strstr(ultUrl, "http://x/ep2.mkv") != NULL);
  ok("slow start, OK: burn-in off for this playback (toast)", video_tpk_queimar_voltou() == 1);
  ok("slow start, OK: a second OK is an ordinary OK", video_tpk_queimar_tardio_ok() == 0);

  // 3. The notice goes when the burned stream starts.
  originalTocando("http://x/ep3.mkv", 0);
  trocar();
  bombearPor(TB_LENTO_MS + 200);
  ok("late start: the notice is up", video_tpk_queimar_aviso() == 3);
  queimadoPrepara();
  ok("late start: it started, the notice goes", video_tpk_queimar_aviso() == 4);
  ok("late start: OK no longer goes back", video_tpk_queimar_tardio_ok() == 0);

  // 4. A burned stream that never played, item still in the account: stuck ->
  //    the original + remove and re-add (no same-mode retry).
  itemConta = 77; readicoes = 0;
  originalTocando("http://x/ep4.mkv", 0);
  trocar();
  resultado = TBB_BUSCANDO;   // the re-add's asks: TorBox not ready yet
  { int n0 = nAbrir;
    nv_tpk_video_evento(EV_ERRO, -5, 0);
    bombearPor(MF_ESPERA_MS + 600);
    ok("stuck item: re-added once", readicoes == 1);
    ok("stuck item: the original reopens", nAbrir == n0 + 1 && strstr(ultUrl, "http://x/ep4.mkv") != NULL); }
  itemConta = 0;

  // 5. Not in the account: the 16/11 path (TorBox's own stream fails -> original + toast).
  readicoes = 0;
  originalTocando("http://x/ep5.mkv", 0);
  trocar();
  { int n0 = nAbrir;
    nv_tpk_video_evento(EV_ERRO, -5, 0);
    bombearPor(MF_ESPERA_MS + 600);
    ok("not in the account: no re-add", readicoes == 0);
    ok("not in the account: the original, burn-in off (toast)", nAbrir == n0 + 1 && strstr(ultUrl, "http://x/ep5.mkv") != NULL &&
       video_tpk_queimar_voltou() == 1); }

  // 6. Clock: the burned stream is anchored at 0 at the open, video_pos is the player's.
  queimadoTocando("http://x/ep6.mkv", 0);
  fakePosMs = 15100;
  bombearPor(40);
  ok("burned stream: video_pos is the player's own clock", fabs(video_pos() - 15.1) < 0.3);

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
    bombearPor(MF_ESPERA_MS + 600);
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

  // 10. Log 24 (owner 24/11/2026): the local-fix reopen right after a switch
  //     (the probe found ASS tracks before the switch's seek landed) goes to
  //     the switch's point, not the burned stream's 0.0 s.
  { const char *de = NULL;
    double a = video_tpk_mf_alvo(1, 27.1, 27100, 0xFFFFFFFFu, 0, 0.0, &de);
    ok("local-fix reopen during a switch: at the switch's point", fabs(a - 27.1) < 0.01 && de && strstr(de, "reopen in progress"));
    a = video_tpk_mf_alvo(0, 0, 27100, 0xFFFFFFFFu, 0, 0.3, &de);
    ok("local-fix reopen: a pending seek wins over the clock", fabs(a - 27.1) < 0.01 && de && strstr(de, "pending seek"));
    a = video_tpk_mf_alvo(0, 0, -1, 4000u, 600.0, 0.2, &de);
    ok("local-fix reopen: the seek asked in the last 10 s", fabs(a - 600.0) < 0.01 && de);
    a = video_tpk_mf_alvo(0, 0, -1, 12000u, 600.0, 42.0, &de);
    ok("local-fix reopen: otherwise the TV's clock", fabs(a - 42.0) < 0.01 && !de); }

  printf("tpk-tbqueima: %s\n", falhas ? "FALHOU" : "ok");
  return falhas ? 1 : 0;
}
