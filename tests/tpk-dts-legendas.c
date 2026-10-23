// SUBTITLES WHILE DTS/TrueHD IS CONVERTED (.tpk), exercised with no TV.
//
// Text subtitles (ASS/SSA, SRT) are drawn by the app's overlay reading the
// original MKV, so on the converted stream the menu is built from the file. This
// pins what that menu must be: image subtitles (PGS) left out as the TV leaves
// them out, one distinct `numero` per entry (the sheet's id and the remembered
// choice use it), the mkvass ordinal of each track, and the chosen subtitle
// kept when the player is swapped for the converted one mid-film.
//
// Runs src/video_tpk.c against a fake host and a fake converter (always
// available, HLS) with a hand-built MKV header: video, DTS, PGS, ASS, SRT.
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"
#include "dts/dts_hls_tpk.h"

const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return c && p && !strcmp(c, p); }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
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
static void montarCabecalho(void) {
  unsigned char t[768];
  long n = 0;
  n += faixa(t + n, 1, 1, "V_MPEGH/ISO/HEVC", NULL, 0);
  n += faixa(t + n, 2, 2, "A_DTS", "eng", 6);
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

// --- the converter: always there, HLS ---------------------------------------
static int abriuConversor;
int proxyts_e_url(const char *u) { (void)u; return 0; }
int ajustes_dts_tpk_modo(void) { return DTSTPK_HLS; }
int ajustes_dts_ac3(void) { return 1; }
int dts_tpk_disponivel(void) { return 1; }
const char *dts_tpk_modo_nome(int m) { (void)m; return "HLS"; }
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n) {
  (void)url; (void)headers; (void)audio_ordinal; (void)stereo_aac; (void)modo; (void)inicio;
  abriuConversor++;
  snprintf(out, n, "http://127.0.0.1:1/tok/v.m3u8");
  return 1;
}
int dts_tpk_url_em(double s, char *out, size_t n) { (void)s; (void)out; (void)n; return 0; }
void dts_tpk_fechar(void) {}
int dts_tpk_estado(void) { return DTSTPK_PRONTO; }
const char *dts_tpk_erro(void) { return ""; }
double dts_tpk_duracao(void) { return 7200; }
int dts_tpk_modo(void) { return DTSTPK_HLS; }
double dts_tpk_inicio_stream(void) { return -1; }
const char *dts_tpk_caminho(void) { return "DTS core only"; }
int dts_tpk_prebuffer_ms(void) { return 0; }
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
static char ultUrl[256];
static int nLegsHost;
static void hAbrir(const char *u, const char *c) { (void)c; snprintf(ultUrl, sizeof ultUrl, "%s", u); }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return 0; }
static void hEscolher(int tipo, int idx) { (void)idx; if (tipo == 1) nLegsHost++; }

static int falhas;
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static int nTexto(void) {
  int i, n = 0;
  for (i = 0; i < video_n_legenda(); i++) if (!strncmp(video_legenda(i)->codec, "S_TEXT/", 7)) n++;
  return n;
}
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(5); } }
static void playerPronto(int nAud, int nLegTv) {
  int i;
  nv_tpk_video_evento(1, 7200000, 0);
  for (i = 0; i < nAud; i++) nv_tpk_video_faixa(0, i, "eng");
  for (i = 0; i < nLegTv; i++) nv_tpk_video_faixa(1, i, i ? "por" : "eng");
  nv_tpk_video_faixas_fim(nAud ? 0 : -1, -1);
}

int main(void) {
  SDL_Init(0);
  montarCabecalho();
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hInt, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();

  // A: header known before opening -> converted from the start.
  cabPronto = 1;
  video_tocar("http://h/a.mkv");
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "A: opens the converted stream directly");
  ok(video_n_legenda() == 3 && nTexto() == 2, "A: menu has the file's 3 subtitles (PGS is drawn by the app)");
  ok(video_n_legenda() == 3 && video_legenda(0)->numero == 0 && video_legenda(1)->numero == 1 &&
     video_legenda(2)->numero == 2, "A: each subtitle has its own numero");
  ok(video_n_legenda() == 3 && video_legenda(0)->ordinalMkv == 0 && video_legenda(1)->ordinalMkv == 1 &&
     video_legenda(2)->ordinalMkv == 2, "A: mkvass ordinals follow the file order (PGS 0, ASS 1, SRT 2)");
  ok(video_n_legenda() == 3 && !strcmp(video_legenda(0)->codec, "S_HDMV/PGS") &&
     !strcmp(video_legenda(2)->codec, "S_TEXT/UTF8"), "A: codec of the PGS and SRT entries");
  playerPronto(1, 0);   // the converted player: one AC3, no subtitles
  bombear(3);
  ok(video_n_legenda() == 3 && video_n_audio() == 1, "A: player's own list does not replace the file's");
  video_escolher_legenda(1);
  ok(video_legenda_atual() == 1 && nLegsHost == 0, "A: choosing a subtitle stays in the app (nothing to the player)");
  playerPronto(1, 0);   // the host re-reads the audio (#165)
  bombear(3);
  ok(video_legenda_atual() == 1, "A: chosen subtitle survives the re-read");
  video_parar();

  // B: original first (TV lists the 2 text subtitles, no audio), then the
  // header arrives and the conversion starts mid-film.
  cabPronto = 0; abriuConversor = 0;
  video_tocar("http://h/b.mkv");
  ok(!abriuConversor && !dts_tpk_e_url(ultUrl), "B: opens the original");
  playerPronto(0, 2);
  bombear(2);
  video_escolher_legenda(1);   // Portuguese SRT, picked on the original
  ok(video_legenda_atual() == 1, "B: subtitle 1 chosen on the original");
  cabPronto = 1;
  { Uint32 t0 = SDL_GetTicks(); while (!abriuConversor && SDL_GetTicks() - t0 < 3000) bombear(1); }
  ok(abriuConversor == 1 && dts_tpk_e_url(ultUrl), "B: switches to the converted stream");
  ok(video_n_legenda() == 3 && video_legenda_atual() == 2 && !strcmp(video_legenda(2)->codec, "S_TEXT/UTF8") &&
     !strcmp(video_legenda(2)->idioma, "por"), "B: the same subtitle (por SRT) is still chosen after the switch");
  playerPronto(1, 0);
  bombear(3);
  ok(video_legenda_atual() == 2 && video_n_legenda() == 3, "B: and after the converted player answers");
  video_parar();

  printf(falhas ? "tpk-dts-legendas: %d FAIL(S)\n" : "tpk-dts-legendas: ok\n", falhas);
  return falhas != 0;
}
