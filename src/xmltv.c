// XMLTV em fluxo — ver xmltv.h.
#include "xmltv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int xmltv_iniciar(Xmltv *x) {
  void (*c)(const char *, const char *const *, int, void *) = x->canal;
  void (*p)(const char *, const char *, const char *, const char *, void *) = x->programa;
  void (*ic)(const char *, const char *, void *) = x->icone;
  void *u = x->u;
  memset(x, 0, sizeof *x);
  x->canal = c; x->programa = p; x->icone = ic; x->u = u;
  x->inicio = 1;
  x->buf = malloc(XMLTV_ELEM + 1);
  return x->buf != NULL;
}

void xmltv_fim(Xmltv *x) {
  if (x->zOk) inflateEnd(&x->z);
  x->zOk = 0;
  free(x->buf); x->buf = NULL;
}

// --- tempo ------------------------------------------------------------------------
static long long diasDeCivil(int y, int m, int d) {
  long long era; unsigned yoe, doy, doe;
  y -= m <= 2;
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = (unsigned)(y - era * 400);
  doy = (153 * (unsigned)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long)doe - 719468;
}
long long xmltv_tempo(const char *s) {
  int y, m, d, hh, mm, ss = 0, i;
  long long seg;
  const char *f;
  for (i = 0; i < 12; i++) if (s[i] < '0' || s[i] > '9') return 0;
  y  = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
  m  = (s[4]-'0')*10 + (s[5]-'0');   d  = (s[6]-'0')*10 + (s[7]-'0');
  hh = (s[8]-'0')*10 + (s[9]-'0');   mm = (s[10]-'0')*10 + (s[11]-'0');
  f = s + 12;
  if (f[0] >= '0' && f[0] <= '9' && f[1] >= '0' && f[1] <= '9') { ss = (f[0]-'0')*10 + (f[1]-'0'); f += 2; }
  if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
  seg = diasDeCivil(y, m, d) * 86400 + hh * 3600 + mm * 60 + ss;
  while (*f == ' ') f++;
  if ((*f == '+' || *f == '-') && f[1] && f[2] && f[3] && f[4]) {
    int off = ((f[1]-'0')*10 + (f[2]-'0')) * 3600 + ((f[3]-'0')*10 + (f[4]-'0')) * 60;
    seg += (*f == '+') ? -off : off;
  }
  return seg;
}

void xmltv_desentidade(char *s) {
  char *r = s, *w = s;
  while (*r) {
    if (*r == '&') {
      char *p = strchr(r, ';');
      if (p && p - r < 10) {
        if      (!strncmp(r, "&amp;",  5)) { *w++ = '&';  r = p + 1; continue; }
        else if (!strncmp(r, "&lt;",   4)) { *w++ = '<';  r = p + 1; continue; }
        else if (!strncmp(r, "&gt;",   4)) { *w++ = '>';  r = p + 1; continue; }
        else if (!strncmp(r, "&quot;", 6)) { *w++ = '"';  r = p + 1; continue; }
        else if (!strncmp(r, "&apos;", 6)) { *w++ = '\''; r = p + 1; continue; }
        else if (r[1] == '#') {
          long cp = (r[2] == 'x' || r[2] == 'X') ? strtol(r + 3, NULL, 16) : atol(r + 2);
          r = p + 1;
          if (cp > 0 && cp < 0x80) { *w++ = (char)cp; continue; }
          if (cp >= 0x80 && cp < 0x800) { *w++ = (char)(0xC0 | (cp >> 6));
                                           *w++ = (char)(0x80 | (cp & 0x3F)); continue; }
          // 3 bytes so cabem quando a entidade tinha pelo menos 3 (sempre: "&#2048;").
          if (cp >= 0x800 && cp < 0x10000) { *w++ = (char)(0xE0 | (cp >> 12));
                                             *w++ = (char)(0x80 | ((cp >> 6) & 0x3F));
                                             *w++ = (char)(0x80 | (cp & 0x3F)); continue; }
          continue;
        }
      }
    }
    *w++ = *r++;
  }
  *w = 0;
}

// --- um elemento inteiro -------------------------------------------------------------
// Atributo `nome="v"` (ou com aspas simples) dentro da TAG DE ABERTURA [ini, fimTag).
static int atr(const char *ini, const char *fimTag, const char *nome, char *dst, size_t n) {
  size_t k = strlen(nome);
  const char *p = ini;
  dst[0] = 0;
  while (p < fimTag && (p = strstr(p, nome)) && p < fimTag) {
    const char *q = p + k;
    if ((p == ini || p[-1] == ' ' || p[-1] == '\t' || p[-1] == '\n' || p[-1] == '\r') && *q == '=' &&
        (q[1] == '"' || q[1] == '\'')) {
      char aspa = q[1];
      const char *v = q + 2, *f = strchr(v, aspa);
      size_t m;
      if (!f || f > fimTag) return 0;
      m = (size_t)(f - v);
      if (m >= n) m = n - 1;
      memcpy(dst, v, m); dst[m] = 0;
      xmltv_desentidade(dst);
      return 1;
    }
    p += k;
  }
  return 0;
}

// Texto do PRIMEIRO <tag ...>texto</tag> a partir de `p`; CDATA vira texto.
// Devolve onde o fechamento acabou (para achar o proximo), ou NULL.
static const char *texto(const char *p, const char *tag, char *dst, size_t n) {
  char abre[24], fecha[24];
  const char *a, *b, *c;
  size_t m;
  dst[0] = 0;
  snprintf(abre, sizeof abre, "<%s", tag);
  snprintf(fecha, sizeof fecha, "</%s>", tag);
  for (a = p; (a = strstr(a, abre)); a++) {
    char d = a[strlen(abre)];
    if (d == '>' || d == ' ' || d == '\t' || d == '\n' || d == '\r' || d == '/') break;
  }
  if (!a) return NULL;
  b = strchr(a, '>');
  if (!b) return NULL;
  if (b[-1] == '/') return b + 1;          // <title/>
  b++;
  c = strstr(b, fecha);
  if (!c) return NULL;
  if (!strncmp(b, "<![CDATA[", 9)) {
    const char *e = strstr(b + 9, "]]>");
    if (e && e < c) { b += 9; m = (size_t)(e - b); if (m >= n) m = n - 1; memcpy(dst, b, m); dst[m] = 0; return c + strlen(fecha); }
  }
  m = (size_t)(c - b);
  if (m >= n) m = n - 1;
  memcpy(dst, b, m); dst[m] = 0;
  xmltv_desentidade(dst);
  // espacos nas pontas
  { char *s = dst, *e = dst + strlen(dst);
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\t')) e--;
    memmove(dst, s, (size_t)(e - s)); dst[e - s] = 0; }
  return c + strlen(fecha);
}

static void elemento(Xmltv *x) {
  char *b = x->buf;
  const char *fimTag;
  b[x->n] = 0;
  fimTag = strchr(b, '>');
  if (!fimTag) return;
  if (x->dentro == 1) {
    char id[160], nomes[XMLTV_NOMES][160];
    const char *ptr[XMLTV_NOMES], *p = fimTag;
    int k = 0;
    x->nCanais++;
    if (!atr(b, fimTag, "id", id, sizeof id) || !id[0]) return;
    while (k < XMLTV_NOMES && p && (p = texto(p, "display-name", nomes[k], sizeof nomes[k]))) {
      if (nomes[k][0]) { ptr[k] = nomes[k]; k++; }
    }
    if (x->canal) x->canal(id, ptr, k, x->u);
    if (x->icone) {
      const char *ic = strstr(fimTag, "<icon"), *fi = ic ? strchr(ic, '>') : NULL;
      char src[600];
      if (fi && atr(ic, fi, "src", src, sizeof src) && src[0]) x->icone(id, src, x->u);
    }
  } else {
    char canal[160], ini[40], fim[40], titulo[240];
    x->nProgramas++;
    if (!atr(b, fimTag, "channel", canal, sizeof canal) ||
        !atr(b, fimTag, "start", ini, sizeof ini)) return;
    atr(b, fimTag, "stop", fim, sizeof fim);
    texto(fimTag, "title", titulo, sizeof titulo);
    if (x->programa) x->programa(canal, ini, fim, titulo, x->u);
  }
}

// Byte a byte, sobre o XML ja aberto. Fora de elemento so se procura '<' e o
// nome da tag; dentro, acumula ate o fechamento.
static void xml(Xmltv *x, const char *d, size_t n) {
  size_t i;
  x->bytesXml += (long long)n;
  for (i = 0; i < n; i++) {
    char c = d[i];
    if (!x->dentro) {
      if (x->lendoNome) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == ':') {
          if (x->nNome < (int)sizeof x->nome - 1) x->nome[x->nNome++] = c;
          else x->lendoNome = 0;
          continue;
        }
        x->nome[x->nNome] = 0;
        x->lendoNome = 0;
        if (!strcmp(x->nome, "channel") || !strcmp(x->nome, "programme")) {
          x->dentro = x->nome[0] == 'c' ? 1 : 2;
          x->pulando = 0;
          x->n = (size_t)snprintf(x->buf, XMLTV_ELEM, "<%s", x->nome);
          // o caractere que encerrou o nome pertence ao elemento
        } else continue;
      } else {
        if (c == '<') { x->lendoNome = 1; x->nNome = 0; }
        continue;
      }
    }
    // dentro de um elemento
    memmove(x->cauda, x->cauda + 1, sizeof x->cauda - 1);
    x->cauda[sizeof x->cauda - 1] = c;
    if (!x->pulando) {
      if (x->n < XMLTV_ELEM) x->buf[x->n++] = c;
      else { x->pulando = 1; x->nPulados++; }
    }
    if (c == '>') {
      // Fim do elemento: "/>" logo na tag de abertura, ou o fechamento — este
      // pela cauda dos ultimos bytes, que vale tambem para o elemento pulado
      // e para o fechamento partido entre dois pedacos.
      const char *fecha = x->dentro == 1 ? "</channel>" : "</programme>";
      size_t k = strlen(fecha);
      int acabou = !memcmp(x->cauda + sizeof x->cauda - k, fecha, k);
      if (!acabou && !x->pulando && x->n >= 2 && x->buf[x->n - 2] == '/' &&
          !memchr(x->buf, '>', x->n - 1)) acabou = 1;
      if (acabou) {
        if (!x->pulando) elemento(x);
        x->dentro = 0; x->n = 0; x->pulando = 0;
        memset(x->cauda, 0, sizeof x->cauda);
      }
    }
  }
}

int xmltv_alimentar(Xmltv *x, const char *dados, size_t n) {
  if (!x->buf || x->erroZ) return 0;
  x->bytesBrutos += (long long)n;
  if (x->inicio && n) {
    // gzip pelos dois primeiros bytes; um pedaco de 1 byte so no comeco e
    // raro demais para tratar (e cai como XML puro, que nao comeca com 0x1f).
    x->inicio = 0;
    if (n >= 2 && (unsigned char)dados[0] == 0x1f && (unsigned char)dados[1] == 0x8b) {
      memset(&x->z, 0, sizeof x->z);
      if (inflateInit2(&x->z, 16 + 15) != Z_OK) { x->erroZ = 1; return 0; }
      x->zOk = 1; x->gz = 1;
    }
  }
  if (!x->gz) { xml(x, dados, n); return 1; }
  {
    char saida[16384];
    x->z.next_in = (Bytef *)dados;
    x->z.avail_in = (uInt)n;
    while (x->z.avail_in > 0) {
      int r;
      x->z.next_out = (Bytef *)saida;
      x->z.avail_out = sizeof saida;
      r = inflate(&x->z, Z_NO_FLUSH);
      if (sizeof saida - x->z.avail_out) xml(x, saida, sizeof saida - x->z.avail_out);
      if (r == Z_STREAM_END) {
        // gzip concatenado (alguns paineis): recomeca no mesmo fluxo.
        if (x->z.avail_in > 0) { inflateReset(&x->z); continue; }
        break;
      }
      if (r != Z_OK && r != Z_BUF_ERROR) { x->erroZ = 1; return 0; }
      if (r == Z_BUF_ERROR && x->z.avail_out != 0) break;
    }
  }
  return 1;
}
