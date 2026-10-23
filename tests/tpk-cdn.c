// No TV: the .tpk player gets the TorBox CDN link instead of the add-on link
// (cdnlink.h). Runs src/video_tpk.c against a fake host and a fake resolver:
//  - the add-on link is resolved in a thread; the player opens the CDN link;
//  - a link the source check resolved a moment ago is used with no new request;
//  - a failed resolve opens the add-on link, as before;
//  - live channels (no reconnect) and local links never go through the CDN;
//  - stop while resolving: nothing opens afterwards;
//  - a seek / resume with an old CDN link reopens at the position with a fresh
//    link (for the app it is a seek), a fresh link seeks normally;
//  - a non-network player error after playing with a CDN link reconnects
//    (fresh link), at the position.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"
#include "dts/dts_hls_tpk.h"
#include "audioescolha.h"
#include "cdnlink.h"

int ajustes_audio_offset_ms(void) { return 0; }
void dts_remux_set_audio_offset_ms(int ms) { (void)ms; }
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
int dts_tpk_disponivel(void) { return 0; }
const char *dts_tpk_modo_nome(int m) { (void)m; return "TS"; }
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n) {
  (void)url; (void)headers; (void)audio_ordinal; (void)stereo_aac; (void)modo;
  abriuConversor++;
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
static char ultCab[512];
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

// --- the fake cdnlink ---------------------------------------------------------
static int resolveOk = 1, nResolve, recenteSim, validadeMs = 150 * 60 * 1000;
static const char *cachedUrl = "https://cdn.test/checked";
static int fOrigem(const char *u) { return u && !strncmp(u, "http", 4) && !strstr(u, "127.0.0.1") && !strstr(u, ".m3u8") && !strstr(u, "tb-cdn.st"); }
static int fRecente(const char *o, const char *cab, unsigned maxMs, char *d, unsigned t, unsigned *idade) {
  (void)o; (void)cab; (void)maxMs;
  if (!recenteSim) return 0;
  snprintf(d, t, "%s", cachedUrl); if (idade) *idade = 5000; return 1;
}
static int fResolver(const char *o, const char *c, int s, char *d, unsigned t, unsigned *ms) {
  (void)o; (void)c; (void)s; (void)ms;
  nResolve++;
  SDL_Delay(20);
  if (!resolveOk) { d[0] = 0; return 0; }
  snprintf(d, t, "https://cdn.test/fresh%d", nResolve);
  return 1;
}
static int fileRequests;
int debrid_tb_link(int usenet, int item, int file, char *dst, unsigned n) {
  ok(usenet == 1 && item == 42 && file == 7, "direct refresh keeps item/file identity");
  fileRequests++;
  SDL_Delay(20);
  snprintf(dst, n, "https://nexus.tb-cdn.st/fresh%d", fileRequests);
  return 1;
}
#include "tpk-cdn-mkv.inc"
static unsigned fValidade(void) { return (unsigned)validadeMs; }
void video_tpk_cdn_arquivo(const char *url, int usenet, int item, int file);
static void esperarAbrir(int n) { Uint32 t0 = SDL_GetTicks(); while (nAbriu < n && SDL_GetTicks() - t0 < 3000) bombear(1); }

int main(void) {
  static const CdnlinkGanchos g = { fOrigem, fRecente, fResolver, fValidade };
  SDL_Init(0);
  nv_tpk_video_registrar(hAbrir, hSem, hPausar, hBuscar, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);

  // NO HOOKS: the add-on link, at once (the other tpk tests rely on this).
  video_definir_reconexao(1);
  video_tocar("http://addon.test/a.mkv");
  ok(nAbriu == 1 && !strcmp(ultUrl, "http://addon.test/a.mkv"), "no hooks: the player opens the add-on link at once");
  video_parar();

  video_tpk_cdn_ganchos(&g);

  // Both CDN open paths must keep only destination-safe headers.
  const char *headers = "Cookie: session=secret\naUtHoRiZaTiOn: Bearer secret\nX-Api-Key: secret\nUser-Agent: fixture\nAccept: video/*\n";
  for (int cached = 0; cached < 2; cached++) {
    recenteSim = cached; nAbriu = 0;
    video_definir_reconexao(1);
    video_definir_cabecalhos(headers);
    video_tocar("https://addon.test/private.mkv");
    esperarAbrir(1);
    ok(!strcmp(ultCab, "User-Agent: fixture\nAccept: video/*\n"),
       cached ? "cached CDN strips origin credentials" : "resolved CDN strips origin credentials");
    ok(!strcmp(video_cabecalhos(), headers), "source credentials retained for source requests");
    video_parar();
  }
  cachedUrl = "https://ADDON.test/same-origin"; recenteSim = 1; nAbriu = 0;
  video_definir_reconexao(1);
  video_definir_cabecalhos(headers);
  video_tocar("https://addon.test/private.mkv");
  ok(!strcmp(ultCab, headers), "same origin retains source credentials");
  video_parar();
  cachedUrl = "https://addon.test:8443/other-origin"; nAbriu = 0;
  video_definir_reconexao(1);
  video_definir_cabecalhos(headers);
  video_tocar("https://addon.test/private.mkv");
  ok(!strstr(ultCab, "secret"), "different port strips source credentials");
  video_parar(); cachedUrl = "https://cdn.test/checked";
  recenteSim = 0; resolveOk = 0; nAbriu = 0;
  video_definir_reconexao(1);
  video_definir_cabecalhos(headers);
  video_tocar("https://addon.test/private.mkv"); esperarAbrir(1);
  ok(!strcmp(ultCab, headers), "failed CDN keeps source credentials on source host");
  video_parar(); video_definir_cabecalhos(""); resolveOk = 1;

  // A direct TorBox URL skips redirect resolution, but still expires.
  nAbriu = nBuscas = fileRequests = 0;
  video_definir_reconexao(1);
  video_tocar("https://nexus.tb-cdn.st/direct");
#ifdef NV_TEST_DIRECT_CDN
  video_tpk_cdn_arquivo("https://nexus.tb-cdn.st/direct", 1, 42, 7);
#endif
  posMs = 60000; playerPronto(); bombear(3);
  validadeMs = 0; SDL_Delay(2);
  nBuscas = 0;
  video_buscar(600);
  ok(nBuscas == 0, "expired direct file: no raw seek");
  bombear(2);
  esperarAbrir(2);
  ok(fileRequests == 1 && !strcmp(ultUrl, "https://nexus.tb-cdn.st/fresh1"), "expired direct file: requestdl before seek");
  validadeMs = 150 * 60 * 1000;
  playerPronto(); bombear(3); posMs = 600000; bombear(3);
  video_pausar(1); bombear(2);
  validadeMs = 0; SDL_Delay(2); retomadas = 0;
  video_pausar(0); esperarAbrir(3);
  ok(retomadas == 0 && fileRequests == 2, "expired direct file: requestdl before resume");
  video_parar();
  validadeMs = 150 * 60 * 1000;

  // Hidden-index selected files must refresh inside the active proxy.
  hiddenHeader(); hiddenMkv = 1;
  for (int i = 0; i < 3; i++) {
    nAbriu = fileRequests = 0;
    video_definir_reconexao(1);
    video_tocar("https://nexus.tb-cdn.st/direct");
    video_tpk_cdn_arquivo("https://nexus.tb-cdn.st/direct", 1, 42, 7);
    playerPronto();
    Uint32 start = SDL_GetTicks();
    while (mkvfix_estado() != MKVFIX_PRECISA && SDL_GetTicks() - start < 3000) bombear(1);
    ok(mkvfix_estado() == MKVFIX_PRECISA, "selected file has hidden-index repair");
    video_buscar(120); esperarAbrir(2);
    playerPronto(); bombear(3);
    video_pausar(1); validadeMs = 0; SDL_Delay(2); video_pausar(0);
    expiredStatus = i == 0 ? 403 : i == 1 ? 404 : 410;
    proxyRead(ultUrl);
    ok(fileRequests == 1, "proxy expiry requests same TorBox item/file once");
    video_parar(); validadeMs = 150 * 60 * 1000;
  }
  hiddenMkv = 0;

  // RESOLVED IN A THREAD: nothing opens until the CDN link is known.
  nAbriu = 0; nResolve = 0;
  video_definir_reconexao(1);
  video_tocar("http://addon.test/a.mkv");
  ok(nAbriu == 0, "resolving: the player is not opened yet");
  ok(!video_pronto(), "resolving: the app shows the opening card");
  esperarAbrir(1);
  ok(nAbriu == 1 && !strcmp(ultUrl, "https://cdn.test/fresh1"), "the player opens the CDN link");
  posMs = 1000; playerPronto(); bombear(3);
  posMs = 60000; bombear(3);
  ok(perto(video_pos(), 60.0) && video_url_atual() && !strcmp(video_url_atual(), "http://addon.test/a.mkv"),
     "plays; the source identity stays the add-on link");

  // FRESH LINK: a seek is an ordinary seek.
  nAbriu = 0; nBuscas = 0;
  video_buscar(120);
  ok(nBuscas == 1 && nAbriu == 0, "fresh CDN link: seek is a normal seek");

  // OLD LINK: the seek reopens at the target with a fresh link.
  validadeMs = 0; SDL_Delay(2);
  nAbriu = 0; nBuscas = 0; nResolve = 0;
  video_buscar(600);
  ok(nAbriu == 0 && nBuscas == 0, "old link: no raw seek on it");
  ok(video_pronto() && perto(video_pos(), 600), "old link: for the app it is a seek (no opening card, position = target)");
  esperarAbrir(1);
  ok(nResolve == 1 && nAbriu == 1 && !strcmp(ultUrl, "https://cdn.test/fresh1"), "old link: reopened with a fresh CDN link");
  validadeMs = 150 * 60 * 1000;
  posMs = 0; playerPronto(); bombear(3);
  ok(nBuscas == 1, "old link: the new player seeks to the target once it plays");
  posMs = 600500; bombear(3);

  // OLD LINK + PAUSE: resume reopens at the position.
  pausas = retomadas = 0;
  video_pausar(1); bombear(2);
  validadeMs = 0; SDL_Delay(2);
  nAbriu = 0; nResolve = 0; nBuscas = 0;
  video_pausar(0);
  ok(retomadas == 0, "old link + resume: no raw resume on the old link");
  esperarAbrir(1);
  ok(nAbriu == 1 && !strncmp(ultUrl, "https://cdn.test/fresh", 22), "old link + resume: reopened with a fresh link");
  validadeMs = 150 * 60 * 1000;
  posMs = 0; playerPronto(); bombear(3);
  ok(nBuscas == 1 && perto(video_pos(), 0.0) == 0, "old link + resume: back to the paused position");
  posMs = 700000; bombear(3);

  // ERROR AFTER PLAYING, NOT A NETWORK ERROR: reconnect with a fresh link.
  nAbriu = 0; nResolve = 0;
  nv_tpk_video_evento(5, (int)0xFE6C0031u, 0);
  bombear(2);
  ok(video_reconectando() > 0 && !video_falhou(), "CDN link + error after playing: reconnect, not a failed source");
  { Uint32 t0 = SDL_GetTicks(); while (!nAbriu && SDL_GetTicks() - t0 < 5000) bombear(1); }
  ok(nAbriu == 1 && nResolve == 1 && !strncmp(ultUrl, "https://cdn.test/fresh", 22), "reconnect: fresh CDN link");
  video_parar();

  // THE CHECK RESOLVED IT A MOMENT AGO: no new request.
  recenteSim = 1; nAbriu = 0; nResolve = 0;
  video_definir_reconexao(1);
  video_tocar("http://addon.test/b.mkv");
  ok(nAbriu == 1 && nResolve == 0 && !strcmp(ultUrl, "https://cdn.test/checked"), "checked a moment ago: CDN link at once, no request");
  video_parar();
  recenteSim = 0;

  // RESOLVE FAILS: the add-on link, as before.
  resolveOk = 0; nAbriu = 0;
  video_definir_reconexao(1);
  video_tocar("http://addon.test/c.mkv");
  esperarAbrir(1);
  ok(nAbriu == 1 && !strcmp(ultUrl, "http://addon.test/c.mkv"), "resolve fails: the add-on link");
  // ... and an error after playing with the add-on link is NOT turned into a reconnect.
  posMs = 1000; playerPronto(); bombear(3); posMs = 60000; bombear(3);
  nv_tpk_video_evento(5, (int)0xFE6C0031u, 0);
  bombear(2);
  ok(video_falhou(), "add-on link + format error after playing: failed as before");
  video_parar();
  resolveOk = 1;

  // LIVE CHANNEL / LOCAL: never through the CDN.
  nAbriu = 0; nResolve = 0;
  video_definir_reconexao(0);
  video_tocar("http://addon.test/live.ts");
  ok(nAbriu == 1 && nResolve == 0, "live channel: the link as is");
  video_parar();
  nAbriu = 0;
  video_definir_reconexao(1);
  video_tocar("http://127.0.0.1:9/x.ts");
  ok(nAbriu == 1 && nResolve == 0, "local proxy link: as is");
  video_parar();

  // STOP WHILE RESOLVING: nothing opens afterwards.
  nAbriu = 0;
  video_definir_reconexao(1);
  video_tocar("http://addon.test/d.mkv");
  video_parar();
  SDL_Delay(60); bombear(5);
  ok(nAbriu == 0, "stop while resolving: the player is never opened");

  if (falhas) { printf("tpk-cdn: %d falha(s)\n", falhas); return 1; }
  printf("tpk-cdn: ok\n");
  return 0;
}
