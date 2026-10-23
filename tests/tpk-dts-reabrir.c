// CONVERTED STREAM REOPENING (.tpk), exercised with no TV.
//
// On the continuous TS the player can't seek: a seek, a reconnect or a switch
// opens a new player on a new stream (?t=). The app must see that as a seek,
// not as a source opening: video_pronto() stays 1 and video_pos() holds the
// target, or the "Opening source" card and the app's opening deadlines treat a
// playing source as dead. The reconnect resumes at the last real position (not
// 0) through a new stream, and nothing ever sends a raw seek to the TS.
//
// Runs src/video_tpk.c against a fake host and a fake converter (continuous
// TS) with the same hand-built MKV header as tpk-dts-legendas.c. Also the
// preferred-language rule: a mixed file converts the DTS/TrueHD track when the
// preferred audio language exists only there.
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"
#include "dts/dts_hls_tpk.h"
#include "audioescolha.h"

// These fixtures never resolve a TorBox file link. Fail closed if requested.
int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  (void)usenet; (void)item; (void)file;
  if (n) dst[0] = 0;
  return 0;
}
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return c && p && !strcmp(c, p); }
static const char *pref = "";
const char *ling_audio(void) { return pref; }
static const char *pref2 = "", *orig = "";
const char *ling_audio2(void) { return pref2; }
const char *ling_original(void) { return orig; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *nome) { (void)nome; return NULL; }
int ling_letreiro(const char *nome, int forcado) { (void)nome; (void)forcado; return 0; }
void mkvass_aceitar_texto(int sim) { (void)sim; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }
void intro_definir_capitulos(const IntroTrecho *v, int n) { (void)v; (void)n; }
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)tamFinal;
  if (tam) *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 1;
  return 0;
}
char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim, long *tam) {
  (void)url; (void)segundos; (void)ini; (void)fim;
  if (tam) *tam = 0;
  return 0;
}

// --- the MKV header the "prefetch" hands over ---------------------------------
static unsigned char cab[1024];
static long cabN;
static int cabPronto;   // mkvass_cabecalho answers only once this is set
static long el(unsigned char *o, unsigned id, int idLen, const unsigned char *d, long n) {
  long p = 0;
  for (int i = idLen - 1; i >= 0; i--) o[p++] = (unsigned char)(id >> (8 * i));
  o[p++] = (unsigned char)(0x40 | (n >> 8)); o[p++] = (unsigned char)n;   // 2-byte size
  memcpy(o + p, d, (size_t)n);
  return p + n;
}
static long uintEl(unsigned char *o, unsigned id, int idLen, unsigned v) {
  unsigned char b = (unsigned char)v;
  return el(o, id, idLen, &b, 1);
}
static long faixa(unsigned char *o, int num, int tipo, const char *codec, const char *lang, int canais) {
  unsigned char d[256];
  long n = 0;
  n += uintEl(d + n, 0xD7, 1, (unsigned)num);
  n += uintEl(d + n, 0x83, 1, (unsigned)tipo);
  n += el(d + n, 0x86, 1, (const unsigned char *)codec, (long)strlen(codec));
  if (lang) n += el(d + n, 0x22B59C, 3, (const unsigned char *)lang, (long)strlen(lang));
  if (canais) { unsigned char a[8]; long k = uintEl(a, 0x9F, 1, (unsigned)canais); n += el(d + n, 0xE1, 1, a, k); }
  return el(o, 0xAE, 1, d, n);
}
static int duplicateDts;
static int misto;   // + a Japanese AAC track the TV plays
static const char *videoCodec = "V_MPEGH/ISO/HEVC";
static void montarCabecalho(void) {
  unsigned char t[768];
  long n = 0;
  n += faixa(t + n, 1, 1, videoCodec, NULL, 0);
  n += faixa(t + n, 2, 2, "A_DTS", duplicateDts ? "jpn" : "eng", 6);
  if (duplicateDts) n += faixa(t + n, 7, 2, "A_DTS", "jpn", 6);
  if (misto) n += faixa(t + n, 6, 2, "A_AAC", "jpn", 2);
  n += faixa(t + n, 3, 17, "S_HDMV/PGS", "eng", 0);
  n += faixa(t + n, 4, 17, "S_TEXT/ASS", "eng", 0);
  n += faixa(t + n, 5, 17, "S_TEXT/UTF8", "por", 0);
  cabN = 0;
  cab[cabN++] = 0x1A; cab[cabN++] = 0x45; cab[cabN++] = 0xDF; cab[cabN++] = 0xA3; cab[cabN++] = 0x80;
  cabN += el(cab + cabN, 0x1654AE6B, 4, t, n);
  { unsigned char z[64] = {0}; cabN += el(cab + cabN, 0xEC, 1, z, sizeof z); }   // the probe wants some tail
}
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url;
  if (!cabPronto) return 0;
  if (buf) { *buf = malloc((size_t)cabN); memcpy(*buf, cab, (size_t)cabN); }
  if (n) *n = cabN;
  return 1;
}

// --- the converter: always there, continuous TS ------------------------------
static int abriuConversor, urlEmN;
static double urlEmUlt = -1, inicioStream = -1;
int proxyts_e_url(const char *u) { (void)u; return 0; }
static int modoFake = DTSTPK_PROG_TS;
int ajustes_dts_tpk_modo(void) { return modoFake; }
int ajustes_dts_ac3(void) { return 1; }
int dts_tpk_disponivel(void) { return 1; }
const char *dts_tpk_modo_nome(int m) { (void)m; return "TS"; }
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n) {
  (void)url; (void)headers; (void)audio_ordinal; (void)stereo_aac; (void)modo;
  abriuConversor++;
  SDL_Delay(3);   // like the TV: starting the local server takes a few ms
  snprintf(out, n, "http://127.0.0.1:1/tok/v.ts?t=%.0f", inicio);
  return 1;
}
int dts_tpk_url_em(double s, char *out, size_t n) {
  urlEmN++; urlEmUlt = s; inicioStream = -1;   // the new stream's keyframe is not known yet
  snprintf(out, n, "http://127.0.0.1:1/tok/v.ts?t=%.0f", s);
  return 1;
}
void dts_tpk_fechar(void) {}
int dts_tpk_estado(void) { return DTSTPK_PRONTO; }
const char *dts_tpk_erro(void) { return ""; }
double dts_tpk_duracao(void) { return 7200; }
int dts_tpk_modo(void) { return modoFake; }
double dts_tpk_inicio_stream(void) { return inicioStream; }
const char *dts_tpk_caminho(void) { return "DTS core only"; }
int dts_tpk_prebuffer_ms(void) { return 1000; }
int dts_tpk_sem_indice(void) { return 0; }
int dts_tpk_e_url(const char *u) { return u && strstr(u, "127.0.0.1") != NULL; }
int video_faixa_dts(const VideoFaixa *f) { return f && !strncmp(f->codec, "A_DTS", 5); }

// --- the host ------------------------------------------------------------------
void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);
void video_tpk_log_host(const char *linha);
int video_tpk_fonte_tocou(void);
int video_tpk_convertendo(void);
int video_tpk_capa(void);
static char ultUrl[256];
static int nAbriu, nBuscas, posMs;
static char ultCab[128];
static void hAbrir(const char *u, const char *c) { nAbriu++; snprintf(ultUrl, sizeof ultUrl, "%s", u); snprintf(ultCab, sizeof ultCab, "%s", c ? c : ""); }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hBuscar(int ms) { (void)ms; nBuscas++; }
static int pausas, retomadas, pausaRecusada;
void nv_tpk_video_evento(int tipo, int a, int b);
// Like Video.cs: a pause that takes reports EV_PAUSADO, a play EV_TOCANDO; a
// pause during a seek is refused ("Failed to pause") and reports nothing.
static void hPausar(int p) {
  if (p) { pausas++; if (!pausaRecusada) nv_tpk_video_evento(3, 0, 0); }
  else { retomadas++; nv_tpk_video_evento(2, 0, 0); }
}
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return posMs; }
static void hEscolher(int tipo, int idx) { (void)tipo; (void)idx; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(5); } }
static const char *tvLang = "eng";   // the language the TV gives its one playable track
static void playerPronto(void) {
  nv_tpk_video_evento(1, 0, 0);
  nv_tpk_video_faixa(0, 0, tvLang);
  nv_tpk_video_faixas_fim(0, -1);
  nv_tpk_video_evento(2, 0, 0);
}
static int perto(double a, double b) { return a > b - 0.01 && a < b + 0.01; }

int main(void) {
  SDL_Init(0);
  montarCabecalho();
  cabPronto = 1;
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_definir_reconexao(1);
  video_tocar("http://h/a.mkv");
  ok(abriuConversor == 1 && strstr(ultUrl, "v.ts?t=0"), "opens the converted TS from the start");
  ok(!video_pronto(), "first open is an open (the app's opening card shows)");
  ok(!video_tpk_capa(), "first open: no cover (the opening card is the screen)");
  inicioStream = 0; posMs = 1000;
  playerPronto(); bombear(3);
  posMs = 60000; bombear(3);
  ok(perto(video_pos(), 60.0) && video_tpk_fonte_tocou() && video_tpk_convertendo(), "plays: position 60 s, the source played");

  // SEEK: a new stream at the target, never a raw seek, and the app sees a seek.
  nAbriu = 0; nBuscas = 0;
  video_buscar(600);
  ok(urlEmN == 1 && perto(urlEmUlt, 600) && nAbriu == 1 && strstr(ultUrl, "t=600"), "seek: the player opens the stream at 600 s");
  bombear(3);
  ok(video_pronto() && perto(video_pos(), 600), "seek: video_pronto stays 1 and the position is the target");
  ok(video_tpk_capa(), "seek: the screen covers the black until the new frame");
  inicioStream = 598; posMs = 598500;
  playerPronto(); bombear(3);
  ok(nBuscas == 0, "seek: no raw seek on the progressive stream");
  ok(perto(video_pos(), 598.5), "seek: released once the new player shows the keyframe before 600 s");
  ok(!video_tpk_capa(), "seek: cover gone with the first frame");

  // RECONNECT: resumes at the last real position through a new stream.
  posMs = 620000; bombear(3);
  nAbriu = 0;
  video_tpk_log_host("erro ConnectionFailed");
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);
  bombear(2);
  ok(video_reconectando() > 0, "reconnect: scheduled");
  { Uint32 t0 = SDL_GetTicks(); while (!nAbriu && SDL_GetTicks() - t0 < 4000) bombear(1); }
  ok(nAbriu == 1 && perto(urlEmUlt, 620) && strstr(ultUrl, "t=620"), "reconnect: reopens the stream at 620 s, not 0");
  ok(video_pronto() && perto(video_pos(), 620), "reconnect: the app keeps the position while reopening");
  inicioStream = 619; posMs = 619200;
  playerPronto(); bombear(3);
  ok(nBuscas == 0 && perto(video_pos(), 619.2), "reconnect: playing again with no raw seek");

  // ERROR RIGHT AFTER A SEEK (the "alvo 0s" case): the target is the seek's.
  posMs = 700000; bombear(40);   // 80 s past the drop: the reconnect is over
  nAbriu = 0; urlEmN = 0;
  video_buscar(900);
  bombear(2);
  video_tpk_log_host("erro ConnectionFailed");
  nv_tpk_video_evento(5, (int)0xFE6C0026u, 0);
  { Uint32 t0 = SDL_GetTicks(); while (nAbriu < 2 && SDL_GetTicks() - t0 < 4000) bombear(1); }
  ok(nAbriu == 2 && perto(urlEmUlt, 900), "error during a seek: reconnect goes to the seek target, not 0");
  ok(video_pronto() && perto(video_pos(), 900), "error during a seek: no opening card, position kept");

  video_parar();
  ok(!video_tpk_convertendo() && !video_tpk_fonte_tocou(), "stop clears the converted state");

  // SEEK BEFORE THE PLAYER IS READY (pending seek) on the TS: it reopens the
  // stream at the target by URL, never a raw seek, and the fresh open is not
  // taken for a stuck player (no stale "preso" warning, no extra reopen).
  video_tocar("http://h/g.mkv");
  bombear(3);
  nAbriu = 0; nBuscas = 0; urlEmN = 0;
  bombear(3);
  ok(nAbriu == 0, "fresh open: no recovery reopen from a stale stuck warning");
  video_buscar(300);
  inicioStream = 0; posMs = 1000;
  playerPronto(); bombear(3);
  ok(nBuscas == 0, "pending seek on the TS: no raw seek");
  ok(urlEmN >= 1 && perto(urlEmUlt, 300) && strstr(ultUrl, "t=300"), "pending seek on the TS: reopens the stream at 300 s");
  video_parar();

  // NO START BUFFER (removed, Round 2): the converted stream never pauses itself.
  misto = 0; montarCabecalho(); cabPronto = 1; pausas = retomadas = 0;
  video_tocar("http://h/f.mkv");
  inicioStream = 0; posMs = 1000;
  playerPronto(); bombear(3);
  ok(pausas == 0, "no start buffer on the TS");
  video_parar();
  modoFake = DTSTPK_HLS;
  video_tocar("http://h/f2.mkv");
  posMs = 1000;
  playerPronto(); bombear(3);
  ok(pausas == 0, "no start buffer on HLS");
  video_parar();
  // The prebuffer reaches the host as a pseudo header with the converted URL.
  video_tocar("http://h/f4.mkv");
  ok(dts_tpk_e_url(ultUrl) && !strcmp(ultCab, "X-Nuvio-Prebuffer: 1000"), "prebuffer: 1000 ms goes to the host with the converted URL");
  video_parar();
  // STOP IS FINAL: after Back (or the error card) nothing reopens the file.
  // The player's track list of the stopped file used to restart the
  // conversion on the next tick, playing on behind the menus (TV, Oct 2026).
  video_tocar("http://h/f3.mkv");
  posMs = 30000;
  playerPronto(); bombear(3);
  ok(video_tpk_convertendo(), "stop: converting before the stop");
  video_parar();
  nAbriu = 0; abriuConversor = 0;
  bombear(20);
  ok(nAbriu == 0 && abriuConversor == 0 && !video_tpk_convertendo(), "stop: nothing reopens after the stop");
  modoFake = DTSTPK_PROG_TS;

  // PREFERRED LANGUAGE ONLY ON DTS: English DTS + Japanese AAC, English wanted.
  misto = 1; montarCabecalho(); abriuConversor = 0;
  pref = "jpn";
  video_tocar("http://h/b.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "mixed file, preferred language playable (jpn): the original plays");
  video_parar();
  pref = "eng";
  video_tocar("http://h/c.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "mixed file, preferred language only on DTS (eng): converts it");
  video_parar();
  // Header only after the original is playing: same rule, mid-film. The TV
  // lists the one track it plays (the Japanese AAC).
  cabPronto = 0; abriuConversor = 0; tvLang = "jpn";
  video_tocar("http://h/e.mkv");
  inicioStream = -1; posMs = 2000;
  playerPronto(); bombear(3);
  cabPronto = 1;
  { Uint32 t0 = SDL_GetTicks(); while (!abriuConversor && SDL_GetTicks() - t0 < 3000) bombear(1); }
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl) && video_pronto(), "header read while the original plays: converts, no opening card");
  video_parar();

  // THE AUDIO ORDER (audioescolha.h): picked by hand > preferred > second > original.
  // Second language only on DTS: converts it before opening.
  abriuConversor = 0; pref = "swe"; pref2 = "eng";
  video_tocar("http://h/o1.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "order: preferred (Swedish) missing, second (English) only on DTS: converts");
  video_parar();
  // No preference, original language only on DTS: converts.
  abriuConversor = 0; pref = ""; pref2 = ""; orig = "eng";
  video_tocar("http://h/o2.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "order: original language (English) only on DTS: converts");
  video_parar();
  // Preferred Japanese plays on the TV: the original, even with English as the original language.
  abriuConversor = 0; pref = "jpn"; orig = "eng";
  video_tocar("http://h/o3.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "order: preferred language playable beats the original language on DTS");
  video_parar();
  // Picked the Japanese AAC by hand in this playback: it wins over the preferred English DTS.
  abriuConversor = 0; pref = "eng"; orig = "";
  audioesc_titulo("tt9:0:0");
  audioesc_lembrar("jpn", "A_AAC", "Japanese", 2);
  video_tocar("http://h/o4.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "order: the track picked by hand earlier (Japanese AAC) beats the preferred English DTS");
  video_parar();
  // And the other way: English DTS picked by hand, preferred Japanese: converts the English.
  pref = "jpn";
  audioesc_lembrar("eng", "A_DTS", "English", 6);
  video_tocar("http://h/o5.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "order: English DTS picked by hand earlier beats the preferred Japanese: converts");
  audioesc_titulo("tt10:0:0");
  pref = "eng"; tvLang = "eng";
  video_parar();
  pref = ""; abriuConversor = 0;
  video_tocar("http://h/d.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "mixed file, no preference: the original plays");
  video_parar();
  // VC-1 / MPEG-2 video: the converted TS/HLS does not play on the TV, so a
  // DTS-only file with that video stays on the original, as on master.
  misto = 0; pref = "";
  for (int k = 0; k < 2; k++) {
    videoCodec = k ? "V_MPEG2" : "V_MS/VFW/FOURCC"; montarCabecalho(); abriuConversor = 0;
    video_tocar("http://h/f.mkv");
    inicioStream = -1; posMs = 2000;
    playerPronto(); bombear(20);
    ok(!abriuConversor && !dts_tpk_e_url(ultUrl), k ? "MPEG-2 video, only DTS: the original plays" : "VC-1 video, only DTS: the original plays");
    video_parar();
  }
  videoCodec = "V_MPEG4/ISO/AVC"; montarCabecalho(); abriuConversor = 0;
  video_tocar("http://h/g.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "H.264 video, only DTS: converts");
  video_parar();
  // Identical Japanese DTS tracks: remember the second through the menu's
  // identity, then restore it before the next episode opens its converter.
  duplicateDts = 1; montarCabecalho(); pref = "jpn";
  audioesc_titulo("tt550"); audioesc_fonte_binge("release550");
  video_tocar("http://h/episode1.mkv");
  playerPronto(); bombear(3);
  ok(video_n_audio() == 2, "two identical Japanese DTS tracks in the converted menu");
  {
    const VideoFaixa *f = video_audio(1);
    if (f) {
      audioesc_lembrar_faixa(f->idioma, f->codec, f->rotulo, f->canais,
                            video_audio_numero_memoria(f));
      video_escolher_audio(1);
    }
  }
  video_parar();
  video_tocar("http://h/episode2.mkv");
  playerPronto(); bombear(3);
  ok(video_audio_atual() == 1 && video_audio(1)->stream_id == 7,
     "same bingeGroup restores the second identical Japanese DTS track");
  video_parar();
  audioesc_fonte_binge("another-release");
  video_tocar("http://h/episode3.mkv");
  playerPronto(); bombear(3);
  ok(video_audio_atual() == 0, "another bingeGroup falls back to the first Japanese DTS track");
  video_parar();
  duplicateDts = 0; audioesc_fonte_binge("");
  // KAGUYA BD T3 SHAPE (16/10): Japanese DTS-HD MA first and default, English
  // FLAC second. The TV hides the DTS, so its one audio track (A0) is the FLAC
  // and the file-to-player match is "none". English preferred: the FLAC plays,
  // nothing converts. Japanese preferred: the Japanese DTS converts.
  { unsigned char t[512]; long n = 0;
    n += faixa(t + n, 1, 1, "V_MPEG4/ISO/AVC", "eng", 0);
    n += faixa(t + n, 2, 2, "A_DTS", "jpn", 6);
    n += faixa(t + n, 3, 2, "A_FLAC", "eng", 2);
    n += faixa(t + n, 4, 17, "S_TEXT/ASS", "eng", 0);
    cabN = 0; cab[cabN++] = 0x1A; cab[cabN++] = 0x45; cab[cabN++] = 0xDF; cab[cabN++] = 0xA3; cab[cabN++] = 0x80;
    cabN += el(cab + cabN, 0x1654AE6B, 4, t, n);
    { unsigned char z[64] = {0}; cabN += el(cab + cabN, 0xEC, 1, z, sizeof z); } }
  audioesc_titulo("tt-bdt3");   // not the tt550 memory above
  cabPronto = 1; abriuConversor = 0; pref = "eng"; pref2 = ""; orig = ""; tvLang = "eng";
  video_tocar("http://h/t3.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "BD T3 shape, English preferred: the original opens (English FLAC playable)");
  inicioStream = -1; posMs = 2000;
  playerPronto(); bombear(20);
  ok(!abriuConversor && video_audio_atual() >= 0 && video_audio(video_audio_atual()) &&
     !strcmp(video_audio(video_audio_atual())->idioma, "eng") && video_audio(video_audio_atual())->numero >= 0,
     "BD T3 shape, English preferred: plays the TV's English track (the FLAC), no conversion");
  video_parar();
  abriuConversor = 0; pref = "jpn"; tvLang = "eng";
  video_tocar("http://h/t3b.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "BD T3 shape, Japanese preferred: converts the Japanese DTS");
  video_parar();
  pref = "";

  // THE CONVERTER RESCUES A DTS-ONLY FILE THE TV REFUSES (16/10, TTGA): the
  // header is not known at open, the TV says AudioCodecNotSupported. That is
  // not a failed source: no `falhou`, the Prepare error after it is ignored,
  // and the converter takes over as soon as the header is read.
  misto = 0; pref = ""; videoCodec = "V_MPEG4/ISO/AVC"; montarCabecalho();
  cabPronto = 0; abriuConversor = 0;
  video_tocar("http://h/r.mkv");
  ok(!abriuConversor, "rescue: the original opens (header not known yet)");
  cabPronto = 1;   // the header probe finds the tracks
  video_tpk_log_host("erro AudioCodecNotSupported");
  nv_tpk_video_evento(5, (int)0xFE6C002Eu, 0);
  bombear(1);
  ok(!video_falhou(), "rescue: AudioCodecNotSupported is not a failed source");
  // The Prepare exception of the ORIGINAL that follows, before the converted
  // player opens (later ones the host drops: Video.cs checks the session).
  nv_tpk_video_evento(5, -1, 0);
  bombear(1);
  ok(!video_falhou(), "rescue: the Prepare error after it is ignored too");
  { Uint32 t0 = SDL_GetTicks(); while (!abriuConversor && SDL_GetTicks() - t0 < 3000) bombear(1); }
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl) && !video_falhou(), "rescue: the converter takes over, no error");
  // S95C 16/10: the converter gave up on the same tick ("o player nao preparou
  // em 45 s", 0 requests) because convBombear's clock was read before the
  // converter opened. It must still be converting a moment later.
  bombear(10);
  ok(video_tpk_convertendo() && !video_falhou() && dts_tpk_e_url(ultUrl), "rescue: still converting a moment later (no instant give-up)");
  video_parar();
  // HEADER PROBE COMES BACK EMPTY (host paused for side reads, network): the
  // converter reads the file itself and takes over; the original never even
  // prepared (S95C 16/10: the TTGA file failed ~2 s after open, "no error code").
  cabPronto = 0; abriuConversor = 0;
  video_tocar("http://h/r2.mkv");
  nv_tpk_video_evento(5, (int)0xFE6C002Eu, 0);
  { Uint32 t0 = SDL_GetTicks(); while (!abriuConversor && !video_falhou() && SDL_GetTicks() - t0 < 4000) bombear(1); }
  bombear(10);
  ok(!video_falhou() && abriuConversor >= 1 && video_tpk_convertendo(), "rescue, header not read: the converter's own probe takes over");
  { Uint32 t0 = SDL_GetTicks(); while (!dts_tpk_e_url(ultUrl) && SDL_GetTicks() - t0 < 3000) bombear(1); }
  ok(dts_tpk_e_url(ultUrl) && !video_falhou(), "rescue, header not read: the player opens the converted stream");
  video_parar();
  // Nothing to convert (only a FLAC the TV refuses, say): failed at once, as before.
  cabPronto = 1; misto = 0;
  { unsigned char t[512]; long n = 0;   // video + one AAC: no DTS/TrueHD track
    n += faixa(t + n, 1, 1, "V_MPEG4/ISO/AVC", NULL, 0);
    n += faixa(t + n, 2, 2, "A_AAC", "eng", 2);
    cabN = 0; cab[cabN++] = 0x1A; cab[cabN++] = 0x45; cab[cabN++] = 0xDF; cab[cabN++] = 0xA3; cab[cabN++] = 0x80;
    cabN += el(cab + cabN, 0x1654AE6B, 4, t, n);
    { unsigned char z[64] = {0}; cabN += el(cab + cabN, 0xEC, 1, z, sizeof z); } }
  abriuConversor = 0;
  video_tocar("http://h/s.mkv");
  nv_tpk_video_evento(5, (int)0xFE6C002Eu, 0);
  { Uint32 t0 = SDL_GetTicks(); while (!video_falhou() && SDL_GetTicks() - t0 < 3000) bombear(1); }
  ok(video_falhou() && !abriuConversor, "rescue: nothing to convert -> failed right away");
  video_parar();
  printf(falhas ? "tpk-dts-reabrir: %d FAIL(S)\n" : "tpk-dts-reabrir: ok\n", falhas);
  return falhas != 0;
}
