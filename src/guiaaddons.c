#include "guiaaddons.h"
#include <stdio.h>
#include <string.h>

int guiaaddons_oculto(const GuiaAddonsOcultos *o, const char *base) {
  if (!o || !base || !base[0]) return 0;
  for (int i = 0; i < o->n; i++)
    if (!strcmp(o->base[i], base)) return 1;
  return 0;
}

void guiaaddons_ler(GuiaAddonsOcultos *o, const char *texto) {
  const char *p = texto;
  o->n = 0;
  while (p && *p && o->n < GUIAADDONS_MAX) {
    const char *fim = strpbrk(p, "\r\n");
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    if (len > 0 && len < sizeof o->base[0]) {
      char tmp[sizeof o->base[0]];
      memcpy(tmp, p, len);
      tmp[len] = 0;
      if (!guiaaddons_oculto(o, tmp)) memcpy(o->base[o->n++], tmp, len + 1);
    }
    if (!fim) break;
    p = fim + 1;
  }
}

size_t guiaaddons_texto(const GuiaAddonsOcultos *o, char *dst, size_t n) {
  size_t w = 0;
  if (!dst || !n) return 0;
  dst[0] = 0;
  for (int i = 0; i < o->n; i++) {
    size_t len = strlen(o->base[i]);
    if (w + len + 2 > n) { dst[0] = 0; return 0; }
    memcpy(dst + w, o->base[i], len);
    w += len;
    dst[w++] = '\n';
    dst[w] = 0;
  }
  return w;
}

int guiaaddons_alternar(GuiaAddonsOcultos *o, const char *base) {
  if (!base || !base[0] || strlen(base) >= sizeof o->base[0]) return 0;
  for (int i = 0; i < o->n; i++)
    if (!strcmp(o->base[i], base)) {
      memmove(o->base[i], o->base[i + 1], (size_t)(o->n - i - 1) * sizeof o->base[0]);
      o->n--;
      return 0;
    }
  if (o->n >= GUIAADDONS_MAX) return 0;
  snprintf(o->base[o->n++], sizeof o->base[0], "%s", base);
  return 1;
}

const char *guiaaddons_arquivo(int perfil, char *dst, size_t n) {
  snprintf(dst, n, "guia-addons-p%d.txt", perfil > 0 ? perfil : 1);
  return dst;
}
