// #771: MP4 hint must not drop a Matroska subtitle menu, and must not Range a
// real MP4. Drives video_tocar / video_bombear (review harness matrix).
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
extern void mkvfix_mp4_stub_ocultas(int n);
extern void mkvfix_mp4_stub_estado(int e);
extern void dts_tpk_stub_ligar(int on);
extern int dts_tpk_stub_aberturas(void);

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

static unsigned char cabMkv[1024];
static long cabMkvN;
static int cacheOn, rangeOn, posMs, redeN;

char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim, long *tam) {
  (void)url; (void)segundos; (void)ini; (void)fim;
  redeN++;
  if (!rangeOn || cabMkvN <= 0) { if (tam) *tam = 0; return 0; }
  char *b = malloc((size_t)cabMkvN);
  memcpy(b, cabMkv, (size_t)cabMkvN);
  if (tam) *tam = cabMkvN;
  return b;
}
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)tamFinal;
  redeN++;
  if (status) *status = 0;
  if (erro) *erro = 1;
  if (tam) *tam = 0;
  return 0;
}

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
static void montar(void) {
  unsigned char t[768]; long n = 0;
  n += faixa(t + n, 1, 1, "V_MPEGH/ISO/HEVC", NULL, 0);
  n += faixa(t + n, 2, 2, "A_AAC", "eng", 2);
  n += faixa(t + n, 3, 17, "S_TEXT/ASS", "eng", 0);
  n += faixa(t + n, 4, 17, "S_TEXT/UTF8", "por", 0);
  cabMkvN = 0;
  cabMkv[cabMkvN++] = 0x1A; cabMkv[cabMkvN++] = 0x45; cabMkv[cabMkvN++] = 0xDF; cabMkv[cabMkvN++] = 0xA3; cabMkv[cabMkvN++] = 0x80;
  cabMkvN += el(cabMkv + cabMkvN, 0x1654AE6B, 4, t, n);
}
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  if (!cacheOn) return 0;
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

static int falhas, nLegHost, nBuscar, ultBuscarMs = -1, nAbrirMkv;
static void hAbrir(const char *u, const char *c) { (void)c; if (u && strstr(u, "127.0.0.1")) nAbrirMkv++; }
static void hBuscarT(int ms) { nBuscar++; ultBuscarMs = ms; }
static void hSem(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
static int hPos(void) { return posMs; }
static void hEscolher(int tipo, int idx) { (void)idx; if (tipo == 1) nLegHost++; }
static void ok(int c, const char *o) { printf("%s %s\n", c ? "ok  " : "FAIL", o); if (!c) falhas++; }
static void bombear(int vezes) { while (vezes--) { video_bombear(); SDL_Delay(20); } }
static void bombear_ms(int ms) {
  Uint32 fim = SDL_GetTicks() + (Uint32)ms;
  do { video_bombear(); SDL_Delay(20); } while (SDL_GetTicks() < fim);
}
static void pronto(int dur, int nAud, int nLeg) {
  int i;
  nv_tpk_video_evento(1, dur, 0);
  for (i = 0; i < nAud; i++) nv_tpk_video_faixa(0, i, "eng");
  for (i = 0; i < nLeg; i++) nv_tpk_video_faixa(1, i, i ? "por" : "eng");
  nv_tpk_video_faixas_fim(nAud ? 0 : -1, nLeg ? 0 : -1);
}

int main(void) {
  int rede0, dts0, host0;
  SDL_Init(0);
  montar();
  nv_tpk_video_registrar(hAbrir, hSem, hInt, hBuscarT, hInt, hJanela, hPos);
  nv_tpk_video_registrar_faixas(hEscolher);
  video_iniciar_auto();

  /* A: cached hidden text, false MP4, no PGS. */
  cacheOn = 0; rangeOn = 0; posMs = 0; redeN = nLegHost = 0;
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/fake.mp4");
  pronto(7200000, 1, 2);
  bombear(2);
  cacheOn = 1; mkvfix_mp4_stub_ocultar(2);
  bombear(8);
  pronto(7200000, 1, 0);
  bombear(4);
  ok(video_n_legenda() == 2, "A cached hidden MP4 restores 2 file text subs");
  ok(video_legenda(0) && !strcmp(video_legenda(0)->codec, "S_TEXT/ASS"), "A ASS in file menu");
  ok(video_legenda_ordinal_mkv(0) == 0 && video_legenda_ordinal_mkv(1) == 1, "A ordinals 0 then 1");
  video_escolher_legenda(1);
  ok(video_legenda_atual() == 1 && nLegHost == 0, "A hidden choice stays overlay");
  ok(redeN == 0, "A cached path does no Range");
  video_parar();

  /* B: hidden false MP4, header only via Range. */
  cacheOn = 0; rangeOn = 1; posMs = 6000; redeN = nLegHost = 0;
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/fake2.mp4");
  pronto(7200000, 1, 0);
  bombear(4);
  mkvfix_mp4_stub_ocultar(2);
  bombear(2); /* reopen clears pronto; the host reports ready again */
  pronto(7200000, 1, 0);
  bombear_ms(1200);
  printf("info B rede=%d nLeg=%d sondado=%d\n", redeN, video_n_legenda(), video_mkv_sondado());
  ok(redeN > 0, "B uncached hidden MP4 issues Range");
  ok(video_n_legenda() == 2, "B uncached hidden MP4 restores file menu");
  ok(video_legenda(0) && !strcmp(video_legenda(0)->codec, "S_TEXT/ASS"), "B ASS from Range");
  video_parar();

  /* C: ordinary MKV, no prefetch, Range. */
  cacheOn = 0; rangeOn = 1; posMs = 6000; redeN = 0;
  video_definir_reconexao(1); video_definir_mp4(0);
  video_tocar("http://h/real.mkv");
  pronto(7200000, 1, 2);
  bombear_ms(1200);
  printf("info C rede=%d codec=%s sondado=%d\n", redeN,
         video_legenda(0) && video_legenda(0)->codec[0] ? video_legenda(0)->codec : "-",
         video_mkv_sondado());
  ok(redeN > 0, "C MKV without prefetch uses Range");
  ok(video_legenda(0) && !strcmp(video_legenda(0)->codec, "S_TEXT/ASS"), "C Range labels native text");
  ok(video_mkv_sondado() == 1, "C probe finished as MKV");
  video_parar();

  /* D: true MP4, no hidden, no cache, 0 audio -> converter, no Range. */
  cacheOn = 0; rangeOn = 1; posMs = 6000; rede0 = redeN;
  dts_tpk_stub_ligar(1); dts0 = dts_tpk_stub_aberturas();
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/real.mp4");
  pronto(7200000, 0, 0);
  bombear_ms(800);
  printf("info D redeDelta=%d sondado=%d dtsDelta=%d\n", redeN - rede0, video_mkv_sondado(),
         dts_tpk_stub_aberturas() - dts0);
  ok(redeN == rede0, "D true MP4 does no MKV Range");
  ok(video_mkv_sondado() == 2, "D true MP4 probe stays off");
  ok(dts_tpk_stub_aberturas() > dts0, "D true MP4 still side-probes converter");
  dts_tpk_stub_ligar(0);
  video_parar();

  /* E: correct MP4 hint, TV listed audio, no hidden, no cache. */
  cacheOn = 0; rangeOn = 1; posMs = 6000; rede0 = redeN; dts0 = dts_tpk_stub_aberturas();
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/hint.mp4");
  pronto(7200000, 1, 1);
  bombear_ms(900);
  printf("info E redeDelta=%d sondado=%d dtsDelta=%d\n", redeN - rede0, video_mkv_sondado(),
         dts_tpk_stub_aberturas() - dts0);
  ok(redeN == rede0, "E correct hint does no MKV Range");
  ok(video_mkv_sondado() == 2, "E correct hint keeps MKV probe off");
  ok(dts_tpk_stub_aberturas() == dts0, "E correct hint does not side-probe");
  video_parar();

  /* F: chosen ordinal survives hidden reopen. */
  cacheOn = 0; rangeOn = 0; posMs = 0; nLegHost = 0;
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/ord.mp4");
  pronto(7200000, 1, 2);
  video_escolher_legenda(1);
  host0 = nLegHost;
  cacheOn = 1; mkvfix_mp4_stub_ocultar(2);
  bombear(8);
  pronto(7200000, 1, 0);
  bombear(4);
  ok(video_legenda_atual() == 1 && video_legenda_ordinal_mkv(video_legenda_atual()) == 1,
     "F chosen ordinal 1 survives reopen");
  ok(nLegHost == host0, "F reopen does not rewrite player subtitle");
  video_parar();

  /* G: live hinted MP4, recon off. */
  cacheOn = 0; rangeOn = 1; posMs = 6000; rede0 = redeN; dts0 = dts_tpk_stub_aberturas();
  video_definir_reconexao(0); video_definir_mp4(1);
  video_tocar("http://h/live.mp4");
  pronto(0, 0, 0);
  bombear_ms(900);
  ok(redeN == rede0 && dts_tpk_stub_aberturas() == dts0, "G live MP4 adds no side connection");
  video_parar();

  /* H: resume at a nonzero point with ASS hidden from the TV. The seek waits
     for the probe; the reopen through the local server lands ON the target
     (the TV log had it back at 0 s). */
  cacheOn = 0; rangeOn = 0; posMs = 0; redeN = nLegHost = nBuscar = nAbrirMkv = 0; ultBuscarMs = -1;
  video_definir_reconexao(1); video_definir_mp4(1);
  video_tocar("http://h/resume.mp4");
  pronto(7200000, 1, 0);
  mkvfix_mp4_stub_estado(MKVFIX_SONDANDO);   /* a sonda no ar (.mp4 nao sonda no stub) */
  video_buscar(459.0);
  ok(nBuscar == 0 && video_tpk_seek_segurado_ms() > 0, "H resume seek held during the probe");
  mkvfix_mp4_stub_ocultas(2); mkvfix_mp4_stub_estado(MKVFIX_NORMAL);
  bombear(4);
  ok(nAbrirMkv > 0, "H hidden text subs reopen through the local server");
  pronto(7200000, 1, 0);
  nv_tpk_video_evento(2, 0, 0);              /* tocando: o alvo sai */
  bombear(4);
  ok(ultBuscarMs == 459000, "H the reopen seeks to the held target, not back to 0");
  video_parar();

  printf(falhas ? "tpk-mp4-mkvfix-legendas: %d FAIL(S)\n" : "tpk-mp4-mkvfix-legendas: ok\n", falhas);
  return falhas != 0;
}
