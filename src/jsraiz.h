// Tiny JSON helpers shared by the personal-server modules (jellyfin.c, plex.c).
// No allocation, no tree: they walk a response body that is already in memory.
// valorRaiz only sees depth-1 keys of the first object, because js_texto/js_num
// stop at the FIRST occurrence of a key and server items repeat "Id"/"Name"
// inside nested blocks (MediaSources, People, UserData, Media/Part...).
#ifndef NV_JSRAIZ_H
#define NV_JSRAIZ_H
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "js.h"

static inline void apagarSegredo(void *p, size_t n) {
  volatile unsigned char *v = p;
  while (n--) *v++ = 0;
}

static inline unsigned long long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}

static inline void copiar(char *dst, size_t tam, const char *src) {
  if (!tam) return;
  snprintf(dst, tam, "%s", src ? src : "");
}

// js_texto/js_num stop at the FIRST occurrence of a key, and Jellyfin items
// repeat "Id", "Name", "RunTimeTicks" inside MediaSources/People/UserData.
// This walks only depth-1 keys of the object starting at the first '{' in
// [ini,fim) and returns the start of the value.
static inline const char *valorRaiz(const char *ini, const char *fim, const char *chave) {
  const char *p = ini;
  size_t nk = strlen(chave);
  int prof = 0;
  if (!p) return NULL;
  if (!fim) fim = p + strlen(p);
  while (p < fim && *p != '{') p++;
  if (p >= fim) return NULL;
  for (; p < fim; p++) {
    char c = *p;
    if (c == '"') {
      const char *s = p + 1, *q = s;
      while (q < fim && *q != '"') { if (*q == '\\' && q + 1 < fim) q++; q++; }
      if (q >= fim) return NULL;
      if (prof == 1 && (size_t)(q - s) == nk && !memcmp(s, chave, nk)) {
        const char *v = q + 1;
        while (v < fim && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
        if (v < fim && *v == ':') {
          v++;
          while (v < fim && (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r')) v++;
          return v < fim ? v : NULL;
        }
      }
      p = q;
    } else if (c == '{' || c == '[') prof++;
    else if (c == '}' || c == ']') { if (--prof <= 0) return NULL; }
  }
  return NULL;
}

static inline int txtRaiz(const char *ini, const char *fim, const char *chave, char *dst, size_t tam) {
  const char *v = valorRaiz(ini, fim, chave);
  if (!v || *v != '"') return 0;
  return js_cadeia(v, dst, tam);
}
static inline double numRaiz(const char *ini, const char *fim, const char *chave, double padrao) {
  const char *v = valorRaiz(ini, fim, chave);
  char *e;
  double d;
  if (!v || !(*v == '-' || (*v >= '0' && *v <= '9'))) return padrao;
  d = strtod(v, &e);
  return e == v ? padrao : d;
}
static inline int boolRaiz(const char *ini, const char *fim, const char *chave, int padrao) {
  const char *v = valorRaiz(ini, fim, chave);
  if (!v) return padrao;
  if (!strncmp(v, "true", 4)) return 1;
  if (!strncmp(v, "false", 5)) return 0;
  return padrao;
}
// Object/array value: [*ini, *fim) of the value itself.
static inline int blocoRaiz(const char *ini, const char *fim, const char *chave,
                     const char **bi, const char **bf) {
  const char *v = valorRaiz(ini, fim, chave), *e;
  if (!v || (*v != '{' && *v != '[')) return 0;
  e = js_fim(v);
  if (!e) return 0;
  *bi = v; *bf = e;
  return 1;
}
// Iterates elements of an array value [ai, af): returns the next element start.
// Stops at '}' and NUL too: depoisElemento does not move on either of them, so a
// caller's loop would never end ("Guid":[1}], or an object where an array belongs).
static inline const char *elemento(const char *p, const char *af) {
  while (p && p < af && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t' || *p == '['))
    p++;
  if (!p || p >= af || !*p || *p == ']' || *p == '}') return NULL;
  return p;
}
static inline const char *depoisElemento(const char *p) {
  if (*p == '{' || *p == '[') return js_fim(p);
  if (*p == '"') {
    p++;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    return *p ? p + 1 : NULL;
  }
  while (*p && *p != ',' && *p != ']' && *p != '}') p++;
  return p;
}

#endif
