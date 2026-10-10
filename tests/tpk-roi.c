// Sem TV: o .tpk nunca manda ao plano um ROI fora da tela (#188, #195).
// Liga src/video_tpk.c a um host falso que so anota o ultimo retangulo.
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "video.h"
#include "faixasmkv.h"
#include "mkvass.h"

// This geometry fixture has no media bytes or network metadata provider.
void mkvass_aceitar_texto(int sim) { (void)sim; }

const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return !strcmp(c, p); }
const char *ling_audio(void) { return ""; }
int ling_tipo_legenda(const char *s, int f, int d) { (void)s; (void)f; (void)d; return 0; }
const char *ling_tipo_legenda_rotulo(int t) { (void)t; return ""; }
const char *ling_do_nome(const char *s) { (void)s; return NULL; }
int ling_letreiro(const char *s, int f) { (void)s; (void)f; return 0; }


void nv_tpk_video_registrar(void (*)(const char *, const char *), void (*)(void), void (*)(int),
                            void (*)(int), void (*)(int), void (*)(int, int, int, int), int (*)(void));
void nv_tpk_video_evento(int tipo, int a, int b);

static int jx, jy, jw, jh, nj;
static void hAbrir(const char *u, const char *c) { (void)u; (void)c; }
static void hParar(void) {}
static void hInt(int v) { (void)v; }
static void hJanela(int x, int y, int w, int h) { jx = x; jy = y; jw = w; jh = h; nj++; }
static int hPos(void) { return 0; }

static int falhas;
static void confere(const char *nome, int x, int y, int w, int h) {
  int ok = jx == x && jy == y && jw == w && jh == h;
  printf("%s %s: %d,%d %dx%d (esperado %d,%d %dx%d)\n", ok ? "ok  " : "FALHA", nome, jx, jy, jw, jh, x, y, w, h);
  if (!ok) falhas++;
}
static __attribute__((unused)) void naTela(const char *nome) {
  int ok = jx >= 0 && jy >= 0 && jx + jw <= 1920 && jy + jh <= 1080;
  printf("%s %s: %d,%d %dx%d dentro da tela\n", ok ? "ok  " : "FALHA", nome, jx, jy, jw, jh);
  if (!ok) falhas++;
}

int main(void) {
  nv_tpk_video_registrar(hAbrir, hParar, hInt, hInt, hInt, hJanela, hPos);
  video_tocar("http://x/trailer.m3u8");
  assert(!video_erro_texto()[0]);
  nv_tpk_video_evento(5, (int)0xfe6c0031u, 0);
  video_bombear();
  assert(video_falhou());
  assert(strstr(video_erro_texto(), "0xfe6c0031"));
  video_tocar("http://x/replacement.m3u8");
  assert(!video_falhou());
  assert(!video_erro_texto()[0]);

  nv_tpk_video_evento(6, 3828, 1588);   // Apple matted, registro 11875
  nv_tpk_video_evento(1, 60000, 0);
#if NV_TPK_ZOOM_ROI
  if (!video_recorte_fonte()) { printf("FALHA canario sem recorte\n"); return 1; }
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  confere("canario deixa o roi de zoom passar", -803, -191, 3530, 1466);
#else
  if (video_recorte_fonte()) { printf("FALHA recorte ligado no padrao\n"); falhas++; }
  // Runtime flag (#241, #290): opt-in, and since #290 it also unlocks the
  // player's crop/zoom aspect modes, not only the trailer.
  video_tpk_zoom_roi_definir(1);
  if (!video_recorte_fonte()) { printf("FALHA flag ligada nao libera o recorte no player\n"); falhas++; }
  if (!video_recorte_fonte_trailer() || !video_tpk_zoom_roi()) { printf("FALHA flag em execucao nao liga\n"); falhas++; }
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  confere("flag ligada, player: roi de zoom passa", -803, -191, 3530, 1466);
  video_tpk_trailer_marcar(1);
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  confere("flag ligada + trailer deixa o roi fora da tela passar", -803, -191, 3530, 1466);
  video_tpk_trailer_marcar(0);
  video_tpk_zoom_roi_definir(0);
  if (video_recorte_fonte()) { printf("FALHA flag desligada deixou recorte no player\n"); falhas++; }
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  naTela("flag desligada: roi volta para a tela");
  video_tpk_trailer_marcar(1);
  video_tocar("http://x/filme.m3u8");   // outra abertura zera a marca
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  naTela("nova abertura com a flag desligada");
  // O mesmo pedido do registro 11875 ("-> roi -803,-191 3530x1466").
  video_janela_fonte(872, 208, 2082, 1170, 0, 0, 1920, 1080);
  naTela("zoom do trailer 3828x1588");
  confere("zoom do trailer vira o destino", 0, 0, 1920, 1080);
  // 11619: "roi -118,0 2161x1080", origem negativa so em x.
  nv_tpk_video_evento(6, 1920, 960);
  video_janela_fonte(106, 0, 1706, 960, 0, 0, 1920, 1080);
  naTela("recorte lateral 1920x960");
  // Hero menor que a tela, zoom estoura por baixo.
  video_janela_fonte(0, 100, 1920, 760, 100, 500, 1600, 580);
  naTela("hero parcial");
  // Reaplicar nao ressuscita ROI recusado.
  nj = 0; video_recorte_reaplicar();
  if (nj) { printf("FALHA reaplicar mandou %d,%d %dx%d\n", jx, jy, jw, jh); falhas++; }
  else printf("ok   reaplicar nao manda nada\n");
#endif
  // Quadro inteiro num destino pequeno continua passando cru.
  nv_tpk_video_evento(6, 1920, 1080);
  video_janela_fonte(0, 0, 1920, 1080, 10, 10, 640, 360);
  confere("quadro inteiro no PiP", 10, 10, 640, 360);
#if !NV_TPK_ZOOM_ROI
  // Zoom num destino pequeno que estoura a tela pela esquerda/topo.
  video_janela_fonte(480, 270, 960, 540, 100, 100, 640, 360);
  naTela("zoom no PiP");
#endif
  printf(falhas ? "tpk-roi: %d falha(s)\n" : "tpk-roi: ok\n", falhas);
  return falhas != 0;
}
