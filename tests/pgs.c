// Decodificador PGS (legenda de imagem do Blu-ray em S_HDMV/PGS). O fixture e
// montado aqui, byte a byte: um display set com 1 objeto 6x2 em duas fatias de
// ODS, outro que so limpa a tela, e um truncado que nao pode derrubar nada.
#include "pgs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

typedef struct { uint8_t b[512]; size_t n; } Buf;
static void u8(Buf *o, int v) { o->b[o->n++] = (uint8_t)v; }
static void u16(Buf *o, int v) { u8(o, v >> 8); u8(o, v); }
static void seg(Buf *o, int tipo, const uint8_t *d, size_t n) {
  u8(o, tipo); u16(o, (int)n); memcpy(o->b + o->n, d, n); o->n += n;
}

// PCS: tela 1920x1080, estado de composicao `estado`, `n` objetos (0 ou 1).
static void pcs(Buf *o, int estado, int n) {
  Buf p = {{0}, 0};
  u16(&p, 1920); u16(&p, 1080); u8(&p, 0x10); u16(&p, 1); u8(&p, estado);
  u8(&p, 0); u8(&p, 0); u8(&p, n);
  if (n) { u16(&p, 7); u8(&p, 0); u8(&p, 0); u16(&p, 100); u16(&p, 900); }
  seg(o, 0x16, p.b, p.n);
}
static void pds(Buf *o) {
  Buf p = {{0}, 0};
  u8(&p, 0); u8(&p, 0);
  u8(&p, 1); u8(&p, 235); u8(&p, 128); u8(&p, 128); u8(&p, 255);  // branco opaco
  u8(&p, 2); u8(&p, 16); u8(&p, 128); u8(&p, 128); u8(&p, 128);   // preto, alfa 128
  seg(o, 0x14, p.b, p.n);
}
// RLE 6x2: linha 0 = c1, c2 c2 c2, c0 c0; linha 1 = c0 c0 c0, c1 c1 c1.
static const uint8_t RLE[] = {
  0x01, 0x00, 0x83, 0x02, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x40, 0x03, 0x00, 0xC0, 0x03, 0x01, 0x00, 0x00,
};
static void ods(Buf *o, int fatiar) {
  size_t cab = 4 + sizeof RLE, corte = fatiar ? 5 : sizeof RLE;
  Buf p = {{0}, 0};
  u16(&p, 7); u8(&p, 0); u8(&p, fatiar ? 0x80 : 0xC0);
  u8(&p, 0); u16(&p, (int)cab); u16(&p, 6); u16(&p, 2);
  memcpy(p.b + p.n, RLE, corte); p.n += corte;
  seg(o, 0x15, p.b, p.n);
  if (!fatiar) return;
  p.n = 0; u16(&p, 7); u8(&p, 0); u8(&p, 0x40);
  memcpy(p.b + p.n, RLE + corte, sizeof RLE - corte); p.n += sizeof RLE - corte;
  seg(o, 0x15, p.b, p.n);
}

static void confere(const PgsQuadro *q, int fatiado) {
  static const uint8_t esp[2][6][4] = {
    {{255,255,255,255},{0,0,0,128},{0,0,0,128},{0,0,0,128},{0,0,0,0},{0,0,0,0}},
    {{0,0,0,0},{0,0,0,0},{0,0,0,0},{255,255,255,255},{255,255,255,255},{255,255,255,255}},
  };
  int i;
  CHECA(q->n == 1);
  if (q->n != 1) return;
  CHECA(q->tela_w == 1920 && q->tela_h == 1080);
  CHECA(q->obj[0].x == 100 && q->obj[0].y == 900);
  CHECA(q->obj[0].w == 6 && q->obj[0].h == 2);
  if (!q->obj[0].rgba) { CHECA(0); return; }
  for (i = 0; i < 12; i++)
    CHECA(!memcmp(q->obj[0].rgba + 4 * i, esp[i / 6][i % 6], 4));
  if (fatiado) puts("  ods em duas fatias ok");
}

int main(void) {
  Pgs *d = pgs_novo();
  PgsQuadro q;
  Buf b;
  int r;
  // 1) display set inteiro, ODS de uma fatia
  b.n = 0; pcs(&b, 0x80, 1); pds(&b); ods(&b, 0); seg(&b, 0x80, NULL, 0);
  r = pgs_decodificar(d, b.b, b.n, &q);
  CHECA(r == 1);
  confere(&q, 0);
  pgs_quadro_liberar(&q);
  // 2) o mesmo, ODS em duas fatias
  b.n = 0; pcs(&b, 0x80, 1); pds(&b); ods(&b, 1); seg(&b, 0x80, NULL, 0);
  r = pgs_decodificar(d, b.b, b.n, &q);
  CHECA(r == 1);
  confere(&q, 1);
  pgs_quadro_liberar(&q);
  // Flags PCS conforme pgssubdec: 0x40 forced, 0x80 crop.
  for (int flags = 0x40; flags <= 0xc0; flags += 0x40) {
    b.n = 0; pcs(&b, 0x80, 1);
    b.b[17] = (uint8_t)flags;
    if (flags & 0x80) {
      Buf crop = {{0}, 0};
      u16(&crop, 1); u16(&crop, 0); u16(&crop, 2); u16(&crop, 1);
      memcpy(b.b + b.n, crop.b, crop.n); b.n += crop.n;
      b.b[2] += (uint8_t)crop.n;
    }
    pds(&b); ods(&b, 0); seg(&b, 0x80, NULL, 0);
    r = pgs_decodificar(d, b.b, b.n, &q);
    CHECA(r == 1 && q.n == 1);
    if (q.n == 1) {
      CHECA(q.obj[0].forcado == !!(flags & 0x40));
      CHECA(q.obj[0].w == ((flags & 0x80) ? 2 : 6));
      CHECA(q.obj[0].h == ((flags & 0x80) ? 1 : 2));
    }
    pgs_quadro_liberar(&q);
  }
  // ODS minimo nao pode reservar 64 telas; substituicao e nova epoca liberam teto.
  for (int k = 0; k < 7; k++) {
    int id = k == 5 ? 1 : k == 6 ? 5 : k + 1;
    uint8_t grande[] = {0, (uint8_t)id, 0, 0xc0, 0, 0, 5, 7, 128, 4, 56, 1};
    b.n = 0; pcs(&b, k == 0 || k == 6 ? 0x80 : 0, 1);
    b.b[15] = (uint8_t)id;
    seg(&b, 0x15, grande, sizeof grande); seg(&b, 0x80, NULL, 0);
    r = pgs_decodificar(d, b.b, b.n, &q);
    CHECA(r == (k == 4 ? 0 : 1));
    pgs_quadro_liberar(&q);
  }
  // Dimensoes 65535x65535 sao recusadas tambem em size_t de 32 bits.
  {
    uint8_t enorme[] = {0, 7, 0, 0xc0, 0, 0, 5, 255, 255, 255, 255, 1};
    b.n = 0; pcs(&b, 0x80, 1);
    seg(&b, 0x15, enorme, sizeof enorme); seg(&b, 0x80, NULL, 0);
    CHECA(pgs_decodificar(d, b.b, b.n, &q) == 0);
    pgs_quadro_liberar(&q);
  }
  // 3) composicao sem objetos limpa a tela: quadro valido com n == 0
  b.n = 0; pcs(&b, 0x00, 0); seg(&b, 0x80, NULL, 0);
  r = pgs_decodificar(d, b.b, b.n, &q);
  CHECA(r == 1 && q.n == 0);
  pgs_quadro_liberar(&q);
  // 3b) bloco cortado antes do END, no inicio de uma epoca: o objeto que ele
  // guardou nao pode sobreviver para um display set seguinte.
  { Pgs *e = pgs_novo();
    b.n = 0; pcs(&b, 0x80, 1); pds(&b); ods(&b, 0);
    r = pgs_decodificar(e, b.b, b.n, &q);
    CHECA(r == 0);
    b.n = 0; pcs(&b, 0x00, 1); seg(&b, 0x80, NULL, 0);
    r = pgs_decodificar(e, b.b, b.n, &q);
    CHECA(r == 0);
    if (r == 1) pgs_quadro_liberar(&q);
    pgs_liberar(e); }
  // 4) truncado em qualquer ponto: nunca crasha, nunca devolve objeto torto
  b.n = 0; pcs(&b, 0x80, 1); pds(&b); ods(&b, 0); seg(&b, 0x80, NULL, 0);
  for (size_t corte = 0; corte < b.n; corte++) {
    r = pgs_decodificar(d, b.b, corte, &q);
    if (r == 1) { CHECA(q.n == 0 || q.obj[0].rgba); pgs_quadro_liberar(&q); }
  }
  // 5) lixo
  { uint8_t lixo[64]; size_t i; for (i = 0; i < sizeof lixo; i++) lixo[i] = (uint8_t)(i * 37 + 5);
    r = pgs_decodificar(d, lixo, sizeof lixo, &q);
    if (r == 1) pgs_quadro_liberar(&q); }
  pgs_liberar(d);
  if (falhas) { printf("pgs: %d falha(s)\n", falhas); return 1; }
  puts("pgs: ok");
  return 0;
}
