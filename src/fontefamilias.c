// Ver fontefamilias.h.
#include "fontefamilias.h"

#include <stdlib.h>
#include <string.h>

static char minusc(char c) { return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c; }

/* `nome` como o libass compara: sem espacos nas pontas, sem o '@' da escrita
 * vertical, minusculas ASCII. */
static size_t normalizar(const char *nome, size_t n, char *out, size_t cap) {
  size_t a = 0, b = n, i, o = 0;
  while (a < b && (nome[a] == ' ' || nome[a] == '\t')) a++;
  while (b > a && (nome[b - 1] == ' ' || nome[b - 1] == '\t' || nome[b - 1] == '\r')) b--;
  if (a < b && nome[a] == '@') a++;
  for (i = a; i < b && o + 1 < cap; i++) out[o++] = minusc(nome[i]);
  out[o] = 0;
  return o;
}

int fontefam_tem(const FonteFamilias *s, const char *nome) {
  char k[256];
  int i;
  if (!normalizar(nome, strlen(nome), k, sizeof k)) return 0;
  for (i = 0; i < s->n; i++) if (!strcmp(s->nomes[i], k)) return 1;
  return 0;
}

void fontefam_por(FonteFamilias *s, const char *nome, size_t n) {
  char k[256];
  int i;
  if (!normalizar(nome, n, k, sizeof k)) return;
  for (i = 0; i < s->n; i++) if (!strcmp(s->nomes[i], k)) return;
  if (s->n == s->cap) {
    int nc = s->cap ? s->cap * 2 : 16;
    char **nv = (char **)realloc(s->nomes, (size_t)nc * sizeof *nv);
    if (!nv) return;
    s->nomes = nv; s->cap = nc;
  }
  if ((s->nomes[s->n] = strdup(k)) != NULL) s->n++;
}

void fontefam_liberar(FonteFamilias *s) {
  int i;
  for (i = 0; i < s->n; i++) free(s->nomes[i]);
  free(s->nomes);
  s->nomes = NULL; s->n = s->cap = 0;
}

static int comeca(const char *p, const char *fim, const char *pre) {
  size_t n = strlen(pre);
  if ((size_t)(fim - p) < n) return 0;
  while (n--) if (minusc(*p++) != *pre++) return 0;
  return 1;
}

/* Runs on every harvested batch (mkvass entregarFontes) over the whole body,
 * which reaches several MB with frame-by-frame typesetting (owner 16/11/2026:
 * ASS had become slower than libass-engine-rewrite-tizen). So the line pass
 * stops at [Events] once the styles were read (events are most of the body),
 * and \fn is found with memchr instead of a byte loop (memmem would need
 * _GNU_SOURCE, which the .tpk's forced include makes unreliable). Same result. */
void fontefam_varrer(FonteFamilias *s, const char *doc, size_t n) {
  const char *p = doc, *fim = doc + n;
  int fontAt = -1, estilos = 0, viuEstilos = 0;
  while (p < fim) {
    const char *l = p, *e = memchr(p, '\n', (size_t)(fim - p));
    if (!e) e = fim;
    p = e + 1;
    while (l < e && (*l == ' ' || *l == '\t')) l++;
    if (l < e && *l == '[') {
      if (viuEstilos && comeca(l, e, "[events]")) break;
      estilos = comeca(l, e, "[v4+ styles]") || comeca(l, e, "[v4 styles]"); fontAt = -1;
      viuEstilos |= estilos;
      continue;
    }
    if (!estilos) continue;
    if (comeca(l, e, "format:")) {
      const char *c = l + 7; int k = 0;
      while (c < e) {
        const char *v = memchr(c, ',', (size_t)(e - c));
        char campo[32];
        if (!v) v = e;
        if (normalizar(c, (size_t)(v - c), campo, sizeof campo) && !strcmp(campo, "fontname")) { fontAt = k; break; }
        k++; c = v + 1;
      }
    } else if (comeca(l, e, "style:") && fontAt >= 0) {
      const char *c = l + 6; int k = 0;
      while (c < e && k < fontAt) { const char *v = memchr(c, ',', (size_t)(e - c)); if (!v) { c = e; break; } c = v + 1; k++; }
      if (c < e) { const char *v = memchr(c, ',', (size_t)(e - c)); fontefam_por(s, c, (size_t)((v ? v : e) - c)); }
    }
  }
  /* \fn<nome> ate a proxima tag ou o fim do bloco; \fn vazio volta ao estilo. */
  for (p = doc; p + 3 <= fim; ) {
    const char *a, *b = memchr(p, '\\', (size_t)(fim - p));
    if (!b || b + 3 > fim) break;
    if (b[1] != 'f' || b[2] != 'n') { p = b + 1; continue; }
    a = b = b + 3;
    while (b < fim && *b != '\\' && *b != '}' && *b != '\n') b++;
    fontefam_por(s, a, (size_t)(b - a));
    p = b;
  }
}

/* ---- a tabela 'name' ---- */
static unsigned u16(const unsigned char *p) { return (unsigned)p[0] << 8 | p[1]; }
static unsigned long u32(const unsigned char *p) {
  return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
}

static int face(const unsigned char *d, size_t n, size_t off, void (*cb)(const char *, void *), void *u) {
  unsigned nt, i, fmt, cont, base;
  size_t tab = 0, tamTab = 0;
  int lidos = 0;
  if (off + 12 > n) return 0;
  nt = u16(d + off + 4);
  if (off + 12 + (size_t)nt * 16 > n) return 0;
  for (i = 0; i < nt; i++) {
    const unsigned char *r = d + off + 12 + (size_t)i * 16;
    if (!memcmp(r, "name", 4)) { tab = u32(r + 8); tamTab = u32(r + 12); break; }
  }
  if (!tamTab || tab + 6 > n || tab + tamTab > n) return 0;
  fmt = u16(d + tab); cont = u16(d + tab + 2); base = u16(d + tab + 4);
  (void)fmt;
  if (6 + (size_t)cont * 12 > tamTab) return 0;
  for (i = 0; i < cont; i++) {
    const unsigned char *r = d + tab + 6 + (size_t)i * 12;
    unsigned plat = u16(r), id = u16(r + 6), len = u16(r + 8), o = u16(r + 10);
    const unsigned char *s = d + tab + base + o;
    char nome[256];
    size_t k = 0, j;
    if (id != 1 && id != 4 && id != 6 && id != 16) continue;
    if ((size_t)base + o + len > tamTab) continue;
    if (plat == 0 || plat == 3) {   // UTF-16BE
      for (j = 0; j + 1 < len && k + 4 < sizeof nome; j += 2) {
        unsigned c = u16(s + j);
        if (c >= 0xD800 && c < 0xE000) { if (c < 0xDC00) j += 2; c = 0xFFFD; }   // fora do BMP: nao e nome de fonte
        if (c < 0x80) nome[k++] = (char)c;
        else if (c < 0x800) { nome[k++] = (char)(0xC0 | c >> 6); nome[k++] = (char)(0x80 | (c & 63)); }
        else { nome[k++] = (char)(0xE0 | c >> 12); nome[k++] = (char)(0x80 | (c >> 6 & 63)); nome[k++] = (char)(0x80 | (c & 63)); }
      }
    } else if (plat == 1) {         // Mac: bytes (o ASCII e o que importa)
      for (j = 0; j < len && k + 1 < sizeof nome; j++) nome[k++] = (char)s[j];
    } else continue;
    nome[k] = 0;
    if (k) { cb(nome, u); lidos++; }
  }
  return lidos;
}

int fontefam_nomes(const unsigned char *d, size_t n, void (*cb)(const char *nome, void *u), void *u) {
  int lidos = 0;
  if (!d || n < 12) return 0;
  if (!memcmp(d, "ttcf", 4)) {
    unsigned long nf = u32(d + 8), i;
    if (nf > 256 || 12 + nf * 4 > n) return 0;
    for (i = 0; i < nf; i++) lidos += face(d, n, u32(d + 12 + i * 4), cb, u);
    return lidos;
  }
  if (u32(d) == 0x00010000ul || !memcmp(d, "OTTO", 4) || !memcmp(d, "true", 4) || !memcmp(d, "typ1", 4))
    return face(d, n, 0, cb, u);
  return 0;
}

typedef struct { const FonteFamilias *s; int casou; } Quer;
static void querCb(const char *nome, void *u) {
  Quer *q = (Quer *)u;
  if (!q->casou && fontefam_tem(q->s, nome)) q->casou = 1;
}

int fontefam_quer(const FonteFamilias *s, const unsigned char *d, size_t n) {
  Quer q = { s, 0 };
  if (!fontefam_nomes(d, n, querCb, &q)) return 1;   // nao se le: entrega
  return q.casou;
}
