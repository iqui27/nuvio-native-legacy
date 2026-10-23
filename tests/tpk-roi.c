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
void mkvass_aceitar_imagem(int sim) { (void)sim; }

const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *c) { return c; }
int ling_casa(const char *c, const char *p) { return !strcmp(c, p); }
const char *ling_audio(void) { return ""; }
const char *ling_audio2(void) { return ""; }
const char *ling_original(void) { return ""; }
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
void video_tpk_log_host(const char *);
static void confere(const char *nome, int x, int y, int w, int h) {
  int ok = jx == x && jy == y && jw == w && jh == h;
  printf("%s %s: %d,%d %dx%d (esperado %d,%d %dx%d)\n", ok ? "ok  " : "FALHA", nome, jx, jy, jw, jh, x, y, w, h);
  if (!ok) falhas++;
}
// Home requests a destination, then punches its intersection with the final plane.
static __attribute__((unused)) void cartaoDaHome(const char *nome, int sx, int sy, int sw, int sh,
                         float cx, float cy, float cw, float ch) {
  float x,y,w,h;
  video_tpk_cartao_destino(cx,cy,cw,ch,&x,&y,&w,&h);
  video_janela_fonte(sx,sy,sw,sh,(int)x,(int)y,(int)w,(int)h);
  video_tpk_cartao_furo(cx,cy,cw,ch,&x,&y,&w,&h);
  float left = jx > cx ? jx : cx, top = jy > cy ? jy : cy;
  float right = jx + jw < cx + cw ? jx + jw : cx + cw;
  float bottom = jy + jh < cy + ch ? jy + jh : cy + ch;
  int ok = x == left && y == top && w == right - left && h == bottom - top;
  printf("%s %s: hole %.0f,%.0f %.0fx%.0f\n", ok ? "ok  " : "FALHA", nome, x,y,w,h);
  if (!ok) falhas++;
}
static void cartaoExpandido(void) {
  video_tocar("http://x/expanded-card.m3u8");
  video_tpk_trailer_marcar(1); video_tpk_cartao_marcar(1);
  video_tpk_tizen_definir("6.0");
  nv_tpk_video_evento(6,1920,1080);
  assert(video_tpk_cartao_cover());
  cartaoDaHome("#471 expanded full source: hole equals final plane",0,0,1920,1080,40,300,606,341);
  confere("#471 expanded full source rounds to even height",40,300,606,340);
  cartaoDaHome("#471 expanded delayed crop: hole equals final plane",0,0,1920,1080,40,300,606,341);
  video_recorte_reaplicar();
  confere("#471 expanded delayed crop keeps the final plane",40,300,606,340);
  cartaoDaHome("#471 cover ROI: hole is confined to the card",600,0,720,1080,40,300,360,540);
  confere("#471 cover ROI preserves its final plane",-259,300,960,540);
  video_recorte_reaplicar();
  confere("#471 replay preserves the cover plane",-259,300,960,540);
  video_tpk_cartao_marcar(0); video_tpk_trailer_marcar(0);
  video_tpk_tizen_definir("0");
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
  { VideoStats st;
    nv_tpk_video_evento(7, 62, 0);
    video_estatisticas(&st);
    assert(st.bufferPct == 62 && st.bytes == -1 && st.bufferSeg < 0);
    assert(!strcmp(st.host, "x"));
    nv_tpk_video_evento(7, 100, 0);
  }
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
  // P1: hero/detail/fullscreen trailers keep the raw destination: a 4:3 frame
  // into 1920x1080 stays the full-screen LetterBox path, never a smaller ROI.
  nv_tpk_video_evento(6, 1440, 1080);
  video_tpk_trailer_marcar(1);
  video_janela_fonte(0, 0, 1440, 1080, 0, 0, 1920, 1080);
  confere("P1 non-card trailer keeps the raw fullscreen destination", 0, 0, 1920, 1080);
  video_janela_fonte(0, 0, 1440, 1080, 100, 100, 960, 540);
  confere("P1 non-card trailer keeps the raw destination", 100, 100, 960, 540);
  // P2: portrait cover cannot fit the panel. Never stretch it into the card.
  video_tpk_cartao_marcar(1);
  nv_tpk_video_evento(6, 1080, 1920);
  cartaoDaHome("P2 portrait trailer uses the home fit",0,656,1080,606,400,200,960,540);
  confere("P2 portrait trailer falls back to proportional fit", 728, 200, 304, 540);
  // A panel-contained ROI is not proof of visible video on this family.
  nv_tpk_video_evento(6, 1920, 1080);
  video_tpk_trailer_marcar(1);
  cartaoDaHome("unproven family: plane equals hole",600,0,720,1080,40,300,360,540);
  confere("unproven model family uses letterbox at panel edge", 40, 469, 360, 202);
  cartaoDaHome("unproven family: plane equals hole within panel",600,0,720,1080,800,300,360,540);
  confere("unproven model family uses letterbox even with ROI in panel", 800, 469, 360, 202);
  // #471: Tizen 6.x card trailer COVERS the card by cropping the source (center
  // 720x1080 of 1920x1080 into a 360x540 card = roi 960x540 at x-260; the
  // existing (int)(v+0.5) rounding truncates -259.5 to -259, still covers it).
  video_tpk_tizen_definir("6.0");
  if (!video_tpk_cartao_cover()) { printf("FALHA Tizen 6.0 should cover the card\n"); falhas++; }
  video_janela_fonte(600, 0, 720, 1080, 40, 300, 360, 540);
  confere("#471 Tizen 6 card trailer covers the card", -259, 300, 960, 540);
  // Portrait source cannot cover within the panel even on Tizen 6: fit.
  // Fresh session: the previous card request must not already have cleared cover.
  video_tocar("http://x/portrait.m3u8"); video_tpk_trailer_marcar(1); video_tpk_cartao_marcar(1);
  video_tpk_tizen_definir("6.0");
  nv_tpk_video_evento(6, 1080, 1920);
  video_janela_fonte(0, 656, 1080, 606, 400, 200, 960, 540);
  confere("#471 Tizen 6 portrait falls back to proportional fit", 728, 200, 304, 540);
  if (video_tpk_cartao_cover()) { printf("FALHA portrait fallback must not keep cover\n"); falhas++; }
  { float x,y,w,h; video_tpk_cartao_furo(400,200,960,540,&x,&y,&w,&h);
    int ok = x==728 && y==200 && w==304 && h==540;
    printf("%s #471 card hole follows the 304x540 plane: %.0f,%.0f %.0fx%.0f\n", ok?"ok  ":"FALHA", x,y,w,h);
    if (!ok) falhas++; }
  // Host rejected the ROI: the plane is the fallback fit, so the hole shrinks too.
  video_tocar("http://x/card.m3u8"); video_tpk_trailer_marcar(1); video_tpk_cartao_marcar(1);
  video_tpk_tizen_definir("6.0");
  nv_tpk_video_evento(6, 1920, 1080);
  video_janela_fonte(600, 0, 720, 1080, 40, 300, 360, 540);
  confere("#471 Tizen 6 cover before the host rejects ROI", -259, 300, 960, 540);
  video_tpk_log_host("[aspecto] sessao=1 DisplayRoi rc=0xffffffa1 fallback=ok");
  if (video_tpk_cartao_cover()) { printf("FALHA rejected ROI must clear effective cover\n"); falhas++; }
  nj = 0; cartaoDaHome("#471 rejected ROI: plane equals hole",600,0,720,1080,40,300,360,540);
  confere("#471 rejected ROI uses the proportional fit", 40, 469, 360, 202);
  { float x,y,w,h; video_tpk_cartao_furo(40,300,360,540,&x,&y,&w,&h);
    int ok = x==40 && y==469 && w==360 && h==202;
    printf("%s #471 rejected ROI hole: %.0f,%.0f %.0fx%.0f\n", ok?"ok  ":"FALHA", x,y,w,h);
    if (!ok) falhas++; }
  cartaoDaHome("#471 rejected ROI: whole-frame plane equals hole",0,0,1920,1080,40,300,360,540);
  cartaoDaHome("#471 rejected ROI: delayed crop plane equals hole",0,2,1920,1076,40,300,360,540);
  nv_tpk_video_evento(6, 1920, 1080);
  // Other families keep the letterbox.
  video_tpk_tizen_definir("9.0");
  if (video_tpk_cartao_cover()) { printf("FALHA Tizen 9.0 must not cover the card\n"); falhas++; }
  cartaoDaHome("#471 Tizen 9: plane equals hole",600,0,720,1080,40,300,360,540);
  confere("#471 Tizen 9 card trailer stays letterboxed", 40, 469, 360, 202);
  { const char *familias[] = {"4.0","5.0","7.0","9.0"};
    for (int i=0;i<4;i++) {
      video_tocar("http://x/card.m3u8"); video_tpk_trailer_marcar(1); video_tpk_cartao_marcar(1);
      video_tpk_tizen_definir(familias[i]);
      nv_tpk_video_evento(6,1920,1080);
      printf("#471 default fit Tizen %s\n",familias[i]);
      cartaoDaHome("whole-frame plane equals hole",0,0,1920,1080,40,300,360,540);
      cartaoDaHome("delayed crop plane equals hole",0,2,1920,1076,40,300,360,540);
      confere("fitted card destination stays unchanged",40,469,360,202);
    }
  }
  video_tpk_tizen_definir("0");
  video_tpk_cartao_marcar(0);
  video_tpk_trailer_marcar(0);
#endif
  cartaoExpandido();
  // P1: legacy hosts must also invalidate a cached crop on repositioning.
  nv_tpk_video_evento(6, 1920, 1080);
  video_janela_fonte(480, 270, 960, 540, 400, 300, 640, 360);
  nj = 0; video_recorte_reaplicar();
  assert(nj == 1);
  nj = 0; video_recorte_reaplicar();
  assert(nj == 1);   // replay itself must keep the crop cached
  video_janela(0, 0, 1920, 1080);
  nj = 0; video_recorte_reaplicar();
  assert(nj == 0);
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
