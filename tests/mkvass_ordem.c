// The order the reader fetches subtitle points in after a jump (owner 16/11,
// Love Is War S3E02: after jumps to 1259-1320 s lines came up to 2.3 s late on
// TorBox's CDN). The window is [playhead - 8 s, playhead + ahead]; what is on
// screen now (from 3 s before the playhead) must come before the older part.
// Includes the module, like mkvass_limites.c: no threads, network or disk.
#include "../src/mkvass.c"

static int falhas;
static void conferir(int ok, const char *caso) {
  printf("%s: %s\n", ok ? "ok" : "FALHA", caso);
  if (!ok) falhas++;
}

int main(void) {
  static Fio f;
  static Ponto p[40];
  int i, noJ, k;
  memset(&f, 0, sizeof f);
  f.escala = 1000000UL;            // 1 unit = 1 ms
  f.pontos = p; f.nPontos = 40; f.folgaJan = -1.0;
  // One point every second from 1290 s to 1329 s, all text blocks (rel >= 0).
  for (i = 0; i < 40; i++) { p[i].tempo = (unsigned long)(1290 + i) * 1000UL; p[i].rel = 10; p[i].cluster = 1000 + i; }

  // Jump to 1305 s: window 1297..1395 s.
  k = proximoPendente(&f, 1305.0 - MKVASS_ATRAS_SEG, 1305.0 + 90.0, &noJ);
  conferir(k >= 0 && noJ && segundosDe(&f, p[k].tempo) == 1302.0,
           "after a jump the first point is 3 s before the playhead (1302 s), not 8 s (1297 s)");

  // Everything from 1302 s on fetched: the older part [1297, 1302) comes next.
  for (i = 0; i < 40; i++) if (segundosDe(&f, p[i].tempo) >= 1302.0) p[i].colhido = 1;
  k = proximoPendente(&f, 1305.0 - MKVASS_ATRAS_SEG, 1305.0 + 90.0, &noJ);
  conferir(k >= 0 && noJ && segundosDe(&f, p[k].tempo) == 1297.0,
           "then the older part of the window, from its start (1297 s)");

  // All of the window fetched: nothing in it (the window case stops there).
  for (i = 0; i < 40; i++) p[i].colhido = 1;
  f.janela = 1;
  k = proximoPendente(&f, 1305.0 - MKVASS_ATRAS_SEG, 1305.0 + 90.0, &noJ);
  conferir(k < 0, "window fully fetched: no point left");

  // Body in window mode, a point before the window only: not offered.
  p[0].colhido = 0;
  k = proximoPendente(&f, 1305.0 - MKVASS_ATRAS_SEG, 1305.0 + 90.0, &noJ);
  conferir(k < 0, "window mode: a point behind the window waits");

  // Not in window mode: outside points still come (ahead first, else the nearest behind).
  f.janela = 0;
  k = proximoPendente(&f, 1305.0 - MKVASS_ATRAS_SEG, 1305.0 + 90.0, &noJ);
  conferir(k == 0 && !noJ, "whole-track mode: the point behind the window comes after it");

  // Playing from the start (no jump): the first pending point is the earliest.
  for (i = 0; i < 40; i++) p[i].colhido = 0;
  k = proximoPendente(&f, 1290.0 - MKVASS_ATRAS_SEG, 1290.0 + 90.0, &noJ);
  conferir(k == 0 && noJ, "at the window's start: the earliest point first");

  printf(falhas ? "mkvass_ordem: FAIL\n" : "mkvass_ordem: ok\n");
  return falhas ? 1 : 0;
}
