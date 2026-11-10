// #771: wrong MP4 hint on a real Matroska must not drop the mkvfix subtitle
// menu after hidden ASS reopen. Exercises src/video_tpk.c (fake host, stub mkvfix).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "mkvfix.h"
#include "video.h"
#include "mkvass.h"
#include "intro.h"
#include "rede.h"

extern void mkvfix_mp4_stub_ocultar(int n);

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
void intro_definir_capitulos_palpite(const IntroTrecho *v, int n) { (void)v; (void)n; }
void intro_definir_capitulos_marcas(const double *v, int n) { (void)v; (void)n; }
void rede_lateral(int sim) { (void)sim; }
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

static unsigned char cabMkv[1024];
static long cabMkvN;
static int cabPronto, mkvCabPronto;
static long el(unsigned char *o, unsigned id, int idLen, const unsigned char *d, long n) {
  long p = 0;
  for (int i = idLen - 1; i >= 0; i--) o[p++] = (unsigned char)(id >> (8 * i));
  o[p++] = (unsigned char)(0x40 | (n >> 8)); o[p++] = (unsigned char)n;
  memcpy(o + p, d, (size_t)n);
  return p + n;
}
static long uintEl(unsigned char *o, unsigned id, int idLen, unsigned v) {
  unsigned char b = (unsigned char)v;
  return el(o, id, idLen, &b, 1);
}
static long faixa(unsigned char *o, int num, int tipo, const char *codec, const char *lang, int canais) {
  unsigned char d[256]; long n = 0;
  n += uintEl(d + n, 0xD7, 1, (unsigned)num);
  n += uintEl(d + n, 0x83, 1, (unsigned)tipo);
  n += el(d + n, 0x86, 1, (const unsigned char *)codec, (long)strlen(codec));
  if (lang) n += el(d + n, 0x22B59C, 3, (const unsigned char *)lang, (long)strlen(lang));
  if (canais) { unsigned char a[8]; long k = uintEl(a, 0x9F, 1, (unsigned)canais); n += el(d + n, 0xE1, 1, a, k); }
  return el(o, 0xAE, 1, d, n);
}
static long montarCab(unsigned char *out, const unsigned char *tracks, long tracksN) {
  long n = 0;
  out[n++] = 0x1A; out[n++] = 0x45; out[n++] = 0xDF; out[n++] = 0xA3; out[n++] = 0x80;
  n += el(out + n, 0x1654AE6B, 4, tracks, tracksN);
  { unsigned char z[64] = {0}; n += el(out + n, 0xEC, 1, z, sizeof z); }
  return n;
}
static void montarCabecalhos(void) {
  unsigned char tMkv[768]; long nMkv = 0;
  nMkv += faixa(tMkv + nMkv, 1, 1, "V_MPEGH/ISO/HEVC", NULL, 0);
  nMkv += faixa(tMkv + nMkv, 2, 2, "A_AAC", "eng", 2);
  nMkv += faixa(tMkv + nMkv, 3, 17, "S_HDMV/PGS", "eng", 0);
  nMkv += faixa(tMkv + nMkv, 4, 17, "S_TEXT/ASS", "eng", 0);
  nMkv += faixa(tMkv + nMkv, 5, 17, "S_TEXT/UTF8", "por", 0);
  cabMkvN = montarCab(cabMkv, tMkv, nMkv);
}
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  if (!mkvCabPronto) return 0;
  if (url && strstr(url, "127.0.0.1")) return 0;
  if (buf) { *buf = malloc((size_t)cabMkvN); memcpy(*buf, cabMkv, (size_t)cabMkvN); }
  if (n) *n = cabMkvN;
  return 1;
}

void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_registrar_faixas(void (*)(int, int));
void nv_tpk_video_faixa(int tipo, int idx, const char *lingua);
void nv_tpk_video_faixas_fim(int selAudio, int selLeg);
void nv_tpk_video_evento(int tipo, int a, int b);

static int falhas, aberturas, nLegsHost;
static char ultUrl[256];
static void hAbrir(const char *u, const char *c) {
  (void)c; snprintf(ultUrl, sizeof ultUrl, "%s", u ? u : ""); aberturas++;
}
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return 0; }
static void hEscolher(int tipo, int idx) { (void)idx; if (tipo == 1) nLegsHost++; }
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(5); } }
static void playerPronto(int nAud, int nLegTv) {
  int i;
  nv_tpk_video_evento(1, 7200000, 0);
  nv_tpk_video_evento(3, 0, 0); /* ready so sondaMkv can run before 5s */
  for (i = 0; i < nAud; i++) nv_tpk_video_faixa(0, i, "eng");
  for (i = 0; i < nLegTv; i++) nv_tpk_video_faixa(1, i, i ? "por" : "eng");
  nv_tpk_video_faixas_fim(nAud ? 0 : -1, nLegTv ? 0 : -1);
}

int main(void) {
  SDL_Init(0);
  montarCabecalhos();
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hInt, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();

  video_definir_reconexao(1);
  video_definir_mp4(1);
  cabPronto = 0; mkvCabPronto = 0;
  aberturas = nLegsHost = 0;
  video_tocar("http://h/fake.mp4");
  ok(aberturas == 1 && !strcmp(ultUrl, "http://h/fake.mp4"), "C: opens hinted MP4 URL first");
  playerPronto(1, 2);
  bombear(3);
  ok(video_n_legenda() == 2, "C: TV lists text subtitles before mkvfix");
  mkvfix_mp4_stub_ocultar(2);
  mkvCabPronto = 1;
  bombear(15);
  ok(aberturas == 2 && mkvfix_e_url(ultUrl), "C: mkvfix reopen uses local server");
  playerPronto(1, 0);
  bombear(15);
  ok(video_n_legenda() == 3, "C: file menu restored after mkvfix reopen with MP4 hint");
  ok(video_mkv_sondado() != 2, "C: MKV probe not disabled while header is needed");
  ok(video_legenda(1) && !strcmp(video_legenda(1)->codec, "S_TEXT/ASS"), "C: ASS entry in file menu");
  video_escolher_legenda(2);
  ok(video_legenda_atual() == 2 && nLegsHost == 0, "C: hidden subtitle stays in app overlay");
  video_parar();

  mkvCabPronto = 0; aberturas = 0;
  mkvfix_parar();
  video_definir_mp4(1);
  video_tocar("http://h/real.mp4");
  playerPronto(1, 1);
  bombear(5);
  mkvfix_mp4_stub_ocultar(1);
  bombear(5);
  ok(aberturas == 1, "D: real MP4 without MKV header does not reopen");
  ok(video_mkv_sondado() == 2, "D: real MP4 keeps MKV probe off");
  video_parar();

  printf(falhas ? "tpk-mp4-mkvfix-legendas: %d FAIL(S)\n" : "tpk-mp4-mkvfix-legendas: ok\n", falhas);
  return falhas != 0;
}
