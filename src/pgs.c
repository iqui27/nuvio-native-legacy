#include "pgs.h"
#include <stdlib.h>
#include <string.h>

#define NPAL 32
#define NOBJ 64
#define MAX_PIXELS (1920 * 1080)   // um objeto maior que a tela cheia e lixo
#define MAX_EPOCH_PIXELS ((size_t)MAX_PIXELS * 4) // ate quatro telas de indices por epoca
#define MAX_RLE (4u << 20)

typedef struct { int w, h; uint8_t *idx; } Obj;
typedef struct { int id, x, y, forcado, corta, cx, cy, cw, ch; } Ref;

struct Pgs {
  uint8_t pal[NPAL][256][4];       // Y, Cr, Cb, A como vieram
  Obj obj[NOBJ];
  uint8_t *frag; size_t fragN, fragCap, fragEsp;   // ODS em montagem
  int fragId, fragW, fragH;
  int tw, th, palId, n, temPcs;
  Ref ref[PGS_MAX_OBJ];
};

Pgs *pgs_novo(void) { return calloc(1, sizeof(Pgs)); }

static void limpaEpoca(Pgs *d) {
  int i;
  for (i = 0; i < NOBJ; i++) { free(d->obj[i].idx); d->obj[i].idx = NULL; }
  memset(d->pal, 0, sizeof d->pal);
  d->fragN = 0;
}

void pgs_liberar(Pgs *d) {
  if (!d) return;
  limpaEpoca(d);
  free(d->frag);
  free(d);
}

static int be16(const uint8_t *p) { return p[0] << 8 | p[1]; }

// RLE do PGS: b != 0 e um pixel; 0 seguido de c: c == 0 fim de linha, senao
// bit 7 = "tem cor" e bit 6 = "comprimento em 14 bits".
static uint8_t *rle(const uint8_t *s, size_t n, int w, int h) {
  uint8_t *o = calloc((size_t)w * h, 1);
  size_t i = 0;
  int x = 0, y = 0;
  if (!o) return NULL;
  while (i < n && y < h) {
    int run, cor = 0, c = s[i++];
    if (c) { run = 1; cor = c; }
    else {
      if (i >= n) break;
      c = s[i++];
      if (!c) { x = 0; y++; continue; }
      run = c & 0x3f;
      if (c & 0x40) { if (i >= n) break; run = run << 8 | s[i++]; }
      if (c & 0x80) { if (i >= n) break; cor = s[i++]; }
    }
    if (x + run > w) run = w - x;
    if (cor) memset(o + (size_t)y * w + x, cor, (size_t)run);
    x += run;
  }
  return o;
}

static void ods(Pgs *d, const uint8_t *p, size_t n) {
  int seq;
  if (n < 4) return;
  seq = p[3];
  if (seq & 0x80) {
    size_t tot;
    if (n < 11) return;
    tot = (size_t)p[4] << 16 | (size_t)p[5] << 8 | p[6];
    d->fragW = be16(p + 7); d->fragH = be16(p + 9);
    d->fragId = be16(p);
    d->fragN = 0;
    if (tot < 4 || tot - 4 > MAX_RLE || d->fragW <= 0 || d->fragH <= 0 ||
        (size_t)d->fragW * d->fragH > MAX_PIXELS || d->fragId >= NOBJ) { d->fragEsp = 0; return; }
    d->fragEsp = tot - 4;
    if (d->fragEsp > d->fragCap) {
      uint8_t *g = realloc(d->frag, d->fragEsp);
      if (!g) { d->fragEsp = 0; return; }
      d->frag = g; d->fragCap = d->fragEsp;
    }
    p += 11; n -= 11;
  } else {
    if (!d->fragEsp || be16(p) != d->fragId) return;
    p += 4; n -= 4;
  }
  if (!d->fragEsp) return;
  if (n > d->fragEsp - d->fragN) n = d->fragEsp - d->fragN;
  memcpy(d->frag + d->fragN, p, n);
  d->fragN += n;
  if (seq & 0x40) {
    Obj *o = &d->obj[d->fragId];
    size_t pixels = (size_t)d->fragW * d->fragH;
    int i;
    free(o->idx);
    o->idx = NULL;
    for (i = 0; i < NOBJ; i++)
      if (d->obj[i].idx) pixels += (size_t)d->obj[i].w * d->obj[i].h;
    if (pixels <= MAX_EPOCH_PIXELS && d->fragN == d->fragEsp)
      o->idx = rle(d->frag, d->fragN, d->fragW, d->fragH);
    o->w = d->fragW; o->h = d->fragH;
    d->fragEsp = 0;
  }
}

static void pds(Pgs *d, const uint8_t *p, size_t n) {
  size_t i;
  if (n < 2 || p[0] >= NPAL) return;
  for (i = 2; i + 5 <= n; i += 5) memcpy(d->pal[p[0]][p[i]], p + i + 1, 4);
}

static void pcs(Pgs *d, const uint8_t *p, size_t n) {
  int estado, nobj, i, atualiza;
  size_t o = 11;
  if (n < 11) return;
  d->tw = be16(p); d->th = be16(p + 2);
  estado = p[7] & 0xc0;
  atualiza = p[8] & 0x80;
  nobj = p[10];
  if (estado == 0x80) limpaEpoca(d);
  d->palId = p[9];
  d->temPcs = 1;
  if (atualiza) return;          // so troca a paleta: os objetos seguem os mesmos
  d->n = 0;
  for (i = 0; i < nobj && i < PGS_MAX_OBJ; i++) {
    Ref *r = &d->ref[d->n];
    if (o + 8 > n) return;
    r->id = be16(p + o); r->forcado = (p[o + 3] & 0x40) != 0; r->corta = (p[o + 3] & 0x80) != 0;
    r->x = be16(p + o + 4); r->y = be16(p + o + 6);
    o += 8;
    if (r->corta) {
      if (o + 8 > n) return;
      r->cx = be16(p + o); r->cy = be16(p + o + 2);
      r->cw = be16(p + o + 4); r->ch = be16(p + o + 6);
      o += 8;
    }
    d->n++;
  }
}

static uint8_t sat(float v) { return v <= 0 ? 0 : v >= 255 ? 255 : (uint8_t)(v + 0.5f); }

// Paleta YCrCb limitada -> RGBA. BT.709 de 720 linhas pra cima, BT.601 abaixo.
static void paleta(const uint8_t (*src)[4], int hd, uint8_t (*dst)[4]) {
  int i;
  for (i = 0; i < 256; i++) {
    float y = 1.164383f * (src[i][0] - 16), cr = src[i][1] - 128.0f, cb = src[i][2] - 128.0f;
    if (hd) {
      dst[i][0] = sat(y + 1.792741f * cr);
      dst[i][1] = sat(y - 0.213249f * cb - 0.532909f * cr);
      dst[i][2] = sat(y + 2.112402f * cb);
    } else {
      dst[i][0] = sat(y + 1.596027f * cr);
      dst[i][1] = sat(y - 0.391762f * cb - 0.812968f * cr);
      dst[i][2] = sat(y + 2.017232f * cb);
    }
    dst[i][3] = src[i][3];
    if (!dst[i][3]) dst[i][0] = dst[i][1] = dst[i][2] = 0;
  }
}

static int monta(Pgs *d, PgsQuadro *q) {
  uint8_t cor[256][4];
  int i;
  q->tela_w = d->tw; q->tela_h = d->th; q->n = 0;
  paleta(d->pal[d->palId < NPAL ? d->palId : 0], d->th >= 720, cor);
  for (i = 0; i < d->n; i++) {
    const Ref *r = &d->ref[i];
    const Obj *o = r->id < NOBJ ? &d->obj[r->id] : NULL;
    PgsObjeto *s = &q->obj[q->n];
    int cx = 0, cy = 0, cw, ch, x, y;
    if (!o || !o->idx) continue;
    cw = o->w; ch = o->h;
    if (r->corta) {
      cx = r->cx; cy = r->cy; cw = r->cw; ch = r->ch;
      if (cx < 0 || cy < 0 || cx >= o->w || cy >= o->h) continue;
      if (cw > o->w - cx) cw = o->w - cx;
      if (ch > o->h - cy) ch = o->h - cy;
    }
    if (cw <= 0 || ch <= 0) continue;
    s->rgba = malloc((size_t)cw * ch * 4);
    if (!s->rgba) continue;
    for (y = 0; y < ch; y++)
      for (x = 0; x < cw; x++)
        memcpy(s->rgba + 4 * ((size_t)y * cw + x), cor[o->idx[(size_t)(cy + y) * o->w + cx + x]], 4);
    s->x = r->x; s->y = r->y; s->w = cw; s->h = ch; s->forcado = r->forcado;
    q->n++;
  }
  return d->n == 0 || q->n > 0;   // havia objeto e nenhum chegou: nao e "limpar"
}

int pgs_decodificar(Pgs *d, const uint8_t *buf, size_t n, PgsQuadro *q) {
  size_t i = 0;
  memset(q, 0, sizeof *q);
  if (!d) return 0;
  d->temPcs = 0;
  while (i + 3 <= n) {
    int tipo = buf[i];
    size_t len = (size_t)be16(buf + i + 1);
    const uint8_t *p = buf + i + 3;
    if (len > n - i - 3) break;               // truncado
    if (tipo == 0x16) pcs(d, p, len);
    else if (tipo == 0x14) pds(d, p, len);
    else if (tipo == 0x15) ods(d, p, len);
    else if (tipo == 0x80) {
      if (!d->temPcs) return 0;
      d->temPcs = 0;
      if (monta(d, q)) return 1;
      pgs_quadro_liberar(q);
      return 0;
    }
    i += 3 + len;
  }
  // Bloco sem END: a epoca que ele abriu esta incompleta; nao a deixa viver.
  if (d->temPcs) { limpaEpoca(d); d->temPcs = 0; }
  return 0;
}

void pgs_quadro_liberar(PgsQuadro *q) {
  int i;
  for (i = 0; i < PGS_MAX_OBJ; i++) { free(q->obj[i].rgba); q->obj[i].rgba = NULL; }
  q->n = 0;
}
