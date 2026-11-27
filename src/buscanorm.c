#include "buscanorm.h"
#include <string.h>

// Latin Extended-A (U+0100..U+017F) dobrado para ASCII; ' ' onde nao ha letra
// base (Ĳ, ĳ, Œ, œ). Gerada de NFD, com Đ/Ħ/Ł/Ŧ/Ŋ a mao.
static const char EXT_A[129] =
  "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiii  jjkkkllllll  llnnnnnnnnnoooooo  rrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";

static unsigned dobrarCp(unsigned cp) {
  if (cp < 0x80) return (cp >= 'A' && cp <= 'Z') ? cp + 32 : cp;
  if (cp >= 0xC0 && cp <= 0xFF) {
    if (cp == 0xD7 || cp == 0xF7) return ' ';
    if (cp >= 0xC0 && cp <= 0xC6) return 'a';
    if (cp == 0xC7) return 'c';
    if (cp >= 0xC8 && cp <= 0xCB) return 'e';
    if (cp >= 0xCC && cp <= 0xCF) return 'i';
    if (cp == 0xD0) return 'd';
    if (cp == 0xD1) return 'n';
    if ((cp >= 0xD2 && cp <= 0xD6) || cp == 0xD8) return 'o';
    if (cp >= 0xD9 && cp <= 0xDC) return 'u';
    if (cp == 0xDD) return 'y';
    if (cp >= 0xE0 && cp <= 0xE6) return 'a';
    if (cp == 0xE7) return 'c';
    if (cp >= 0xE8 && cp <= 0xEB) return 'e';
    if (cp >= 0xEC && cp <= 0xEF) return 'i';
    if (cp == 0xF0) return 'd';
    if (cp == 0xF1) return 'n';
    if ((cp >= 0xF2 && cp <= 0xF6) || cp == 0xF8) return 'o';
    if (cp >= 0xF9 && cp <= 0xFC) return 'u';
    if (cp == 0xFD || cp == 0xFF) return 'y';
    return ' ';
  }
  if (cp >= 0x100 && cp <= 0x17F) return (unsigned char)EXT_A[cp - 0x100];
  // Romeno com virgula (U+0218..U+021B): Ș ș Ț ț.
  if (cp == 0x218 || cp == 0x219) return 's';
  if (cp == 0x21A || cp == 0x21B) return 't';
  // Cirilico.
  if (cp >= 0x410 && cp <= 0x42F) return cp + 0x20;       /* А-Я -> а-я */
  if (cp >= 0x400 && cp <= 0x40F) return cp == 0x401 ? 0x435 : cp + 0x50;  /* Ѐ-Џ; Ё -> е */
  if (cp == 0x451) return 0x435;                          /* ё -> е */
  if (cp >= 0x490 && cp <= 0x4BF && !(cp & 1)) return cp + 1;  /* Ґ ґ ... */
  return cp;
}

// Um ponto de codigo de `p` (ate `fim`); avanca `*n` bytes. 0xFFFFFFFF = invalido.
static unsigned lerCp(const unsigned char *p, size_t resta, int *n) {
  unsigned c = p[0];
  int len, i;
  unsigned cp;
  if (c < 0x80) { *n = 1; return c; }
  if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
  else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0F; }
  else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07; }
  else { *n = 1; return 0xFFFFFFFFu; }
  if ((size_t)len > resta) { *n = 1; return 0xFFFFFFFFu; }
  for (i = 1; i < len; i++) {
    if ((p[i] & 0xC0) != 0x80) { *n = 1; return 0xFFFFFFFFu; }
    cp = (cp << 6) | (p[i] & 0x3F);
  }
  *n = len;
  return cp;
}

static int gravarCp(char *d, size_t k, size_t tam, unsigned cp) {
  if (cp < 0x80) { if (k + 1 >= tam) return 0; d[k] = (char)cp; return 1; }
  if (cp < 0x800) {
    if (k + 2 >= tam) return 0;
    d[k] = (char)(0xC0 | (cp >> 6)); d[k + 1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    if (k + 3 >= tam) return 0;
    d[k] = (char)(0xE0 | (cp >> 12)); d[k + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    d[k + 2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  if (k + 4 >= tam) return 0;
  d[k] = (char)(0xF0 | (cp >> 18)); d[k + 1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  d[k + 2] = (char)(0x80 | ((cp >> 6) & 0x3F)); d[k + 3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

void busca_normalizar(const char *s, char *destino, size_t tam) {
  const unsigned char *p = (const unsigned char *)s;
  size_t k = 0, resta = s ? strlen(s) : 0;
  if (!destino || !tam) return;
  while (resta > 0) {
    int n, g;
    unsigned cp = lerCp(p, resta, &n);
    cp = cp == 0xFFFFFFFFu ? ' ' : dobrarCp(cp);
    g = gravarCp(destino, k, tam, cp);
    if (!g) break;
    k += (size_t)g;
    p += n; resta -= (size_t)n;
  }
  destino[k] = 0;
}

int busca_codepoints(const char *s) {
  int n = 0;
  for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
  return n;
}

size_t busca_apagar_ultimo(char *s, size_t n) {
  while (n > 0) {
    n--;
    if (((unsigned char)s[n] & 0xC0) != 0x80) break;
  }
  s[n] = 0;
  return n;
}

size_t busca_byte_cursor(const char *s, int cur) {
  size_t b = 0;
  if (cur < 0) return strlen(s);
  while (s[b] && cur > 0) {
    b++;
    while (((unsigned char)s[b] & 0xC0) == 0x80) b++;
    cur--;
  }
  return b;
}

size_t busca_inserir(char *s, size_t n, size_t cap, int *cur, const char *t) {
  size_t k = strlen(t), b = busca_byte_cursor(s, *cur);
  if (n + k + 1 > cap) return n;
  memmove(s + b + k, s + b, n - b + 1);
  memcpy(s + b, t, k);
  if (*cur >= 0) *cur += busca_codepoints(t);
  return n + k;
}

size_t busca_apagar_antes(char *s, size_t n, int *cur) {
  size_t b, a;
  if (*cur < 0) return busca_apagar_ultimo(s, n);
  if (*cur == 0) return n;
  a = b = busca_byte_cursor(s, *cur);
  while (a > 0 && ((unsigned char)s[--a] & 0xC0) == 0x80) {}
  memmove(s + a, s + b, n - b + 1);
  (*cur)--;
  return n - (b - a);
}

int busca_cursor_mover(const char *s, int *cur, int d) {
  int total = busca_codepoints(s), c = *cur < 0 || *cur > total ? total : *cur;
  if (d < 0) { if (c == 0) return 0; c--; }
  else { if (c >= total) return 0; c++; }
  *cur = c >= total ? -1 : c;
  return 1;
}
