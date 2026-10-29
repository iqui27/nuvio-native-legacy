#include "importar.h"
#include "js.h"
#include <stdio.h>
#include <string.h>

// stremio:// -> https://. Keeps only http(s) URLs whose path ends in manifest.json.
static int manifesto(const char *in, char *out, size_t n) {
  size_t l;
  const char *q;
  int w;
  if (!strncmp(in, "stremio://", 10)) w = snprintf(out, n, "https://%s", in + 10);
  else if (!strncmp(in, "https://", 8) || !strncmp(in, "http://", 7)) w = snprintf(out, n, "%s", in);
  else return 0;
  if (w < 0 || (size_t)w >= n) return 0;
  l = (size_t)w;
  q = strpbrk(out, "?#");
  if (q) l = (size_t)(q - out);
  return l >= 13 && !strncmp(out + l - 13, "manifest.json", 13);
}

void importar_host(const char *url, char *dst, size_t n) {
  const char *p = url ? strstr(url, "://") : NULL;
  size_t l;
  if (!n) return;
  dst[0] = 0;
  if (!p) return;
  p += 3;
  l = strcspn(p, "/:?#");
  if (l >= n) l = n - 1;
  memcpy(dst, p, l);
  dst[l] = 0;
}

static int juntar(AddonRemoto *dst, int n, int max, const char *url, const char *nome) {
  int i;
  char u[NV_ADDON_URL_MAX];
  if (n >= max || !manifesto(url, u, sizeof u)) return n;
  for (i = 0; i < n; i++) if (!strcmp(dst[i].url, u)) return n;
  memset(&dst[n], 0, sizeof dst[n]);
  snprintf(dst[n].url, sizeof dst[n].url, "%s", u);
  if (nome && nome[0]) snprintf(dst[n].nome, sizeof dst[n].nome, "%s", nome);
  else importar_host(u, dst[n].nome, sizeof dst[n].nome);
  dst[n].ativo = 1;
  return n + 1;
}

int importar_stremio_addons(const char *json, AddonRemoto *dst, int max) {
  static char url[NV_ADDON_URL_MAX];
  char nome[64];
  const char *p = json ? js_array(json, NULL, "addons") : NULL;
  int n = 0;
  for (; p && *p == '{'; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if (!js_texto(p, f, "transportUrl", url, sizeof url)) continue;
    nome[0] = 0;
    js_texto(p, f, "name", nome, sizeof nome);
    n = juntar(dst, n, max, url, nome);
  }
  return n;
}

int importar_stremio_biblioteca(const char *json, ImpTitulo *dst, int max) {
  const char *p = js_raiz_array(json);
  char b[8], id[24];
  int n = 0;
  if (!p) p = json ? js_array(json, NULL, "result") : NULL;
  for (; p && *p == '{' && n < max; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    if ((js_bruto(p, f, "removed", b, sizeof b) && !strcmp(b, "true")) ||
        (js_bruto(p, f, "temp", b, sizeof b) && !strcmp(b, "true"))) continue;
    if (!js_texto(p, f, "_id", id, sizeof id) || strncmp(id, "tt", 2)) continue;
    snprintf(dst[n].imdb, sizeof dst[n].imdb, "%s", id);
    dst[n].tipo[0] = dst[n].nome[0] = 0;
    js_texto(p, f, "type", dst[n].tipo, sizeof dst[n].tipo);
    js_texto(p, f, "name", dst[n].nome, sizeof dst[n].nome);
    n++;
  }
  return n;
}

int importar_aiom_addons(const char *txt, AddonRemoto *dst, int max) {
  static char url[NV_ADDON_URL_MAX];
  char nome[64];
  const char *p;
  int n = 0;
  if (!txt) return 0;
  p = js_raiz_array(txt);
  if (!p) p = js_array(txt, NULL, "addons");
  if (p) {
    while (p && n < max) {
      const char *f = p;
      nome[0] = 0;
      if (*p == '"') {
        if (js_cadeia(p, url, sizeof url)) n = juntar(dst, n, max, url, NULL);
        for (f = p + 1; *f && *f != '"'; f++) if (*f == '\\' && f[1]) f++;
        if (*f) f++;
      } else if (*p == '{') {
        f = js_fim(p);
        if (js_texto(p, f, "transportUrl", url, sizeof url) ||
            js_texto(p, f, "manifestUrl", url, sizeof url) ||
            js_texto(p, f, "url", url, sizeof url)) {
          js_texto(p, f, "name", nome, sizeof nome);
          n = juntar(dst, n, max, url, nome);
        }
      } else break;
      p = js_prox(f);
    }
    return n;
  }
  for (p = txt; *p && n < max;) {
    size_t l = strcspn(p, "\r\n");
    if (l && l < sizeof url) {
      memcpy(url, p, l);
      url[l] = 0;
      n = juntar(dst, n, max, url, NULL);
    }
    p += l;
    while (*p == '\r' || *p == '\n') p++;
  }
  return n;
}
