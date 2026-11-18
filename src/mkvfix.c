// Ver mkvfix.h para o porque.
#include "mkvfix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_EBML     0x1A45DFA3u
#define ID_SEGMENT  0x18538067u
#define ID_SEEKHEAD 0x114D9B74u
#define ID_SEEK     0x4DBBu
#define ID_SEEKID   0x53ABu
#define ID_SEEKPOS  0x53ACu
#define ID_VOID     0xECu
#define ID_CUES     0x1C53BB6Bu

// ---- EBML ------------------------------------------------------------------
static int largura(unsigned char b) { int w = 1; unsigned char m = 0x80; while (w <= 8 && !(b & m)) { m >>= 1; w++; } return w; }
static uint32_t lerId(const unsigned char *p, long resta, int *usou) {
  int w; uint32_t v = 0;
  if (resta < 1) return 0;
  w = largura(p[0]);
  if (w > 4 || w > resta) return 0;
  for (int i = 0; i < w; i++) v = (v << 8) | p[i];
  *usou = w; return v;
}
// -1 = tamanho desconhecido, -2 = invalido.
static int64_t lerTam(const unsigned char *p, long resta, int *usou) {
  int w, todos = 1; uint64_t v;
  if (resta < 1) return -2;
  w = largura(p[0]);
  if (w > 8 || w > resta) return -2;
  v = p[0] & (0xFF >> w);
  if (v != (uint64_t)(0xFFu >> w)) todos = 0;
  for (int i = 1; i < w; i++) { v = (v << 8) | p[i]; if (p[i] != 0xFF) todos = 0; }
  *usou = w;
  if (todos) return -1;
  return v > (uint64_t)INT64_MAX ? -2 : (int64_t)v;
}
static uint64_t lerUint(const unsigned char *p, int64_t n) {
  uint64_t v = 0;
  for (int64_t i = 0; i < n && i < 8; i++) v = (v << 8) | p[i];
  return v;
}

int mkvfix_ler_seekhead(const unsigned char *b, long n, uint32_t *id, int64_t *pos, int max) {
  int ui = 0, us = 0, k = 0;
  int64_t s, o;
  if (lerId(b, n, &ui) != ID_SEEKHEAD) return -1;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return -1;
  b += ui + us; o = 0;
  while (o < s) {
    int a = 0, c = 0;
    uint32_t i2 = lerId(b + o, (long)(s - o), &a);
    int64_t s2;
    if (!i2) break;
    s2 = lerTam(b + o + a, (long)(s - o - a), &c);
    if (s2 < 0 || o + a + c + s2 > s) break;
    if (i2 == ID_SEEK && k < max) {
      const unsigned char *v = b + o + a + c;
      int64_t q = 0, p = -1; uint32_t alvo = 0;
      while (q < s2) {
        int x = 0, y = 0;
        uint32_t i3 = lerId(v + q, (long)(s2 - q), &x);
        int64_t s3;
        if (!i3) break;
        s3 = lerTam(v + q + x, (long)(s2 - q - x), &y);
        if (s3 < 0 || q + x + y + s3 > s2) break;
        if (i3 == ID_SEEKID) alvo = (uint32_t)lerUint(v + q + x + y, s3);
        else if (i3 == ID_SEEKPOS && s3 <= 8) p = (int64_t)lerUint(v + q + x + y, s3);
        q += x + y + s3;
      }
      if (alvo && p >= 0) { id[k] = alvo; pos[k] = p; k++; }
    }
    o += a + c + s2;
  }
  return k;
}

int mkvfix_ler_inicio(const unsigned char *b, long n, MkvfixCab *c) {
  int ui = 0, us = 0;
  int64_t s, o;
  memset(c, 0, sizeof *c);
  c->regiao = -1;
  if (lerId(b, n, &ui) != ID_EBML) return 0;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return 0;
  o = ui + us + s;
  if (lerId(b + o, (long)(n - o), &ui) != ID_SEGMENT) return 0;
  s = lerTam(b + o + ui, (long)(n - o - ui), &us);
  if (s == -2) return 0;
  c->segIni = o + ui + us;
  o = c->segIni;
  if (lerId(b + o, (long)(n - o), &ui) == ID_SEEKHEAD) {
    s = lerTam(b + o + ui, (long)(n - o - ui), &us);
    if (s < 0 || o + ui + us + s > n) return 1;
    c->n = mkvfix_ler_seekhead(b + o, (long)(n - o), c->id, c->pos, MKVFIX_ENTRADAS);
    if (c->n < 0) { c->n = 0; return 1; }
    c->regiao = o;
    c->regiaoN = (int)(ui + us + s);
    o += ui + us + s;
    // O Void logo atras e o espaco que o muxer reservou para o SeekHead crescer.
    if (lerId(b + o, (long)(n - o), &ui) == ID_VOID) {
      s = lerTam(b + o + ui, (long)(n - o - ui), &us);
      if (s >= 0 && ui + us + s + c->regiaoN <= 65536) c->regiaoN += (int)(ui + us + s);
    }
  }
  return 1;
}

int64_t mkvfix_entrada(const uint32_t *id, const int64_t *pos, int n, uint32_t alvo) {
  for (int i = 0; i < n; i++) if (id[i] == alvo) return pos[i];
  return -1;
}

// Largura minima do tamanho EBML `x` (todos os bits 1 e reservado).
static int larguraTam(uint64_t x) {
  int w = 1;
  while (w < 8 && x >= ((uint64_t)1 << (7 * w)) - 1) w++;
  return w;
}
static void gravarTam(unsigned char *o, uint64_t x, int w) {
  uint64_t v = x | ((uint64_t)1 << (7 * w));
  for (int i = w - 1; i >= 0; i--) { o[i] = (unsigned char)(v & 0xFF); v >>= 8; }
}
static int bytesDe(uint64_t v) { int k = 1; while (k < 8 && (v >> (8 * k))) k++; return k; }

static int entradaMontar(uint32_t id, int64_t pos, unsigned char *e) {
  int ni = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1;
  int np = bytesDe((uint64_t)pos), k = 0, dentro = 3 + ni + 3 + np;
  e[k++] = 0x4D; e[k++] = 0xBB; e[k++] = (unsigned char)(0x80 | dentro);
  e[k++] = 0x53; e[k++] = 0xAB; e[k++] = (unsigned char)(0x80 | ni);
  for (int i = ni - 1; i >= 0; i--) e[k++] = (unsigned char)(id >> (8 * i));
  e[k++] = 0x53; e[k++] = 0xAC; e[k++] = (unsigned char)(0x80 | np);
  for (int i = np - 1; i >= 0; i--) e[k++] = (unsigned char)((uint64_t)pos >> (8 * i));
  return k;
}

// O resto `r` (bytes depois do SeekHead) tem forma: 0, ou um Void de r bytes
// (r >= 2). r == 1 se resolve alargando o tamanho do SeekHead.
static int restoOk(int r, int w) { return r == 0 || r >= 2 || (r == 1 && w < 8); }

int mkvfix_montar(const uint32_t *id, const int64_t *pos, int n, unsigned char *out, int len) {
  unsigned char cont[512], e[32];
  int cn = 0, entrou = 0, w, r, k;
  uint32_t usados[MKVFIX_ENTRADAS * 2]; int nu = 0;
  if (len < 4 + 1 + 21 || len > 65536) return 0;
  for (int i = 0; i < n; i++) {
    int el, dup = 0, t;
    if (pos[i] < 0) continue;
    for (int j = 0; j < nu; j++) if (usados[j] == id[i]) dup = 1;
    if (dup || cn + 32 > (int)sizeof cont) continue;
    el = entradaMontar(id[i], pos[i], e);
    t = cn + el;
    w = larguraTam((uint64_t)t);
    r = len - (4 + w + t);
    if (r < 0 || !restoOk(r, w)) continue;
    memcpy(cont + cn, e, (size_t)el); cn = t; entrou++;
    if (nu < (int)(sizeof usados / sizeof usados[0])) usados[nu++] = id[i];
  }
  if (!entrou) return 0;
  w = larguraTam((uint64_t)cn);
  r = len - (4 + w + cn);
  if (r == 1) { w++; r = 0; }
  k = 0;
  out[k++] = 0x11; out[k++] = 0x4D; out[k++] = 0x9B; out[k++] = 0x74;
  gravarTam(out + k, (uint64_t)cn, w); k += w;
  memcpy(out + k, cont, (size_t)cn); k += cn;
  if (r >= 2) {
    int v = 1, d = r - 2;
    if (d > 126) { for (v = 2; v <= 8; v++) { d = r - 1 - v; if (d >= 0 && larguraTam((uint64_t)d) <= v) break; } }
    out[k++] = 0xEC;
    gravarTam(out + k, (uint64_t)d, v); k += v;
    memset(out + k, 0, (size_t)d); k += d;
  }
  return k == len ? entrou : 0;
}

// ---- legendas de texto escondidas da TV (ver mkvfix.h) ----------------------
#define ID_TRACKS     0x1654AE6Bu
#define ID_TRACKENTRY 0xAEu
#define ID_TRACKTYPE  0x83u
#define ID_CODECID    0x86u
#define ID_TRACKNUMBER 0xD7u

static int codecTexto(const char *c) {
  return !strncmp(c, "S_TEXT/", 7) || !strcmp(c, "S_ASS") || !strcmp(c, "S_SSA");
}
static int codecAss(const char *c) {
  return !strcmp(c, "S_TEXT/ASS") || !strcmp(c, "S_TEXT/SSA") || !strcmp(c, "S_ASS") || !strcmp(c, "S_SSA");
}

int mkvfix_legendas_texto(const unsigned char *b, long n, int64_t *off, int max) {
  return mkvfix_legendas_texto_num(b, n, off, NULL, max);
}

int mkvfix_legendas_texto_num(const unsigned char *b, long n, int64_t *off, uint64_t *num, int max) {
  int ui = 0, us = 0, k = 0, ass = 0;
  int64_t s, o, ini;
  if (lerId(b, n, &ui) != ID_TRACKS) return -1;
  s = lerTam(b + ui, n - ui, &us);
  if (s < 0 || ui + us + s > n) return -1;
  ini = ui + us; o = 0;
  while (o < s) {
    int a = 0, c = 0;
    uint32_t id = lerId(b + ini + o, (long)(s - o), &a);
    int64_t s2;
    if (!id) return -1;
    s2 = lerTam(b + ini + o + a, (long)(s - o - a), &c);
    if (s2 < 0 || o + a + c + s2 > s) return -1;
    if (id == ID_TRACKENTRY) {
      const unsigned char *v = b + ini + o + a + c;
      int64_t q = 0, tipoEm = -1; uint64_t tipo = 0, numero = 0; char codec[40] = "";
      while (q < s2) {
        int x = 0, y = 0;
        uint32_t i3 = lerId(v + q, (long)(s2 - q), &x);
        int64_t s3;
        if (!i3) break;
        s3 = lerTam(v + q + x, (long)(s2 - q - x), &y);
        if (s3 < 0 || q + x + y + s3 > s2) break;
        if (i3 == ID_TRACKTYPE && s3 <= 8) {
          tipo = lerUint(v + q + x + y, s3);
          tipoEm = s3 == 1 ? (int64_t)(v - b) + q + x + y : -1;   // so o valor de 1 byte se troca
        }
        else if (i3 == ID_TRACKNUMBER && s3 >= 1 && s3 <= 8) numero = lerUint(v + q + x + y, s3);
        else if (i3 == ID_CODECID && s3 < (int64_t)sizeof codec) {
          memcpy(codec, v + q + x + y, (size_t)s3); codec[s3] = 0;
        }
        q += x + y + s3;
      }
      // TrackType 0x11 = legenda, num byte: vira 0x20 sem mexer em tamanho.
      if (tipo == MKVFIX_TIPO_LEGENDA && codecTexto(codec)) {
        if (codecAss(codec)) ass = 1;
        if (tipoEm >= 0 && k < max) { if (num) num[k] = numero; off[k++] = tipoEm; }
      }
    }
    o += a + c + s2;
  }
  return ass ? k : 0;
}

int64_t mkvfix_tracks_pos(const unsigned char *b, long n, const MkvfixCab *c) {
  int64_t p = mkvfix_entrada(c->id, c->pos, c->n, ID_TRACKS), o;
  if (p >= 0) return c->segIni + p;
  // Sem entrada no SeekHead: os elementos do topo, em ordem, dentro do trecho.
  o = c->segIni;
  while (o < n) {
    int ui = 0, us = 0;
    uint32_t id = lerId(b + o, (long)(n - o), &ui);
    int64_t s;
    if (!id) return -1;
    if (id == ID_TRACKS) return o;
    if (id == 0x1F43B675u) return -1;   // Cluster: os dados comecaram, nao ha Tracks antes
    s = lerTam(b + o + ui, (long)(n - o - ui), &us);
    if (s < 0) return -1;
    o += ui + us + s;
  }
  return -1;
}

// ---- Void walker (see mkvfix.h, 16/11/2026) ---------------------------------
#define ID_CLUSTER   0x1F43B675u
#define ID_TIMESTAMP 0xE7u
#define ID_BLOCKGROUP 0xA0u
#define ID_BLOCK      0xA1u
#define ID_SIMPLEBLOCK 0xA3u
enum { MV_SCAN, MV_CLCAB, MV_TOPO, MV_FILHO, MV_PULA };

static int mvNivel1(uint32_t id) {
  return id == ID_SEEKHEAD || id == 0x1549A966u || id == ID_TRACKS || id == 0x1043A770u ||
         id == ID_CUES || id == 0x1941A469u || id == 0x1254C367u || id == ID_VOID || id == 0xBFu;
}

void mkvfix_void_iniciar(MkvfixVoid *v, const uint64_t *faixas, int n, int64_t pos) {
  memset(v, 0, sizeof *v);
  if (n > MKVFIX_OCULTAS) n = MKVFIX_OCULTAS;
  for (int i = 0; i < n; i++) v->faixa[i] = faixas[i];
  v->nf = n > 0 ? n : 0;
  v->pos = pos;
  v->segFim = -1;
  v->st = pos == 0 ? MV_TOPO : MV_SCAN;
}

// ID + size in h[0..n): 0 = needs more bytes, -1 = not EBML, else the header
// length (*tam = -1: unknown size).
static int mvCab(const unsigned char *h, int n, uint32_t *id, int64_t *tam) {
  int wi, ws, todos = 1;
  uint64_t t;
  if (n < 1) return 0;
  wi = largura(h[0]);
  if (wi > 4) return -1;
  if (n < wi + 1) return 0;
  ws = largura(h[wi]);
  if (ws > 8) return -1;
  if (n < wi + ws) return 0;
  *id = 0;
  for (int i = 0; i < wi; i++) *id = (*id << 8) | h[i];
  t = h[wi] & (0xFFu >> ws);
  if (t != (uint64_t)(0xFFu >> ws)) todos = 0;
  for (int i = 1; i < ws; i++) { t = (t << 8) | h[wi + i]; if (h[wi + i] != 0xFF) todos = 0; }
  *tam = todos ? -1 : (int64_t)t;
  return wi + ws;
}
// A vint (track number): 0 = needs more, -1 = invalid, else its width.
static int mvVint(const unsigned char *h, int n, uint64_t *v) {
  int w;
  if (n < 1) return 0;
  w = largura(h[0]);
  if (w > 8) return -1;
  if (n < w) return 0;
  *v = h[0] & (0xFFu >> w);
  for (int i = 1; i < w; i++) *v = (*v << 8) | h[i];
  return w;
}
static int mvAlvo(const MkvfixVoid *v, uint64_t f) {
  for (int i = 0; i < v->nf; i++) if (v->faixa[i] == f) return 1;
  return 0;
}
static void mvPerdeu(MkvfixVoid *v) { v->perdas++; v->st = MV_SCAN; v->win = 0; v->cvN = 0; }

// The child whose header is in hb: 1 = decided (void or not, in *anular),
// 0 = needs more bytes, -1 = not followable (sent as is; *fim may be -1).
static int mvDecidir(MkvfixVoid *v, int64_t ini, int *anular, int64_t *fim) {
  uint32_t id = 0;
  int64_t tam = 0;
  uint64_t f = 0;
  int r = mvCab(v->hb, v->hbN, &id, &tam), w;
  *anular = 0; *fim = -1;
  if (r == 0) return 0;
  if (r < 0 || tam < 0) return -1;
  *fim = ini + r + tam;
  if (*fim > v->clFim) { *fim = -1; return -1; }
  if (id == ID_SIMPLEBLOCK) {
    w = mvVint(v->hb + r, v->hbN - r, &f);
    if (w == 0) return v->pos >= *fim ? 1 : 0;
    if (w > 0) *anular = mvAlvo(v, f);
    return 1;
  }
  if (id == ID_BLOCKGROUP) {
    int o = r;
    while (ini + o < *fim) {
      uint32_t i2 = 0; int64_t t2 = 0;
      int r2 = mvCab(v->hb + o, v->hbN - o, &i2, &t2);
      if (r2 == 0) return v->pos >= *fim ? 1 : 0;
      if (r2 < 0 || t2 < 0 || ini + o + r2 + t2 > *fim) return 1;   // odd group: left as is
      if (i2 == ID_BLOCK) {
        w = mvVint(v->hb + o + r2, v->hbN - o - r2, &f);
        if (w == 0) return v->pos >= *fim ? 1 : 0;
        if (w > 0) *anular = mvAlvo(v, f);
        return 1;
      }
      if ((int64_t)v->hbN < o + r2 + t2) return v->pos >= *fim ? 1 : 0;   // the small child before Block
      o += r2 + (int)t2;
    }
    return 1;
  }
  return 1;   // any other child: nothing to change
}

long mkvfix_void_passar(MkvfixVoid *v, const unsigned char *in, long n, unsigned char *out) {
  long i = 0, o = 0;
  while (i < n) {
    switch (v->st) {
    case MV_PULA: {
      int64_t k = v->ate - v->pos;
      if (k > n - i) k = n - i;
      memcpy(out + o, in + i, (size_t)k);
      o += (long)k; i += (long)k; v->pos += k;
      if (v->pos >= v->ate) v->st = v->ret;
      break; }
    case MV_SCAN:
      while (i < n) {
        unsigned char c = in[i++];
        out[o++] = c; v->pos++;
        v->win = (v->win << 8) | c;
        if (v->win == ID_CLUSTER) { v->st = MV_CLCAB; v->cvN = 0; break; }
      }
      break;
    case MV_CLCAB: {
      // After a Cluster ID found by scanning: its size, then the Timestamp
      // child (E7, a one-byte size of 1..8), optionally after a CRC-32 child
      // (BF 84 + 4 bytes: FFmpeg writes one first). Only then is it believed.
      unsigned char c = in[i++];
      int ws, ts;
      out[o++] = c; v->pos++;
      v->cv[v->cvN++] = c;
      ws = largura(v->cv[0]);
      if (ws > 8) { v->st = MV_SCAN; v->win = c; break; }
      if (v->cvN < ws + 2) break;
      ts = ws;                                     // where E7 should be
      if (v->cv[ws] == 0xBF) {
        if (v->cv[ws + 1] != 0x84) { v->st = MV_SCAN; v->win = 0; break; }
        ts = ws + 6;
        if (v->cvN < ts + 2) break;
      }
      { uint64_t t = v->cv[0] & (0xFFu >> ws); int todos = t == (uint64_t)(0xFFu >> ws);
        unsigned char tsTam = v->cv[ts + 1];
        for (int k = 1; k < ws; k++) { t = (t << 8) | v->cv[k]; if (v->cv[k] != 0xFF) todos = 0; }
        if (todos || v->cv[ts] != ID_TIMESTAMP || tsTam < 0x81 || tsTam > 0x88 || t < 3 || t > (1ull << 30)) {
          v->st = MV_SCAN; v->win = 0; break;
        }
        v->clFim = v->pos - (v->cvN - ws) + (int64_t)t;   // data starts right after the size
        v->sincronias++;
        v->ate = v->pos + (tsTam & 0x7F); v->ret = MV_FILHO; v->st = MV_PULA;
        if (v->ate > v->clFim) mvPerdeu(v);
      }
      break; }
    case MV_TOPO: {
      uint32_t id = 0; int64_t tam = 0; int r;
      unsigned char c;
      if (v->segFim >= 0 && v->pos >= v->segFim) { v->st = MV_SCAN; v->win = 0; break; }
      c = in[i++];
      out[o++] = c; v->pos++;
      v->cv[v->cvN++] = c;
      r = mvCab(v->cv, v->cvN, &id, &tam);
      if (r == 0) { if (v->cvN >= (int)sizeof v->cv) mvPerdeu(v); break; }
      v->cvN = 0;
      if (r < 0) { mvPerdeu(v); break; }
      if (id == ID_SEGMENT) { v->segFim = tam < 0 ? -1 : v->pos + tam; break; }   // its children are the top level
      if (id == ID_CLUSTER) {
        if (tam < 0) { mvPerdeu(v); break; }   // unknown size: rescan at the next Cluster
        v->clFim = v->pos + tam; v->st = MV_FILHO;
        break;
      }
      if ((id == ID_EBML || mvNivel1(id)) && tam >= 0) { v->ate = v->pos + tam; v->ret = MV_TOPO; v->st = MV_PULA; break; }
      mvPerdeu(v);
      break; }
    case MV_FILHO: {
      int anular = 0, d;
      int64_t fim = -1, ini;
      if (!v->hbN) {
        if (v->pos == v->clFim) { v->st = MV_TOPO; v->cvN = 0; break; }
        if (v->pos > v->clFim) { mvPerdeu(v); break; }
      }
      v->hb[v->hbN++] = in[i++]; v->pos++;
      ini = v->pos - v->hbN;
      d = mvDecidir(v, ini, &anular, &fim);
      if (d == 0 && v->hbN < MKVFIX_VOID_HB) break;   // the rest of the header is in the next bytes
      if (d == 0) v->indecisos++;
      if (d > 0 && anular) { v->hb[0] = ID_VOID; v->anulados++; v->bytesAnulados += fim - ini; }
      memcpy(out + o, v->hb, (size_t)v->hbN); o += v->hbN; v->hbN = 0;
      if (fim < 0) { mvPerdeu(v); break; }
      if (v->pos < fim) { v->ate = fim; v->ret = MV_FILHO; v->st = MV_PULA; }
      break; }
    default: mvPerdeu(v); break;
    }
  }
  return o;
}

long mkvfix_void_fim(MkvfixVoid *v, unsigned char *out) {
  long o = v->hbN;
  if (o) { memcpy(out, v->hb, (size_t)o); v->hbN = 0; v->indecisos++; }
  return o;
}

// O ponto no indice (mkvfix.h). Duas passadas pelos CuePoint: conta por faixa
// (a do video tem quase todos), depois o maior CueTime <= alvo dela.
#define ID_CUEPOINT  0xBBu
#define ID_CUETIME   0xB3u
#define ID_CUETRACKP 0xB7u
#define ID_CUETRACK  0xF7u
#define ID_CUECLUSTER 0xF1u
static int cuePonto(const unsigned char *p, long n, int64_t *tempo, unsigned *faixa, int64_t *pos) {
  long o = 0; int achou = 0;
  *tempo = -1;
  while (o + 2 <= n) {
    int ui = 0, us = 0;
    uint32_t id = lerId(p + o, n - o, &ui);
    int64_t t;
    if (!id) return 0;
    t = lerTam(p + o + ui, n - o - ui, &us);
    if (t < 0 || o + ui + us + t > n) return 0;
    if (id == ID_CUETIME && t <= 8) *tempo = (int64_t)lerUint(p + o + ui + us, t);
    else if (id == ID_CUETRACKP && !achou) {
      const unsigned char *q = p + o + ui + us; long m = (long)t, k = 0;
      unsigned f = 0; int64_t c = -1;
      while (k + 2 <= m) {
        int vi = 0, vs = 0;
        uint32_t jd = lerId(q + k, m - k, &vi);
        int64_t u;
        if (!jd) break;
        u = lerTam(q + k + vi, m - k - vi, &vs);
        if (u < 0 || k + vi + vs + u > m) break;
        if (jd == ID_CUETRACK && u <= 8) f = (unsigned)lerUint(q + k + vi + vs, u);
        else if (jd == ID_CUECLUSTER && u <= 8) c = (int64_t)lerUint(q + k + vi + vs, u);
        k += vi + vs + u;
      }
      if (f && c >= 0) { *faixa = f; *pos = c; achou = 1; }
    }
    o += ui + us + t;
  }
  return achou && *tempo >= 0;
}
int64_t mkvfix_cues_posicao(const unsigned char *b, long n, int64_t alvoMs, int64_t *cueMs, int64_t *maxMs) {
  return mkvfix_cues_ponto(b, n, alvoMs, cueMs, maxMs, NULL, NULL);
}
int64_t mkvfix_cues_ponto(const unsigned char *b, long n, int64_t alvoMs, int64_t *cueMs, int64_t *maxMs,
                          int64_t *proxMs, int64_t *proxPos) {
  unsigned contagem[64] = {0}, melhor = 0;
  int64_t tam, achado = -1, achadoT = -1, maior = -1, depoisT = -1, depoisP = -1;
  long o, fim;
  int ui = 0, us = 0;
  if (cueMs) *cueMs = -1;
  if (maxMs) *maxMs = -1;
  if (proxMs) *proxMs = -1;
  if (proxPos) *proxPos = -1;
  if (!b || n < 8) return -1;
  // O trecho guardado pode comecar antes do Cues (o 2o SeekHead, um Void):
  // anda pelos elementos de cima; sem estrutura legivel, procura o ID.
  { long k = 0;
    while (k + 8 <= n && lerId(b + k, n - k, &ui) != ID_CUES) {
      int64_t t = lerTam(b + k + ui, n - k - ui, &us);
      if (!lerId(b + k, n - k, &ui) || t < 0 || k + ui + us + t >= n) { k = -1; break; }
      k += (long)(ui + us + t);
    }
    if (k < 0 || k + 8 > n) {
      for (k = 0; k + 4 <= n; k++)
        if (b[k] == 0x1C && b[k + 1] == 0x53 && b[k + 2] == 0xBB && b[k + 3] == 0x6B) break;
      if (k + 8 > n) return -1;
    }
    b += k; n -= k; }
  if (lerId(b, n, &ui) != ID_CUES) return -1;
  tam = lerTam(b + ui, n - ui, &us);
  o = ui + us;
  fim = tam < 0 || o + tam > n ? n : (long)(o + tam);   // cortado: le o que veio
  for (int passo = 0; passo < 2; passo++) {
    long k = o;
    while (k + 2 <= fim) {
      int vi = 0, vs = 0;
      uint32_t id = lerId(b + k, fim - k, &vi);
      int64_t t, tempo = -1, pos = -1;
      unsigned faixa = 0;
      if (!id) break;
      t = lerTam(b + k + vi, fim - k - vi, &vs);
      if (t < 0 || k + vi + vs + t > fim) break;
      if (id == ID_CUEPOINT && cuePonto(b + k + vi + vs, (long)t, &tempo, &faixa, &pos)) {
        if (passo == 0) { if (faixa < 64) contagem[faixa]++; }
        else if (faixa == melhor) {
          if (tempo > maior) maior = tempo;
          if (tempo <= alvoMs && tempo > achadoT) { achadoT = tempo; achado = pos; }
          if (tempo > alvoMs && (depoisT < 0 || tempo < depoisT)) { depoisT = tempo; depoisP = pos; }
        }
      }
      k += vi + vs + t;
    }
    if (passo == 0) for (unsigned f = 1; f < 64; f++) if (contagem[f] > contagem[melhor]) melhor = f;
    if (!melhor) return -1;
  }
  if (cueMs) *cueMs = achadoT;
  if (maxMs) *maxMs = maior;
  if (proxMs) *proxMs = depoisT;
  if (proxPos) *proxPos = depoisP;
  return achado;
}

// A janela do adiantado (mkvfix.h). O player da TV nao vai ao CuePoint antes do
// ponto: pede o Cluster que CONTEM o segundo (log 13: 9,9 MB depois do cue,
// fora dos 8 MB baixados a partir dele). Estima o byte do segundo entre os dois
// CuePoint em volta (taxa local) ou pela media do arquivo, e comeca `antes`
// bytes antes dessa estimativa (nunca antes do cue).
int64_t mkvfix_janela_ini(int64_t cueMs, int64_t cuePos, int64_t proxMs, int64_t proxPos,
                          int64_t alvoMs, double bytesPorMs, int64_t antes) {
  double est;
  int64_t ini;
  if (cuePos < 0 || cueMs < 0 || alvoMs < cueMs) return cuePos;
  if (proxMs > cueMs && proxPos > cuePos)
    est = (double)cuePos + (double)(alvoMs - cueMs) / (double)(proxMs - cueMs) * (double)(proxPos - cuePos);
  else
    est = (double)cuePos + (double)(alvoMs - cueMs) * (bytesPorMs > 0 ? bytesPorMs : 0);
  // Um indice corrompido (posicoes absurdas) nao pode virar uma conversao fora
  // do int64: limita antes (o chamador ignora uma janela depois do arquivo).
  if (!(est >= (double)cuePos)) est = (double)cuePos;
  if (est > 4.0e18) est = 4.0e18;
  ini = (int64_t)est - antes;
  if (ini < cuePos) ini = cuePos;
  if (proxPos > cuePos && ini > proxPos) ini = proxPos;
  return ini;
}

// ---- servidor ----------------------------------------------------------------
#if defined(__EMSCRIPTEN__) || defined(NV_ANDROID) || defined(NV_MKVFIX_SEM_REDE)   // tests: no network
void mkvfix_sondar(const char *u, const char *c, const char *o) { (void)u; (void)c; (void)o; }
void mkvfix_sondar_relay(const char *u, const char *c, const char *o) { mkvfix_sondar(u, c, o); }
int  mkvfix_relay(void) { return 0; }
void mkvfix_sondar_arquivo(const char *u, const char *c, int usenet, int item, int file) {
  (void)usenet; (void)item; (void)file; mkvfix_sondar(u, c, NULL);
}
int  mkvfix_estado(void) { return MKVFIX_NADA; }
int  mkvfix_ocultas(void) { return 0; }
void mkvfix_modo_ass(int m) { (void)m; }
int  mkvfix_modo_ass_atual(void) { return MKVFIX_ASS_ESCONDER; }
void mkvfix_medidas(MkvfixMedidas *m) { memset(m, 0, sizeof *m); }
int  mkvfix_url(char *out, size_t n) { (void)out; (void)n; return 0; }
int  mkvfix_e_url(const char *u) { (void)u; return 0; }
void mkvfix_parar(void) {}
void mkvfix_adiantar(double alvoSeg, double durSeg) { (void)alvoSeg; (void)durSeg; }
void mkvfix_adiantar_pressa(void) {}
void mkvfix_adiantar_soltar(const char *porque) { (void)porque; }
void mkvfix_memoria_soltar(const char *porque) { (void)porque; }
#else

#include "rede.h"
#include "debrid.h"
#if defined(NV_TPK) && defined(__linux__)
#include <sys/prctl.h>
// Thread names for the CPU line ("[ass] 10s cpu", assrender.c).
#define MF_NOMEAR(n) prctl(PR_SET_NAME, (unsigned long)(n), 0, 0, 0)
#else
#define MF_NOMEAR(n) ((void)0)
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define MF_JANELA     16384      // primeira leitura: o SeekHead mora nos primeiros bytes
#define MF_VIVOS      8          // conexoes do player ao mesmo tempo
#define MF_PARADO_MS  25000L     // o CDN sem mandar byte por isto: desiste da conexao
#define MF_CAB        4096
#define MF_TRACKS_MAX (4L * 1024 * 1024)   // Tracks maior que isto: nao esconde (nunca visto: 243 KB)

typedef struct {
  unsigned id;
  char token[16];
  char *orig, *cab;              // o que o player tocava e os cabecalhos dele
  char *origem;                  // de onde pedir link novo (pode ser NULL)
  int usenet, item, file;        // selected TorBox file, owned by this session
  int relay;                     // #648: repasse puro de bytes (cabecalhos que a TV nao manda)
  pthread_mutex_t m;
  char up[8192];                 // o link de verdade (final, depois de redirect), sob m
  int upSemCab;                  // o final e de outra origem: sem cabecalhos
  int64_t total;
  int64_t segIni;                // inicio dos dados do Segment (o Cues e relativo a ele); -1 = nao MKV
  int64_t regiao; int regiaoN;
  unsigned char conserto[512];
  int64_t oculta[MKVFIX_OCULTAS];  // byte (absoluto) do valor do TrackType de cada legenda escondida
  int nOculta;                     // publicado antes do estado final
  uint64_t vazia[MKVFIX_OCULTAS];  // Void mode: track numbers whose blocks become Void (mkvfix.h)
  int nVazia;                      // publicado antes do estado final
  atomic_int estado;
  volatile int cancelar;
  atomic_int refs;
  atomic_uint pedidos;
} Sess;

static pthread_mutex_t G = PTHREAD_MUTEX_INITIALIZER;
static Sess *atual;
static unsigned proxId;
static int sockOuvir = -1, porta;
static atomic_int vivos;
static atomic_int modoAss = MKVFIX_ASS_ESCONDER;
void mkvfix_modo_ass(int m) {
  if (m != MKVFIX_ASS_ANULAR) m = MKVFIX_ASS_ESCONDER;
  atomic_store(&modoAss, m);
}
int mkvfix_modo_ass_atual(void) { return atomic_load(&modoAss); }
// "[mkvfix] 10s" (mkvfix_medidas): everything the connections did since the last read.
static atomic_llong mdBytes, mdEnvioUs, mdCdnUs, mdBytesAnulados;
static atomic_long mdAnulados, mdSincronias, mdPerdas, mdIndecisos;
void mkvfix_medidas(MkvfixMedidas *m) {
  m->bytes = atomic_exchange(&mdBytes, 0);
  m->envioMs = atomic_exchange(&mdEnvioUs, 0) / 1000;
  m->cdnMs = atomic_exchange(&mdCdnUs, 0) / 1000;
  m->bytesAnulados = atomic_exchange(&mdBytesAnulados, 0);
  m->anulados = atomic_exchange(&mdAnulados, 0);
  m->sincronias = atomic_exchange(&mdSincronias, 0);
  m->perdas = atomic_exchange(&mdPerdas, 0);
  m->indecisos = atomic_exchange(&mdIndecisos, 0);
  m->conexoes = atomic_load(&vivos);
}
static uint64_t agoraUs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000000u + (uint64_t)(t.tv_nsec / 1000);
}

static uint64_t agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)(t.tv_nsec / 1000000);
}

static void sessSoltar(Sess *s) {
  if (!s || atomic_fetch_sub(&s->refs, 1) != 1) return;
  pthread_mutex_destroy(&s->m);
  free(s->orig); free(s->cab); free(s->origem);
  free(s);
}
static Sess *sessPegarAtual(void) {
  Sess *s;
  pthread_mutex_lock(&G);
  s = atual;
  if (s) atomic_fetch_add(&s->refs, 1);
  pthread_mutex_unlock(&G);
  return s;
}

// O player so le pelo servidor quando ha o que trocar: o SeekHead (seek) ou
// TrackEntry de legenda escondidas (mkvfix.h).
static int servivel(Sess *s) {
  int e = atomic_load(&s->estado);
  return s->total > 0 && (e == MKVFIX_PRECISA || s->relay || (e != MKVFIX_SONDANDO && (s->nOculta > 0 || s->nVazia > 0)));
}

static const char *nomeId(uint32_t id) {
  switch (id) {
    case 0x1549A966: return "Info"; case 0x1654AE6B: return "Tracks"; case 0x1043A770: return "Chapters";
    case 0x1941A469: return "Attachments"; case 0x1254C367: return "Tags"; case ID_CUES: return "Cues";
    case ID_SEEKHEAD: return "SeekHead"; case 0x1F43B675: return "Cluster";
    default: return "?";
  }
}
static void mb(char *d, size_t n, int64_t v) {
  if (v < 0) snprintf(d, n, "?");
  else if (v >= 1024LL * 1024 * 1024) snprintf(d, n, "%.2f GB", v / 1073741824.0);
  else if (v >= 1024LL * 1024) snprintf(d, n, "%.1f MB", v / 1048576.0);
  else if (v >= 1024) snprintf(d, n, "%lld KB", (long long)(v / 1024));
  else snprintf(d, n, "%lld B", (long long)v);
}

static void definirEstado(Sess *s, int e, const char *porque) {
  atomic_store(&s->estado, e);
  printf("[mkvfix] check: %s\n", porque);
  fflush(stdout);
}

static unsigned char *ler(Sess *s, int64_t ini, int64_t fim, long *n, int64_t *total, int *st) {
  char up[8192], fin[8192] = "";   // rede.c escreve ate 8192 no final
  int cross = 0, semCab;
  unsigned char *b;
  pthread_mutex_lock(&s->m);
  snprintf(up, sizeof up, "%s", s->up);
  semCab = s->upSemCab;
  pthread_mutex_unlock(&s->m);
  b = (unsigned char *)rede_baixar_trecho64_final(up, semCab ? "" : s->cab, ini, fim, n, total, st,
                                                  &s->cancelar, fin, &cross);
  if (b && fin[0] && strcmp(fin, up)) {
    pthread_mutex_lock(&s->m);
    snprintf(s->up, sizeof s->up, "%s", fin);
    s->upSemCab = s->upSemCab || cross;
    pthread_mutex_unlock(&s->m);
  }
  return b;
}

// A legenda de texto de um arquivo com ASS/SSA: le o Tracks e marca as
// TrackEntry a esconder da TV (mkvfix.h). Antes do estado final.
static void sondaLegendas(Sess *s, const unsigned char *b, long n, const MkvfixCab *c, int64_t total) {
  int64_t tp = mkvfix_tracks_pos(b, n, c), off[MKVFIX_OCULTAS];
  long m = 0; int st = 0, k, ui = 0, us = 0;
  int64_t tam;
  unsigned char *q = NULL;
  const unsigned char *t;
  if (tp < 0 || total <= 0 || tp >= total) {
    printf("[mkvfix] subtitles: no Tracks element found, subtitle tracks left to the TV\n"); fflush(stdout); return;
  }
  // O Tracks quase sempre ja veio no primeiro trecho; senao, um Range so dele.
  if (tp + 12 <= n && lerId(b + tp, n - tp, &ui) == ID_TRACKS &&
      (tam = lerTam(b + tp + ui, (long)(n - tp - ui), &us)) >= 0 && tp + ui + us + tam <= n) {
    t = b + tp; m = (long)(ui + us + tam);
  } else {
    q = ler(s, tp, tp + 15 < total ? tp + 15 : total - 1, &m, NULL, &st);
    if (s->cancelar || !q || lerId(q, m, &ui) != ID_TRACKS || (tam = lerTam(q + ui, m - ui, &us)) < 0 ||
        ui + us + tam > MF_TRACKS_MAX) {
      printf("[mkvfix] subtitles: Tracks not readable (HTTP %d), subtitle tracks left to the TV\n", st);
      fflush(stdout); free(q); return;
    }
    free(q);
    q = ler(s, tp, tp + ui + us + tam - 1, &m, NULL, &st);
    if (s->cancelar || !q) { free(q); return; }
    t = q;
  }
  { uint64_t num[MKVFIX_OCULTAS];
    int modo = atomic_load(&modoAss), nv = 0;
    k = mkvfix_legendas_texto_num(t, m, off, num, MKVFIX_OCULTAS);
    if (k > 0 && modo == MKVFIX_ASS_ANULAR) {
      for (int i = 0; i < k; i++) if (num[i]) s->vazia[nv++] = num[i];
      if (nv == k) {
        s->nVazia = nv;
        printf("[mkvfix] subtitles: %d text subtitle track(s) with ASS/SSA kept in the header, their blocks sent to the TV "
               "as Void (Void mode); the file plays through the local server\n", nv);
        k = 0;
      } else {   // a track without a TrackNumber: Void can't name it, so hide them all as 2.0.8 does
        printf("[mkvfix] subtitles: Void mode, but %d of %d track(s) have no track number: hidden instead\n", k - nv, k);
        modo = MKVFIX_ASS_ESCONDER;
      }
    }
    if (k > 0 && modo == MKVFIX_ASS_ESCONDER) {
      for (int i = 0; i < k; i++) s->oculta[i] = tp + off[i];
      s->nOculta = k;
      printf("[mkvfix] subtitles: %d text subtitle track(s) with ASS/SSA hidden from the TV player (the app draws them); "
             "the file plays through the local server\n", k);
    } else if (k < 0) printf("[mkvfix] subtitles: Tracks element unreadable, subtitle tracks left to the TV\n"); }
  fflush(stdout);
  free(q);
}

static void *fioSonda(void *u) {
  Sess *s = u;
  MF_NOMEAR("nv-relaysonda");
  MkvfixCab c;
  long n = 0, m = 0;
  int st = 0, nSh2, nTodas = 0, entrou = 0, est = MKVFIX_NORMAL;
  int64_t total = -1, sh2, cues;
  uint32_t id2[MKVFIX_ENTRADAS], ids[MKVFIX_ENTRADAS * 2 + 1];
  int64_t pos2[MKVFIX_ENTRADAS], poss[MKVFIX_ENTRADAS * 2 + 1];
  char a[24], b2[24], d[24], msg[420] = "";
  uint64_t t0 = agoraMs();
  unsigned char *b = ler(s, 0, MF_JANELA - 1, &n, &total, &st), *q = NULL;
  if (s->cancelar) goto fim;
  if (!b || n < 64) {
    char x[96]; snprintf(x, sizeof x, "header not readable (HTTP %d, %ld bytes)", st, n);
    definirEstado(s, MKVFIX_FALHOU, x); goto fim;
  }
  if (!mkvfix_ler_inicio(b, n, &c)) { if (s->relay) s->total = total; definirEstado(s, MKVFIX_NORMAL, "not a Matroska file: nothing to do"); goto fim; }
  s->segIni = c.segIni;
  // Daqui em diante o indice (abaixo) e as legendas (sondaLegendas) sao
  // independentes: o estado so e publicado no fim, com as duas respostas.
  if (c.regiao < 0) { snprintf(msg, sizeof msg, "no SeekHead at the start of the Segment: left to the player"); goto legendas; }
  if (mkvfix_entrada(c.id, c.pos, c.n, ID_CUES) >= 0) {
    snprintf(msg, sizeof msg, "the first SeekHead lists the Cues (%d entries): nothing to fix (%u ms)",
             c.n, (unsigned)(agoraMs() - t0));
    goto legendas;
  }
  sh2 = mkvfix_entrada(c.id, c.pos, c.n, ID_SEEKHEAD);
  if (sh2 < 0 || total <= 0) {
    snprintf(msg, sizeof msg, "the first SeekHead (%d entries) has no Cues and no second SeekHead: no index to point to", c.n);
    est = MKVFIX_SEM_INDICE; goto legendas;
  }
  // O fim do pedido nunca passa do arquivo (o segundo SeekHead mora no fim).
  q = ler(s, c.segIni + sh2, c.segIni + sh2 + 4095 < total ? c.segIni + sh2 + 4095 : total - 1, &m, NULL, &st);
  if (s->cancelar) goto fim;
  nSh2 = q ? mkvfix_ler_seekhead(q, m, id2, pos2, MKVFIX_ENTRADAS) : -1;
  cues = nSh2 > 0 ? mkvfix_entrada(id2, pos2, nSh2, ID_CUES) : -1;
  free(q); q = NULL;
  if (cues < 0) {
    snprintf(msg, sizeof msg, "second SeekHead %s: no Cues listed", nSh2 < 0 ? "unreadable" : "read");
    est = nSh2 < 0 ? MKVFIX_FALHOU : MKVFIX_SEM_INDICE; goto legendas;
  }
  // Confere que ha um Cues de verdade la (nao se aponta o player para lixo).
  q = ler(s, c.segIni + cues, c.segIni + cues + 15 < total ? c.segIni + cues + 15 : total - 1, &m, NULL, &st);
  if (s->cancelar) goto fim;
  { int ui = 0;
    int achou = q && m >= 4 && lerId(q, m, &ui) == ID_CUES;
    free(q); q = NULL;
    if (!achou) {
      snprintf(msg, sizeof msg, "the second SeekHead points to Cues, but no Cues element is there");
      est = MKVFIX_SEM_INDICE; goto legendas;
    } }
  // Ordem: o Cues primeiro, depois o que o 1o SeekHead ja tinha (o caminho
  // para o segundo), depois o resto do segundo, enquanto couber.
  ids[nTodas] = ID_CUES; poss[nTodas++] = cues;
  for (int i = 0; i < c.n; i++) { ids[nTodas] = c.id[i]; poss[nTodas++] = c.pos[i]; }
  for (int i = 0; i < nSh2; i++) { ids[nTodas] = id2[i]; poss[nTodas++] = pos2[i]; }
  if (c.regiaoN > (int)sizeof s->conserto ||
      !(entrou = mkvfix_montar(ids, poss, nTodas, s->conserto, c.regiaoN))) {
    snprintf(msg, sizeof msg, "the SeekHead region (%d B) has no room for a Cues entry", c.regiaoN);
    est = MKVFIX_SEM_INDICE; goto legendas;
  }
  s->regiao = c.regiao; s->regiaoN = c.regiaoN;
  mb(a, sizeof a, c.segIni + sh2); mb(b2, sizeof b2, c.segIni + cues); mb(d, sizeof d, total);
  snprintf(msg, sizeof msg, "the first SeekHead (region %d B at %lld, %d entr%s) only points to a second SeekHead at %s, "
           "which lists the Cues at %s (file %s). The TV player does not follow it, so seeks go through the "
           "local fix: %d-byte region rewritten with %d entries (%u ms)",
           c.regiaoN, (long long)c.regiao, c.n, c.n == 1 ? "y" : "ies", a, b2, d, c.regiaoN, entrou,
           (unsigned)(agoraMs() - t0));
  est = MKVFIX_PRECISA;
legendas:
  sondaLegendas(s, b, n, &c, total);
  if (s->cancelar) goto fim;
  s->total = total;
  definirEstado(s, est, msg);
  if (est == MKVFIX_PRECISA) {
    char lista[160] = ""; size_t k = 0;
    uint32_t idN[MKVFIX_ENTRADAS]; int64_t posN[MKVFIX_ENTRADAS];
    int nN = mkvfix_ler_seekhead(s->conserto, s->regiaoN, idN, posN, MKVFIX_ENTRADAS);
    for (int i = 0; i < nN && k < sizeof lista - 16; i++)
      k += (size_t)snprintf(lista + k, sizeof lista - k, "%s%s", k ? ", " : "", nomeId(idN[i]));
    printf("[mkvfix] new SeekHead lists: %s\n", lista);
    fflush(stdout);
  }
fim:
  free(b); free(q);
  sessSoltar(s);
  return NULL;
}

static void sondar(const char *url, const char *cabecalhos, const char *origem,
                   int usenet, int item, int file, int relay) {
  Sess *s, *velho;
  pthread_t t;
  if (!url || (strncmp(url, "http://", 7) && strncmp(url, "https://", 8))) return;
  s = calloc(1, sizeof *s);
  if (!s) return;
  pthread_mutex_init(&s->m, NULL);
  s->orig = strdup(url); s->cab = strdup(cabecalhos ? cabecalhos : "");
  s->origem = origem && *origem && strcmp(origem, url) ? strdup(origem) : NULL;
  snprintf(s->up, sizeof s->up, "%s", url);
  s->usenet = usenet; s->item = item; s->file = file; s->relay = relay;
  s->total = -1; s->regiao = -1; s->segIni = -1;
  atomic_store(&s->estado, MKVFIX_SONDANDO);
  atomic_store(&s->refs, 2);   // a sessao atual + o fio da sonda
  pthread_mutex_lock(&G);
  s->id = ++proxId;
  snprintf(s->token, sizeof s->token, "%08x%04x", (unsigned)(agoraMs() * 2654435761u) ^ (unsigned)(uintptr_t)s,
           s->id & 0xFFFF);
  velho = atual; atual = s;
  pthread_mutex_unlock(&G);
  if (velho) { velho->cancelar = 1; sessSoltar(velho); }
  if (!s->orig || !s->cab || pthread_create(&t, NULL, fioSonda, s) != 0) {
    atomic_store(&s->estado, MKVFIX_FALHOU);
    sessSoltar(s);
    return;
  }
  pthread_detach(t);
}

void mkvfix_sondar(const char *url, const char *cabecalhos, const char *origem) {
  sondar(url, cabecalhos, origem, 0, 0, 0, 0);
}
void mkvfix_sondar_relay(const char *url, const char *cabecalhos, const char *origem) {
  sondar(url, cabecalhos, origem, 0, 0, 0, 1);
}
void mkvfix_sondar_arquivo(const char *url, const char *cabecalhos, int usenet, int item, int file) {
  sondar(url, cabecalhos, NULL, usenet, item, file, 0);
}

int mkvfix_estado(void) {
  int e;
  pthread_mutex_lock(&G);
  e = atual ? atomic_load(&atual->estado) : MKVFIX_NADA;
  pthread_mutex_unlock(&G);
  return e;
}

int mkvfix_ocultas(void) {
  int k = 0;
  pthread_mutex_lock(&G);
  if (atual && atomic_load(&atual->estado) != MKVFIX_SONDANDO) k = atual->nOculta + atual->nVazia;
  pthread_mutex_unlock(&G);
  return k;
}

int mkvfix_relay(void) {
  int ok = 0;
  pthread_mutex_lock(&G);
  ok = atual && atual->relay && servivel(atual);
  pthread_mutex_unlock(&G);
  return ok;
}

void mkvfix_parar(void) {
  Sess *velho;
  pthread_mutex_lock(&G);
  velho = atual; atual = NULL;
  pthread_mutex_unlock(&G);
  if (velho) {
    unsigned p = atomic_load(&velho->pedidos);
    if (p) { printf("[mkvfix] session ended after %u request(s) from the player\n", p); fflush(stdout); }
    velho->cancelar = 1;
    sessSoltar(velho);
  }
}

int mkvfix_e_url(const char *url) {
  char pre[40];
  if (!url || porta <= 0) return 0;
  snprintf(pre, sizeof pre, "http://127.0.0.1:%d/", porta);
  return !strncmp(url, pre, strlen(pre));
}

// --- conexoes -------------------------------------------------------------------
static int enviar(int sock, const void *b, size_t n, Sess *s) {
  const char *p = b;
  while (n) {
    ssize_t r = send(sock, p, n, MSG_NOSIGNAL);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      // Player parado (pausa, buffer cheio): espera; so o fim da sessao solta.
      if (s->cancelar) return 0;
      continue;
    }
    if (r <= 0) return 0;
    p += r; n -= (size_t)r;
  }
  return 1;
}

// --- assperf "Read subs from local relay" (so .tpk) --------------------------
//
// Os bytes que este servidor entrega ao player sao o arquivo inteiro; os blocos
// da legenda ASS vem neles, a frente do playhead. MfTap acompanha o fluxo de
// UMA conexao: acha um Cluster (ID + Timestamp logo depois, senao era falso),
// anda pelos filhos pulando o video sem copiar, e junta so os BlockGroup /
// SimpleBlock da faixa que o mkvass colhe (mkvass_relay_faixa). Cada um vai a
// mkvass_relay_bloco, que o casa com o indice. Pedido que comeca no meio de um
// Cluster (seek) ou byte fora de ordem: volta a procurar o proximo Cluster.
// Nunca muda um byte do que vai ao player e nunca pede nada ao CDN.
#ifdef NV_TPK
#include "mkvass.h"
#define MT_CLUSTER   0x1F43B675u
#define MT_BLOCO_MAX (1L << 20)
enum { MT_PROCURA, MT_TAMCLUSTER, MT_CAB, MT_PULA, MT_JUNTA };
typedef struct {
  int64_t pos;              // posicao absoluta do proximo byte
  int estado, faixa, ligado;
  int64_t clPos, clFim;     // Cluster atual; clFim -1 = tamanho desconhecido
  int clHdr, temTs, precisaTs;
  unsigned long clTs;
  unsigned char cab[16]; int cabN;
  int64_t elPos, pular;
  uint32_t janela;
  int junta;                // 1 Timestamp, 2 bloco
  int espiou;               // bloco: a faixa ja foi conferida
  int hdrN;                 // bytes de cabecalho no inicio de buf
  unsigned char *buf; long bufN, bufCap, bufTam;
} MfTap;

static void mtProcurar(MfTap *t) { t->estado = MT_PROCURA; t->janela = 0; t->cabN = 0; t->temTs = 0; }

static int mtGuardar(MfTap *t, const unsigned char *b, long n) {
  if (t->bufN + n > t->bufCap) {
    long nc = t->bufCap ? t->bufCap : 4096;
    unsigned char *nv;
    while (nc < t->bufN + n) nc *= 2;
    nv = realloc(t->buf, (size_t)nc);
    if (!nv) return 0;
    t->buf = nv; t->bufCap = nc;
  }
  memcpy(t->buf + t->bufN, b, (size_t)n); t->bufN += n;
  return 1;
}

// A faixa do bloco em buf (cabecalho + o que ja chegou dos dados). -1 = ainda
// nao da para saber; -2 = nao e bloco legivel.
static long mtFaixaDo(const MfTap *t, uint32_t id) {
  const unsigned char *d = t->buf + t->hdrN;
  long n = t->bufN - t->hdrN;
  int w;
  if (id == 0xA0u) {   // BlockGroup: o Block (A1) costuma ser o primeiro filho
    int ui = 0, us = 0; int64_t sz;
    if (n < 2) return -1;
    if (lerId(d, n, &ui) != 0xA1u) return -2;
    sz = lerTam(d + ui, n - ui, &us);
    if (sz == -2 && n < 9) return -1;
    if (sz < 0) return -2;
    d += ui + us; n -= ui + us;
  }
  if (n < 1) return -1;
  w = largura(d[0]);
  if (w > 8) return -2;
  if (n < w) return -1;
  { long v = d[0] & (0xFF >> w);
    for (int i = 1; i < w; i++) v = (v << 8) | d[i];
    return v; }
}

static void mtAlimentar(MfTap *t, int64_t pos, const unsigned char *b, size_t n) {
  size_t i = 0;
  int faixa = mkvass_relay_faixa();
  if (!faixa) { t->ligado = 0; t->pos = pos + (int64_t)n; return; }
  if (!t->ligado || t->pos != pos || faixa != t->faixa) {
    // Ligou agora, pulou bytes ou trocou de faixa: procura o proximo Cluster.
    t->ligado = 1; t->faixa = faixa; t->pos = pos; t->bufN = 0; mtProcurar(t);
  }
  while (i < n) {
    switch (t->estado) {
    case MT_PROCURA:
      while (i < n) {
        t->janela = (t->janela << 8) | b[i++]; t->pos++;
        if (t->janela == MT_CLUSTER) { t->clPos = t->pos - 4; t->cabN = 0; t->estado = MT_TAMCLUSTER; break; }
      }
      break;
    case MT_TAMCLUSTER: {
      int w, us = 0; int64_t sz;
      t->cab[t->cabN++] = b[i++]; t->pos++;
      w = largura(t->cab[0]);
      if (w > 8) { mtProcurar(t); break; }
      if (t->cabN < w) break;
      sz = lerTam(t->cab, t->cabN, &us);
      if (sz == -2) { mtProcurar(t); break; }
      t->clHdr = 4 + w; t->clFim = sz < 0 ? -1 : t->clPos + t->clHdr + sz;
      t->temTs = 0; t->precisaTs = 1; t->cabN = 0; t->estado = MT_CAB;
      break; }
    case MT_CAB: {
      int wi, ws, us = 0, ui = 0, dentro; uint32_t id; int64_t sz;
      if (!t->cabN) t->elPos = t->pos;
      t->cab[t->cabN++] = b[i++]; t->pos++;
      wi = largura(t->cab[0]);
      if (wi > 4) { mtProcurar(t); break; }
      if (t->cabN < wi + 1) break;
      ws = largura(t->cab[wi]);
      if (ws > 8) { mtProcurar(t); break; }
      if (t->cabN < wi + ws) break;
      id = lerId(t->cab, t->cabN, &ui);
      sz = lerTam(t->cab + wi, ws, &us);
      t->cabN = 0;
      if (id == MT_CLUSTER) {
        if (sz == -2) { mtProcurar(t); break; }
        t->clPos = t->elPos; t->clHdr = wi + ws; t->clFim = sz < 0 ? -1 : t->clPos + t->clHdr + sz;
        t->temTs = 0; t->precisaTs = 1;
        break;
      }
      dentro = t->clFim < 0 || t->elPos < t->clFim;
      if (sz < 0) { mtProcurar(t); break; }
      if (!dentro) {
        // Fora de Cluster (Cues, Tags...): passa por cima, o proximo e outro Cluster.
        t->pular = sz; t->estado = sz ? MT_PULA : MT_CAB; break;
      }
      if (t->precisaTs && id != 0xE7u && id != 0xBFu && id != 0xECu) { mtProcurar(t); break; }   // Cluster falso
      if (t->elPos + wi + ws + sz > (t->clFim < 0 ? INT64_MAX : t->clFim)) { mtProcurar(t); break; }
      if (id == 0xE7u) {
        if (sz < 1 || sz > 8) { mtProcurar(t); break; }
        t->junta = 1; t->bufN = 0; t->hdrN = 0; t->bufTam = (long)sz; t->estado = MT_JUNTA;
      } else if ((id == 0xA3u || id == 0xA0u) && t->temTs && wi + ws + sz <= MT_BLOCO_MAX) {
        t->junta = 2; t->bufN = 0; t->espiou = 0;
        if (!mtGuardar(t, t->cab, wi + ws)) { t->pular = sz; t->estado = MT_PULA; break; }
        t->hdrN = wi + ws; t->bufTam = (long)(wi + ws + sz);
        t->estado = MT_JUNTA;
      } else if (id == 0xA3u || id == 0xA0u || id == 0xA7u || id == 0xABu || id == 0xECu ||
                 id == 0xAFu || id == 0xBFu || id == 0x5854u || id == 0xA1u) {
        t->pular = sz; t->estado = sz ? MT_PULA : MT_CAB;
      } else mtProcurar(t);
      break; }
    case MT_PULA: {
      int64_t k = t->pular < (int64_t)(n - i) ? t->pular : (int64_t)(n - i);
      i += (size_t)k; t->pos += k; t->pular -= k;
      if (!t->pular) t->estado = MT_CAB;
      break; }
    case MT_JUNTA: {
      long falta = t->bufTam - t->bufN, k = falta < (long)(n - i) ? falta : (long)(n - i);
      if (!mtGuardar(t, b + i, k)) { t->pular = falta; t->estado = MT_PULA; t->bufN = 0; break; }
      i += (size_t)k; t->pos += k;
      if (t->junta == 2 && !t->espiou) {
        long fx = mtFaixaDo(t, t->buf[0]);   // buf[0] e o ID de um byte: A0 ou A3
        if (fx == -2 || (fx >= 0 && fx != t->faixa)) {   // nao e da faixa: o resto e pulado
          t->pular = t->bufTam - t->bufN; t->bufN = 0;
          t->estado = t->pular ? MT_PULA : MT_CAB;
          break;
        }
        if (fx >= 0) t->espiou = 1;
      }
      if (t->bufN < t->bufTam) break;
      if (t->junta == 1) {
        unsigned long v = 0;
        for (long q = 0; q < t->bufN; q++) v = (v << 8) | t->buf[q];
        t->clTs = v; t->temTs = 1; t->precisaTs = 0;
      } else {
        mkvass_relay_bloco(t->clPos, t->clHdr, t->clTs, t->temTs, t->elPos, t->buf, t->bufN);
      }
      t->bufN = 0; t->estado = MT_CAB;
      break; }
    }
  }
}
#endif

typedef struct {
  int sock;
  Sess *s;
  int64_t ini, fim, prox;     // bytes [ini, fim] pedidos; prox = o proximo a mandar
  int cabFeito, head, parcial, falhouEnvio;
  unsigned nr;
  MkvfixVoid vz;              // Void mode (s->nVazia > 0): the walker of this response
  long vzAnul;                // blocks voided in this response (log line)
  uint64_t fimRalo;           // end of the last ralo (CDN wait = gap to the next)
#ifdef NV_TPK
  MfTap tap;
#endif
  unsigned char *cap;         // copia crua do pedido curto, para o cache (ou NULL)
} Envio;

// Sends `n` bytes and counts them for "[mkvfix] 10s" (time blocked = the TV not reading).
static int enviarMedido(Envio *e, const unsigned char *b, size_t n) {
  uint64_t t = agoraUs();
  int r = enviar(e->sock, b, n, e->s);
  atomic_fetch_add(&mdEnvioUs, (long long)(agoraUs() - t));
  if (r) atomic_fetch_add(&mdBytes, (long long)n);
  return r;
}
// Void mode: the walker's counters into the 10 s totals (and this response's).
static void vzContar(Envio *e, const MkvfixVoid *antes) {
  atomic_fetch_add(&mdAnulados, e->vz.anulados - antes->anulados);
  atomic_fetch_add(&mdBytesAnulados, (long long)(e->vz.bytesAnulados - antes->bytesAnulados));
  atomic_fetch_add(&mdSincronias, e->vz.sincronias - antes->sincronias);
  atomic_fetch_add(&mdPerdas, e->vz.perdas - antes->perdas);
  atomic_fetch_add(&mdIndecisos, e->vz.indecisos - antes->indecisos);
  e->vzAnul += e->vz.anulados - antes->anulados;
}

// PEDIDOS CURTOS EM MEMORIA (a volta ao app, 09/11). Ao abrir, o player pede o
// comeco do arquivo e um trecho curto do fim (o indice/Cues: 114 KB no log 7,
// 1,98 MB no log 10). Numa
// reabertura do mesmo arquivo esse trecho e identico, mas ia de novo ao CDN por
// uma conexao nova (log 7: 1,3 s, 1 s so no TCP). Os ultimos pedidos curtos
// (<= 4 MiB, fora do byte 0; 3 deles, ate 12 MiB) ficam aqui, guardados CRUS: a troca do SeekHead e
// das legendas escondidas e refeita ao mandar. Chave: tamanho do arquivo +
// trecho (o link do CDN muda entre aberturas; um arquivo de video de mesmo
// tamanho exato e outro conteudo nao acontece acima de 64 MiB).
#define MF_CACHE_N    3
#define MF_CACHE_MAX  (4L * 1024 * 1024)
#define MF_CACHE_MIN_TOTAL (64LL * 1024 * 1024)
typedef struct { int64_t total, ini, fim; unsigned char *b; } MfCache;
static MfCache mfCache[MF_CACHE_N];
static int mfCacheProx;
static pthread_mutex_t mfCacheTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned char *cachePegar(int64_t total, int64_t ini, int64_t fim) {
  unsigned char *c = NULL;
  pthread_mutex_lock(&mfCacheTrava);
  for (int i = 0; i < MF_CACHE_N; i++)
    if (mfCache[i].b && mfCache[i].total == total && mfCache[i].ini == ini && mfCache[i].fim == fim) {
      size_t n = (size_t)(fim - ini + 1);
      if ((c = malloc(n)) != NULL) memcpy(c, mfCache[i].b, n);
      break;
    }
  pthread_mutex_unlock(&mfCacheTrava);
  return c;
}
// O ADIANTADO (mkvfix.h): o comeco do Cluster do ponto, baixado na saida do
// app. Usado uma vez (o seek da reabertura) e solto; vence em 10 min.
#define MF_ADI_BYTES    (8L * 1024 * 1024)
#define MF_ADI_BPS      (4L * 1024 * 1024)
#define MF_ADI_PRAZO_MS 10000u
#define MF_ADI_VIDA_MS  (10u * 60u * 1000u)
#define MF_ORCAMENTO    (12L * 1024 * 1024)   // indice + adiantado, juntos
#define MF_ADI_ANTES    (3L * 1024 * 1024)   // a janela comeca isto antes da estimativa
static struct { int64_t total, ini; long n; unsigned char *b; uint64_t em; unsigned gen; } adi;
// Cada pedido tem uma geracao; um pedido novo (ou soltar) avanca adiGen e o
// fio antigo para sozinho. Os fios rodam um de cada vez (adiFio), sem que
// quem pede espere: o host chama na hora do OnPause.
static atomic_uint adiGen;
static atomic_int adiPressa;
static pthread_mutex_t adiFio = PTHREAD_MUTEX_INITIALIZER;
// Um pedido novo ou soltar corta tambem a conexao que ainda nao respondeu (o
// ralo so ve a geracao quando chegam bytes; um CDN mudo segurava o fio ate o
// prazo de 5 s). O fio zera ANTES de conferir a geracao (ordem seq_cst).
static volatile int adiCancelar;
// O trecho sendo baixado agora (sob mfCacheTrava): o seek da reabertura so
// espera o adiantado se comecar dentro dele.
static struct { int ativo; int64_t total, ini, fim; } adiBaixa;
static uint64_t agoraMs(void);

// Chamado com mfCacheTrava: tira pedidos curtos (mais velhos primeiro) ate o
// novo caber no orcamento junto do adiantado.
static void cacheCaber(long novo) {
  long usado = adi.b ? adi.n : 0;
  for (int i = 0; i < MF_CACHE_N; i++) if (mfCache[i].b) usado += (long)(mfCache[i].fim - mfCache[i].ini + 1);
  for (int k = 0; k < MF_CACHE_N && usado + novo > MF_ORCAMENTO; k++) {
    MfCache *c = &mfCache[(mfCacheProx + k) % MF_CACHE_N];
    if (!c->b) continue;
    usado -= (long)(c->fim - c->ini + 1);
    free(c->b); c->b = NULL;
  }
}

static void cacheGuardar(int64_t total, int64_t ini, int64_t fim, unsigned char *b) {
  pthread_mutex_lock(&mfCacheTrava);
  cacheCaber((long)(fim - ini + 1));
  for (int i = 0; i < MF_CACHE_N; i++)
    if (mfCache[i].b && mfCache[i].total == total && mfCache[i].ini == ini && mfCache[i].fim == fim) {
      free(mfCache[i].b); mfCache[i].b = b; b = NULL; break;
    }
  if (b) {
    MfCache *c = &mfCache[mfCacheProx]; mfCacheProx = (mfCacheProx + 1) % MF_CACHE_N;
    free(c->b);
    c->total = total; c->ini = ini; c->fim = fim; c->b = b;
  }
  pthread_mutex_unlock(&mfCacheTrava);
}

static int enviarCab(Envio *e) {
  char c[512];
  int64_t len = e->fim - e->ini + 1;
  const char *tipo = e->s->relay ? "application/octet-stream" : "video/x-matroska";
  if (e->parcial)
    snprintf(c, sizeof c, "HTTP/1.1 206 Partial Content\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\n"
             "Content-Range: bytes %lld-%lld/%lld\r\nContent-Length: %lld\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n",
             tipo, (long long)e->ini, (long long)e->fim, (long long)e->s->total, (long long)len);
  else
    snprintf(c, sizeof c, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nAccept-Ranges: bytes\r\n"
             "Content-Length: %lld\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n", tipo, (long long)len);
  e->cabFeito = 1;
  return enviar(e->sock, c, strlen(c), e->s);
}

// ---- adiantar na saida do app (mkvfix.h) -------------------------------------
typedef struct { unsigned char *b; long n, max; uint64_t t0; int venceu; unsigned gen; } Adi;

static int adiRalo(void *u, const char *d, size_t n) {
  Adi *a = u;
  size_t k;
  uint64_t foi, deve;
  if (a->gen != atomic_load(&adiGen)) return 0;
  if (agoraMs() - a->t0 > MF_ADI_PRAZO_MS) { a->venceu = 1; return 0; }
  k = n > (size_t)(a->max - a->n) ? (size_t)(a->max - a->n) : n;
  memcpy(a->b + a->n, d, k); a->n += (long)k;
  // Freio: no maximo MF_ADI_BPS em media (a rede e dividida com o app da
  // frente). O tempo dentro do ralo nao conta como parada do CDN.
  if (!atomic_load(&adiPressa)) {
    foi = agoraMs() - a->t0;
    deve = (uint64_t)a->n * 1000u / MF_ADI_BPS;
    while (deve > foi && a->gen == atomic_load(&adiGen) && !atomic_load(&adiPressa)) {
      uint64_t z = deve - foi > 50 ? 50 : deve - foi;
      usleep((useconds_t)(z * 1000u));
      foi = agoraMs() - a->t0;
    }
  }
  return a->n < a->max;
}

static void adiLimpar(const char *porque) {   // com mfCacheTrava
  if (!adi.b) return;
  if (porque) { printf("[mkvfix] prefetch at the point dropped (%s; %.1f MB not used)\n", porque, adi.n / 1048576.0); fflush(stdout); }
  free(adi.b); adi.b = NULL; adi.n = 0;
}

typedef struct { Sess *s; double alvoSeg, durSeg; unsigned gen; } AdiPedido;

static void *fioAdiantar(void *u) {
  AdiPedido *p = u;
  Sess *s = p->s;
  int64_t pos = -1, cueMs = -1, maxMs = -1, proxMs = -1, proxPos = -1, ini, total = s->total;
  int64_t alvoMs = (int64_t)(p->alvoSeg * 1000.0);
  long durMs = (long)(p->durSeg * 1000.0);
  char up[8192];
  int semCab, st = 0, cross = 0, r;
  int64_t veio = 0, tot = -1;
  Adi a = {0};
  a.gen = p->gen;
  pthread_mutex_lock(&adiFio);              // o anterior ja saiu (ou esta saindo)
  __atomic_store_n(&adiCancelar, 0, __ATOMIC_SEQ_CST);   // antes de conferir a geracao
  if (a.gen != atomic_load(&adiGen)) goto sai;
  // O indice ja veio na abertura (pedido curto em memoria): acha o ponto nele.
  pthread_mutex_lock(&mfCacheTrava);
  for (int i = 0; i < MF_CACHE_N && pos < 0; i++)
    if (mfCache[i].b && mfCache[i].total == total)
      pos = mkvfix_cues_ponto(mfCache[i].b, (long)(mfCache[i].fim - mfCache[i].ini + 1), alvoMs, &cueMs, &maxMs,
                              &proxMs, &proxPos);
  pthread_mutex_unlock(&mfCacheTrava);
  if (pos < 0 || s->segIni < 0) {
    printf("[mkvfix] prefetch at %.1f s skipped: %s\n", p->alvoSeg, s->segIni < 0 ? "not Matroska" : "the index is not in memory");
    fflush(stdout); goto sai;
  }
  // Escala do tempo: o indice em ms (TimestampScale 1 ms, o normal). Outra
  // escala daria o lugar errado: nao adianta.
  if (durMs > 0 && (maxMs > durMs + durMs / 20 + 5000 || maxMs < durMs / 2)) {
    printf("[mkvfix] prefetch at %.1f s skipped: index times (last %lld) do not look like ms for a %ld ms file\n",
           p->alvoSeg, (long long)maxMs, durMs);
    fflush(stdout); goto sai;
  }
  ini = s->segIni + mkvfix_janela_ini(cueMs, pos, proxMs, proxPos, alvoMs,
                                      durMs > 0 ? (double)total / (double)durMs : 0, MF_ADI_ANTES);
  if (ini <= 0 || ini >= total) goto sai;
  a.max = total - ini < MF_ADI_BYTES ? (long)(total - ini) : MF_ADI_BYTES;
  if (!(a.b = malloc((size_t)a.max))) goto sai;
  a.t0 = agoraMs();
  printf("[mkvfix] prefetch at %.3f s: index points %.3f s (byte %lld) and %.3f s, window from byte %lld, "
         "up to %.1f MB at most %.0f MB/s\n", p->alvoSeg, cueMs / 1000.0, (long long)(s->segIni + pos),
         proxMs / 1000.0, (long long)ini, a.max / 1048576.0, MF_ADI_BPS / 1048576.0);
  fflush(stdout);
  pthread_mutex_lock(&s->m);
  snprintf(up, sizeof up, "%s", s->up); semCab = s->upSemCab;
  pthread_mutex_unlock(&s->m);
  pthread_mutex_lock(&mfCacheTrava);
  adiBaixa.ativo = 1; adiBaixa.total = total; adiBaixa.ini = ini; adiBaixa.fim = ini + a.max;
  pthread_mutex_unlock(&mfCacheTrava);
  r = rede_fluxo64(up, semCab ? "" : s->cab, ini, adiRalo, &a, &tot, &st, &adiCancelar, NULL, &cross, 5000, &veio);
  if (a.gen != atomic_load(&adiGen) || a.n <= 0 || (r < 0 && a.n < a.max && !a.venceu)) {
    int trocado = a.gen != atomic_load(&adiGen);
    printf("[mkvfix] prefetch at %.1f s stopped: %s (%.1f MB in %u ms, HTTP %d)\n", p->alvoSeg,
           trocado ? "replaced or cancelled" : "the CDN failed", a.n / 1048576.0, (unsigned)(agoraMs() - a.t0), st);
    fflush(stdout);
    if (trocado || a.n < 1024 * 1024) goto sai;   // pouco ou cancelado: nao guarda
  }
  printf("[mkvfix] prefetch at %.1f s: %.1f MB in %u ms%s\n", p->alvoSeg, a.n / 1048576.0,
         (unsigned)(agoraMs() - a.t0), a.venceu ? " (10 s limit: kept what came)" : "");
  fflush(stdout);
  pthread_mutex_lock(&mfCacheTrava);
  if (a.gen == atomic_load(&adiGen)) {
    adiLimpar(NULL);
    cacheCaber(a.n);
    adi.total = total; adi.ini = ini; adi.n = a.n; adi.b = a.b; adi.em = agoraMs(); adi.gen = a.gen;
    a.b = NULL;
  }
  adiBaixa.ativo = 0;
  pthread_mutex_unlock(&mfCacheTrava);
  // Vence em 10 min sem uso (o app ficou no fundo): solta a memoria. Usado,
  // trocado ou solto: o fio sai.
  while (a.gen == atomic_load(&adiGen)) {
    int vivo;
    usleep(500000);
    pthread_mutex_lock(&mfCacheTrava);
    vivo = adi.b != NULL && adi.gen == a.gen;
    if (vivo && agoraMs() - adi.em > MF_ADI_VIDA_MS) { adiLimpar("10 minutes unused"); vivo = 0; }
    pthread_mutex_unlock(&mfCacheTrava);
    if (!vivo) break;
  }
sai:
  pthread_mutex_lock(&mfCacheTrava);
  adiBaixa.ativo = 0;
  pthread_mutex_unlock(&mfCacheTrava);
  pthread_mutex_unlock(&adiFio);
  free(a.b);
  sessSoltar(s);
  free(p);
  return NULL;
}

void mkvfix_adiantar(double alvoSeg, double durSeg) {
  Sess *s;
  AdiPedido *p;
  pthread_t t;
  unsigned gen = atomic_fetch_add(&adiGen, 1) + 1;   // o anterior para sozinho
  __atomic_store_n(&adiCancelar, 1, __ATOMIC_SEQ_CST);   // mesmo sem ter respondido
  pthread_mutex_lock(&mfCacheTrava);
  adiLimpar(adi.b ? "a new one" : NULL);
  pthread_mutex_unlock(&mfCacheTrava);
  s = sessPegarAtual();
  if (!s) return;
  if (!servivel(s) || s->relay || s->total < MF_CACHE_MIN_TOTAL || alvoSeg < 1.0) { sessSoltar(s); return; }
  p = calloc(1, sizeof *p);
  if (!p) { sessSoltar(s); return; }
  p->s = s; p->alvoSeg = alvoSeg; p->durSeg = durSeg; p->gen = gen;
  atomic_store(&adiPressa, 0);
  if (pthread_create(&t, NULL, fioAdiantar, p) != 0) { sessSoltar(s); free(p); return; }
  pthread_detach(t);
}

void mkvfix_adiantar_pressa(void) { atomic_store(&adiPressa, 1); }

void mkvfix_adiantar_soltar(const char *porque) {
  atomic_fetch_add(&adiGen, 1);
  __atomic_store_n(&adiCancelar, 1, __ATOMIC_SEQ_CST);
  pthread_mutex_lock(&mfCacheTrava);
  adiLimpar(porque);
  pthread_mutex_unlock(&mfCacheTrava);
}

void mkvfix_memoria_soltar(const char *porque) {
  long n = 0;
  mkvfix_adiantar_soltar(porque);
  pthread_mutex_lock(&mfCacheTrava);
  for (int i = 0; i < MF_CACHE_N; i++)
    if (mfCache[i].b) { n += (long)(mfCache[i].fim - mfCache[i].ini + 1); free(mfCache[i].b); mfCache[i].b = NULL; }
  pthread_mutex_unlock(&mfCacheTrava);
  if (n) { printf("[mkvfix] kept short reads dropped (%s; %.1f MB)\n", porque ? porque : "", n / 1048576.0); fflush(stdout); }
}

// O seek da reabertura: o comeco vem do adiantado (copia, para nao segurar a
// trava enquanto manda). Esperando o adiantado terminar: ate 2 s, e so se o
// pedido comeca dentro do trecho sendo baixado (um seek em outro lugar nao
// espera, nem por um CDN que nao respondeu).
static unsigned char *adiPegar(int64_t total, int64_t ini, long *n) {
  unsigned char *c = NULL;
  for (int i = 0;; i++) {
    pthread_mutex_lock(&mfCacheTrava);
    if (i >= 40 || !adiBaixa.ativo || adiBaixa.total != total || ini < adiBaixa.ini || ini >= adiBaixa.fim) break;
    pthread_mutex_unlock(&mfCacheTrava);
    usleep(50000);
  }
  *n = 0;   // com mfCacheTrava
  if (adi.b && adi.total == total && ini >= adi.ini && ini < adi.ini + adi.n) {
    *n = (long)(adi.ini + adi.n - ini);
    if ((c = malloc((size_t)*n)) != NULL) memcpy(c, adi.b + (ini - adi.ini), (size_t)*n);
    free(adi.b); adi.b = NULL; adi.n = 0;   // uma vez so (o fio do prazo sai)
  } else if (adi.b && adi.total == total && ini > adi.ini - 64LL * 1024 * 1024 && ini < adi.ini + adi.n + 64LL * 1024 * 1024) {
    // So perto do palpite: os pedidos normais do comeco do arquivo nao contam.
    printf("[mkvfix] prefetched data not used: the player asked from byte %lld (prefetched %lld-%lld)\n",
           (long long)ini, (long long)adi.ini, (long long)(adi.ini + adi.n - 1));
    fflush(stdout);
  }
  pthread_mutex_unlock(&mfCacheTrava);
  return c;
}

// O corpo que chega do CDN: troca os bytes da regiao do SeekHead e manda.
static int ralo(void *u, const char *dados, size_t n) {
  Envio *e = u;
  unsigned char tmp[16384], vzOut[sizeof tmp + MKVFIX_VOID_HB];
  uint64_t t0 = agoraUs();
  if (e->fimRalo) atomic_fetch_add(&mdCdnUs, (long long)(t0 - e->fimRalo));
  if (e->s->cancelar) return 0;
  if (!e->cabFeito && !enviarCab(e)) { e->falhouEnvio = 1; return 0; }
  while (n && e->prox <= e->fim) {
    size_t k = n > sizeof tmp ? sizeof tmp : n;
    int64_t sobra = e->fim - e->prox + 1, a, z;
    if ((int64_t)k > sobra) k = (size_t)sobra;
    memcpy(tmp, dados, k);
    if (e->cap) memcpy(e->cap + (e->prox - e->ini), dados, k);
    a = e->prox > e->s->regiao ? e->prox : e->s->regiao;
    z = e->prox + (int64_t)k < e->s->regiao + e->s->regiaoN ? e->prox + (int64_t)k : e->s->regiao + e->s->regiaoN;
    if (a < z) memcpy(tmp + (a - e->prox), e->s->conserto + (a - e->s->regiao), (size_t)(z - a));
    // Legenda escondida: o TrackType dela vira "controle" (mkvfix.h).
    for (int i = 0; i < e->s->nOculta; i++)
      if (e->s->oculta[i] >= e->prox && e->s->oculta[i] < e->prox + (int64_t)k)
        tmp[e->s->oculta[i] - e->prox] = MKVFIX_TIPO_ESCONDIDA;
    if (e->s->nVazia > 0) {
      // Void mode (mkvfix.h): the walker rewrites the subtitle blocks' IDs; a
      // block header still being read waits in it for the next bytes.
      MkvfixVoid antes = e->vz;
      long w = mkvfix_void_passar(&e->vz, tmp, (long)k, vzOut);
      vzContar(e, &antes);
      if (w > 0 && !enviarMedido(e, vzOut, (size_t)w)) { e->falhouEnvio = 1; return 0; }
    } else if (!enviarMedido(e, tmp, k)) { e->falhouEnvio = 1; return 0; }
#ifdef NV_TPK
    mtAlimentar(&e->tap, e->prox, tmp, k);   // the subtitle harvest reads the blocks as they were
#endif
    e->prox += (int64_t)k; dados += k; n -= k;
  }
  if (e->prox > e->fim && e->s->nVazia > 0) {   // the end of the Range: what the walker still holds
    unsigned char resto[MKVFIX_VOID_HB];
    MkvfixVoid antes = e->vz;
    long w = mkvfix_void_fim(&e->vz, resto);
    vzContar(e, &antes);
    if (w > 0 && !enviarMedido(e, resto, (size_t)w)) { e->falhouEnvio = 1; return 0; }
  }
  e->fimRalo = agoraUs();
  return e->prox <= e->fim;
}

// O link do CDN venceu (403/404/410 antes do primeiro byte): pede um novo a
// origem com um Range minimo e troca o `up` da sessao.
static int renovar(Sess *s) {
  char fin[8192] = "";
  long n = 0; int st = 0, cross = 0;
  unsigned char *b;
  if (s->item > 0) {
    if (!debrid_tb_link(s->usenet, s->item, s->file, fin, sizeof fin) || !fin[0]) return 0;
    cross = 1;
  } else {
    if (!s->origem) return 0;
    b = (unsigned char *)rede_baixar_trecho64_final(s->origem, s->cab, 0, 0, &n, NULL, &st,
                                                    &s->cancelar, fin, &cross);
    free(b);
    if (!fin[0] || st != 206) return 0;
  }
  pthread_mutex_lock(&s->m);
  snprintf(s->up, sizeof s->up, "%s", fin);
  s->upSemCab = cross;
  pthread_mutex_unlock(&s->m);
  printf("[mkvfix] the CDN link was refused: a fresh one came from the source link\n");
  fflush(stdout);
  return 1;
}

static void campo(const char *cab, const char *nome, char *dst, size_t n) {
  const char *p = cab;
  size_t k = strlen(nome);
  dst[0] = 0;
  while ((p = strchr(p, '\n')) != NULL) {
    p++;
    if (!strncasecmp(p, nome, k) && p[k] == ':') {
      const char *v = p + k + 1, *f;
      while (*v == ' ' || *v == '\t') v++;
      f = v + strcspn(v, "\r\n");
      snprintf(dst, n, "%.*s", (int)(f - v), v);
      return;
    }
  }
}

static void responder(int sock, int cod, const char *msg, int64_t total) {
  char c[256];
  if (cod == 416) snprintf(c, sizeof c, "HTTP/1.1 416 %s\r\nContent-Range: bytes */%lld\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", msg, (long long)total);
  else snprintf(c, sizeof c, "HTTP/1.1 %d %s\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", cod, msg);
  (void)!send(sock, c, strlen(c), MSG_NOSIGNAL);
}

typedef struct { int sock; } Conexao;

static void *atender(void *u) {
  Conexao *cx = u;
  MF_NOMEAR("nv-relay");
  int sock = cx->sock;
  char b[MF_CAB], met[8] = "", alvo[256] = "", range[96] = "";
  size_t tot = 0;
  uint64_t fimPrazo = agoraMs() + 5000, t0;
  Sess *s = NULL;
  Envio e;
  free(cx);
  memset(&e, 0, sizeof e);
  { struct timeval tv = { 1, 0 }; setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv); }
  b[0] = 0;
  while (tot < sizeof b - 1 && !strstr(b, "\r\n\r\n") && !strstr(b, "\n\n")) {
    struct pollfd p = { sock, POLLIN, 0 };
    int resta = (int)(fimPrazo > agoraMs() ? fimPrazo - agoraMs() : 0);
    ssize_t r;
    if (resta <= 0 || poll(&p, 1, resta) <= 0) goto sai;
    r = recv(sock, b + tot, sizeof b - 1 - tot, 0);
    if (r <= 0) goto sai;
    tot += (size_t)r; b[tot] = 0;
  }
  if (sscanf(b, "%7s %255s", met, alvo) != 2) goto sai;
  s = sessPegarAtual();
  // GET /<token>/v.mkv — so a sessao atual e so ja consertada.
  if (!s || !servivel(s) || alvo[0] != '/' ||
      strncmp(alvo + 1, s->token, strlen(s->token)) || alvo[1 + strlen(s->token)] != '/') {
    responder(sock, 404, "Not Found", 0); goto sai;
  }
  if (strcmp(met, "GET") && strcmp(met, "HEAD")) { responder(sock, 405, "Method Not Allowed", 0); goto sai; }
  e.sock = sock; e.s = s; e.head = !strcmp(met, "HEAD");
  e.nr = atomic_fetch_add(&s->pedidos, 1) + 1;
  campo(b, "Range", range, sizeof range);
  e.ini = 0; e.fim = s->total - 1;
  if (range[0]) {
    long long a = -1, z = -1;
    const char *r = strstr(range, "bytes=");
    if (r && sscanf(r + 6, "%lld-%lld", &a, &z) >= 1 && a >= 0) {
      e.parcial = 1; e.ini = a;
      if (z >= a && z < s->total) e.fim = z;
    } else if (r && sscanf(r + 6, "-%lld", &z) == 1 && z > 0) {   // ultimos z bytes
      e.parcial = 1; e.ini = s->total > z ? s->total - z : 0;
    }
  }
  if (e.ini >= s->total) { responder(sock, 416, "Range Not Satisfiable", s->total); goto sai; }
  e.prox = e.ini;
  if (s->nVazia > 0) mkvfix_void_iniciar(&e.vz, s->vazia, s->nVazia, e.ini);
  if (e.head) { enviarCab(&e); goto sai; }
  t0 = agoraMs();
  if (e.parcial && e.ini > 0 && e.fim - e.ini + 1 <= MF_CACHE_MAX && s->total >= MF_CACHE_MIN_TOTAL) {
    unsigned char *c = cachePegar(s->total, e.ini, e.fim);
    if (c) {
      ralo(&e, (const char *)c, (size_t)(e.fim - e.ini + 1));
      free(c);
      printf("[mkvfix] #%u Range bytes %lld-%lld: sent %lld in %u ms from memory%s\n", e.nr, (long long)e.ini,
             (long long)e.fim, (long long)(e.prox - e.ini), (unsigned)(agoraMs() - t0), e.falhouEnvio ? " (player closed)" : "");
      fflush(stdout);
      goto sai;
    }
    e.cap = malloc((size_t)(e.fim - e.ini + 1));
  }
  // O seek da reabertura depois da saida do app: o comeco veio adiantado.
  if (e.parcial && e.ini > 0 && !e.cap) {
    long n = 0;
    unsigned char *c = adiPegar(s->total, e.ini, &n);
    if (c) {
      if ((int64_t)n > e.fim - e.ini + 1) n = (long)(e.fim - e.ini + 1);
      ralo(&e, (const char *)c, (size_t)n);
      free(c);
      printf("[mkvfix] #%u Range bytes %lld-: first %.1f MB from the prefetch in %u ms, the rest from the CDN\n",
             e.nr, (long long)e.ini, n / 1048576.0, (unsigned)(agoraMs() - t0));
      fflush(stdout);
      if (e.prox > e.fim || e.falhouEnvio || s->cancelar) goto sai;
    }
  }
  for (int tentativa = 0; tentativa < 2; tentativa++) {
    char up[8192];
    int semCab, st = 0, cross = 0, r;
    int64_t veio = 0, total = -1;
    pthread_mutex_lock(&s->m);
    snprintf(up, sizeof up, "%s", s->up); semCab = s->upSemCab;
    pthread_mutex_unlock(&s->m);
    r = rede_fluxo64(up, semCab ? "" : s->cab, e.prox, ralo, &e, &total, &st,
                     &s->cancelar, NULL, &cross, MF_PARADO_MS, &veio);
    if (e.prox > e.fim || e.falhouEnvio || s->cancelar) break;
    if (r < 0 && !e.cabFeito && (st == 403 || st == 404 || st == 410) && tentativa == 0 && renovar(s)) continue;
    if (r < 0 || e.prox <= e.fim) {
      if (!e.cabFeito) responder(sock, 502, "Bad Gateway", 0);
      printf("[mkvfix] #%u bytes %lld-%lld: the CDN stopped after %lld bytes (HTTP %d)\n", e.nr,
             (long long)e.ini, (long long)e.fim, (long long)(e.prox - e.ini), st);
      fflush(stdout);
    }
    break;
  }
  if (e.cap) {
    if (e.prox > e.fim && !e.falhouEnvio && !s->cancelar) cacheGuardar(s->total, e.ini, e.fim, e.cap);
    else free(e.cap);
    e.cap = NULL;
  }
  { int64_t len = e.prox - e.ini;
    int tocou = s->regiaoN > 0 && e.ini < s->regiao + s->regiaoN && e.prox > s->regiao;
    for (int i = 0; i < s->nOculta; i++) if (s->oculta[i] >= e.ini && s->oculta[i] < e.prox) tocou = 2;
    if (e.vzAnul > 0 || (s->nVazia > 0 && e.vz.sincronias == 0 && e.ini > 0 && e.prox - e.ini > 4 * 1024 * 1024))
      printf("[mkvfix] #%u Void: %ld subtitle block(s) voided in bytes %lld-%lld (resync %ld, lost %ld, undecided %ld)\n",
             e.nr, e.vzAnul, (long long)e.ini, (long long)e.prox - 1, e.vz.sincronias, e.vz.perdas, e.vz.indecisos);
    // Uma linha por pedido curto (cabecalho, Cues) e por pedido que tocou a
    // regiao; os longos (o video andando) so no fim, se pararem sem fim.
    if (tocou || (e.fim - e.ini) < 4 * 1024 * 1024 || e.nr <= 3)
      printf("[mkvfix] #%u %s bytes %lld-%lld: sent %lld in %u ms%s\n", e.nr, e.parcial ? "Range" : "whole",
             (long long)e.ini, (long long)e.fim, (long long)len, (unsigned)(agoraMs() - t0),
             tocou == 2 ? " (subtitle tracks hidden)" : tocou ? " (SeekHead rewritten)" : e.falhouEnvio ? " (player closed)" : "");
    fflush(stdout); }
sai:
#ifdef NV_TPK
  free(e.tap.buf);
#endif
  shutdown(sock, SHUT_RDWR);
  close(sock);
  if (s) sessSoltar(s);
  atomic_fetch_sub(&vivos, 1);
  return NULL;
}

static void *ouvir(void *u) {
  (void)u;
  for (;;) {
    int c = accept(sockOuvir, NULL, NULL);
    Conexao *cx;
    pthread_t t;
    if (c < 0) { if (errno == EINTR) continue; usleep(100000); continue; }
    if (atomic_fetch_add(&vivos, 1) >= MF_VIVOS) {
      atomic_fetch_sub(&vivos, 1);
      responder(c, 503, "Busy", 0); close(c); continue;
    }
    cx = malloc(sizeof *cx);
    if (!cx) { atomic_fetch_sub(&vivos, 1); close(c); continue; }
    cx->sock = c;
    if (pthread_create(&t, NULL, atender, cx) != 0) { free(cx); atomic_fetch_sub(&vivos, 1); close(c); continue; }
    pthread_detach(t);
  }
  return NULL;
}

static int subir(void) {
  struct sockaddr_in a;
  socklen_t al = sizeof a;
  int um = 1, s;
  pthread_t t;
  if (sockOuvir >= 0) return 1;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) return 0;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(s, (struct sockaddr *)&a, sizeof a) < 0 || listen(s, 16) < 0 ||
      getsockname(s, (struct sockaddr *)&a, &al) < 0) { close(s); return 0; }
  sockOuvir = s; porta = ntohs(a.sin_port);
  if (pthread_create(&t, NULL, ouvir, NULL) != 0) { close(s); sockOuvir = -1; porta = 0; return 0; }
  pthread_detach(t);
  printf("[mkvfix] local server on 127.0.0.1:%d\n", porta);
  fflush(stdout);
  return 1;
}

int mkvfix_url(char *out, size_t n) {
  int ok = 0;
  pthread_mutex_lock(&G);
  if (atual && servivel(atual) && subir()) {
    snprintf(out, n, "http://127.0.0.1:%d/%s/v.mkv", porta, atual->token);
    ok = 1;
  }
  pthread_mutex_unlock(&G);
  return ok;
}
#endif
