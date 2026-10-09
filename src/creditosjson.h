// Limites explicitos de creditos. A leitura manual de intro.c continua tolerante.
#ifndef NV_CREDITOSJSON_H
#define NV_CREDITOSJSON_H
#include "intro.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static inline const char *cj_espaco(const char *p, const char *f) {
  while (p < f && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
  return p;
}
static inline const char *cj_string(const char *p, const char *f) {
  if (p >= f || *p++ != '"') return NULL;
  while (p < f) {
    unsigned char c = (unsigned char)*p++;
    if (c == '"') return p;
    if (c < 32) return NULL;
    if (c == '\\') {
      if (p >= f) return NULL;
      c = (unsigned char)*p++;
      if (c == 'u') {
        for (int i = 0; i < 4; i++, p++)
          if (p >= f || !strchr("0123456789abcdefABCDEF", *p)) return NULL;
      } else if (!strchr("\"\\/bfnrt", c)) return NULL;
    }
  }
  return NULL;
}
static inline const char *cj_numero(const char *p, const char *f) {
  if (p < f && *p == '-') p++;
  if (p >= f) return NULL;
  if (*p == '0') p++;
  else { if (*p < '1' || *p > '9') return NULL; while (p < f && *p >= '0' && *p <= '9') p++; }
  if (p < f && *p == '.') {
    p++; if (p >= f || *p < '0' || *p > '9') return NULL;
    while (p < f && *p >= '0' && *p <= '9') p++;
  }
  if (p < f && (*p == 'e' || *p == 'E')) {
    p++; if (p < f && (*p == '+' || *p == '-')) p++;
    if (p >= f || *p < '0' || *p > '9') return NULL;
    while (p < f && *p >= '0' && *p <= '9') p++;
  }
  return p;
}
// So percorre os valores para validar a estrutura e delimitar credits[].
static inline const char *cj_valor(const char *p, const char *f, int nivel) {
  p = cj_espaco(p, f); if (p >= f || nivel > 16) return NULL;
  if (*p == '"') return cj_string(p, f);
  if (*p == '{' || *p == '[') {
    int objeto = *p == '{'; char fecha = objeto ? '}' : ']';
    p = cj_espaco(p + 1, f); if (p < f && *p == fecha) return p + 1;
    for (;;) {
      if (objeto) {
        p = cj_string(p, f); if (!p) return NULL;
        p = cj_espaco(p, f); if (p >= f || *p++ != ':') return NULL;
      }
      p = cj_valor(p, f, nivel + 1); if (!p) return NULL;
      p = cj_espaco(p, f); if (p >= f) return NULL;
      if (*p == fecha) return p + 1;
      if (*p++ != ',') return NULL;
      p = cj_espaco(p, f);
    }
  }
  if (f - p >= 4 && (!memcmp(p, "null", 4) || !memcmp(p, "true", 4))) return p + 4;
  if (f - p >= 5 && !memcmp(p, "false", 5)) return p + 5;
  return cj_numero(p, f);
}
// Campo direto e unico de um objeto validado. Chaves escapadas recusam a
// verificacao: podem ser outra grafia da mesma chave ASCII.
static inline int cj_campo(const char *p, const char *f, const char *chave,
                            const char **inicio, const char **fim) {
  size_t n = strlen(chave); int achou = 0;
  p = cj_espaco(p, f); if (p >= f || *p++ != '{') return 0;
  p = cj_espaco(p, f);
  while (p < f && *p != '}') {
    const char *nome = p, *nf = cj_string(p, f), *vf;
    if (!nf || memchr(nome, '\\', (size_t)(nf - nome))) return 0;
    p = cj_espaco(nf, f); if (p >= f || *p++ != ':') return 0;
    p = cj_espaco(p, f); vf = cj_valor(p, f, 1); if (!vf) return 0;
    if ((size_t)(nf - nome) == n + 2 && !memcmp(nome + 1, chave, n)) {
      if (achou) return 0;
      achou = 1; *inicio = p; *fim = vf;
    }
    p = cj_espaco(vf, f); if (p < f && *p == ',') p = cj_espaco(p + 1, f); else break;
  }
  return achou;
}
static inline int cj_ms(const char *p, const char *f, double *seg) {
  char numero[64], *convertido; size_t n = (size_t)(f - p); double v;
  if (!n || n >= sizeof numero || *p == '-' || cj_numero(p, f) != f) return 0;
  memcpy(numero, p, n); numero[n] = 0; v = strtod(numero, &convertido);
  if (*convertido || !isfinite(v) || v < 0.0) return 0;
  *seg = v / 1000.0; return 1;
}
static inline int creditosjson_extrair(const char *j, IntroTrecho *out, int max) {
  const char *f, *fim, *p, *a, *b; int n = 0;
  if (!j || !out || max < 1) return 0;
  f = j + strlen(j); p = cj_espaco(j, f);
  if (p >= f || *p != '{' || !(fim = cj_valor(p, f, 0)) || cj_espaco(fim, f) != f ||
      !cj_campo(p, fim, "credits", &a, &b) || *a != '[') return 0;
  p = cj_espaco(a + 1, b);
  while (p < b && *p != ']' && n < max) {
    const char *v = cj_valor(p, b, 1), *ini, *iniFim, *end, *endFim;
    double inicio, final;
    if (!v) return 0;
    if (*p == '{' && cj_campo(p, v, "start_ms", &ini, &iniFim) &&
        cj_campo(p, v, "end_ms", &end, &endFim) &&
        cj_ms(ini, iniFim, &inicio) && cj_ms(end, endFim, &final) && final > inicio) {
      out[n++] = (IntroTrecho){ inicio, final, INTRO_CREDITOS };
    }
    p = cj_espaco(v, b); if (p < b && *p == ',') p = cj_espaco(p + 1, b); else break;
  }
  return n;
}
#endif
