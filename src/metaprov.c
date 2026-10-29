#include "metaprov.h"
#include "ajustes.h"
#include "rede.h"
#include "marco.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Apos uma falha de REDE (sem resposta ou 5xx) o Nuvio descansa um minuto: sem
// isto cada ficha esperaria 5 s antes de ir ao Cinemeta, e o Continuar
// assistindo (varios pedidos em paralelo) ficaria 5 s mais lento por pedido.
// 404/JSON invalido NAO contam: id desconhecido nao e provedor fora do ar.
static pthread_mutex_t pausaTrava = PTHREAD_MUTEX_INITIALIZER;
static time_t pausaAte;

static int nuvioPausado(void) {
  int p;
  pthread_mutex_lock(&pausaTrava);
  p = pausaAte && time(NULL) < pausaAte;
  pthread_mutex_unlock(&pausaTrava);
  return p;
}
static void nuvioPausar(void) {
  pthread_mutex_lock(&pausaTrava);
  pausaAte = time(NULL) + METAPROV_PAUSA_S;
  pthread_mutex_unlock(&pausaTrava);
}
void metaprov_zerar_pausa(void) {
  pthread_mutex_lock(&pausaTrava);
  pausaAte = 0;
  pthread_mutex_unlock(&pausaTrava);
}

// rede_baixar entrega NULL para tudo que nao e 2xx, entao aqui "sem corpo" =
// st 0, que pausa o Nuvio. Por isso so vai ao Nuvio o id que o manifesto dele
// declara (idDoNuvio): id desconhecido "tt..." volta 200 com {"meta":{}}
// (medido), e um "kitsu:"/"mal:" de anime nem sai daqui — um 404 dele
// pausaria o provedor por um minuto para todo mundo.
char *metaprov_get_rede(const char *url, int seg, int *st, void *ctx) {
  Uint32 desde = SDL_GetTicks();
  char msg[240];
  const char *host = strstr(url, "://");
  host = host ? host + 3 : "local";
  size_t nHost = strcspn(host, "/?#");
  const char *at = memchr(host, '@', nHost);
  if (at) { nHost -= (size_t)(at + 1 - host); host = at + 1; }
  char *c = rede_baixar(url, seg);
  (void)ctx;
  snprintf(msg, sizeof msg, "meta: origem fonte=rede host=%.*s total_ms=%u ok=%d",
           (int)nHost, host, (unsigned)(SDL_GetTicks() - desde), c != NULL);
  marco(msg);
  if (st) *st = c ? 200 : 0;
  return c;
}

// idPrefixes do manifesto (catalog.nuvio.tv/manifest.json): "tt" e "tmdb:".
static int idDoNuvio(const char *id) {
  return id && (!strncmp(id, "tt", 2) || !strncmp(id, "tmdb:", 5));
}

// So [A-Za-z0-9-]: o codigo vai dentro de JSON e de URL.
static int idiomaOk(const char *l) {
  size_t i;
  if (!l || !l[0] || strlen(l) > 11) return 0;
  for (i = 0; l[i]; i++) {
    char c = l[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-')) return 0;
  }
  return 1;
}

const char *metaprov_idioma(void) {
  const char *l = ajustes_tmdb_idioma();
  return idiomaOk(l) ? l : "en-US";
}

static void escapar(const char *s, char *dst, size_t n, int manterDoisPontos) {
  static const char HEX[] = "0123456789ABCDEF";
  size_t o = 0;
  if (!n) return;
  for (; s && *s && o + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' ||
        (manterDoisPontos && c == ':')) {
      dst[o++] = (char)c;
    } else {
      dst[o++] = '%'; dst[o++] = HEX[c >> 4]; dst[o++] = HEX[c & 15];
    }
  }
  dst[o] = 0;
}

void metaprov_base_nuvio(char *dst, size_t n, const char *lang) {
  if (!idiomaOk(lang)) lang = "en-US";
  snprintf(dst, n, "%s/%%7B%%22language%%22%%3A%%22%s%%22%%7D",
           METAPROV_NUVIO_HOST, lang);
}

void metaprov_url_meta(char *dst, size_t n, int prov, const char *tipo,
                       const char *id, const char *lang) {
  char base[160], eid[200];
  escapar(id, eid, sizeof eid, 1);
  if (prov == METAPROV_NUVIO) metaprov_base_nuvio(base, sizeof base, lang);
  else snprintf(base, sizeof base, "%s", METAPROV_CINEMETA_URL);
  snprintf(dst, n, "%s/meta/%s/%s.json", base, tipo ? tipo : "", eid);
}

void metaprov_url_busca(char *dst, size_t n, int prov, const char *tipo,
                        const char *termo, const char *lang) {
  char base[160], esc[300];
  const char *cat;
  escapar(termo, esc, sizeof esc, 0);
  if (prov == METAPROV_NUVIO) {
    metaprov_base_nuvio(base, sizeof base, lang);
    cat = (tipo && !strcmp(tipo, "series")) ? "popular-series" : "popular-movies";
  } else {
    snprintf(base, sizeof base, "%s", METAPROV_CINEMETA_URL);
    cat = "top";
  }
  snprintf(dst, n, "%s/catalog/%s/%s/search=%s.json", base, tipo ? tipo : "", cat, esc);
}

void metaprov_duracao(char *s, size_t n) {
  int v = 0, k = 0;
  const char *p;
  if (!s || !n) return;
  for (p = s; *p >= '0' && *p <= '9' && k < 5; p++, k++) v = v * 10 + (*p - '0');
  if (!k || v <= 0) return;
  while (*p == ' ') p++;
  if (*p == 'm' && (!p[1] || !strcmp(p, "min"))) snprintf(s, n, "%d min", v);
}

void metaprov_chave(char *dst, size_t n, const char *tipo, const char *id) {
  snprintf(dst, n, "nuvio:%s/%s/%s", metaprov_idioma(), tipo ? tipo : "",
           id ? id : "");
}

static const char *pulaEsp(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  return p;
}

int metaprov_meta_valido(const char *corpo) {
  const char *m, *p;
  if (!corpo || !(m = strstr(corpo, "\"meta\""))) return 0;
  p = pulaEsp(m + 6);
  if (*p != ':') return 0;
  p = pulaEsp(p + 1);
  if (*p != '{') return 0;               // null, string, array: nada
  p = pulaEsp(p + 1);
  if (*p == '}') return 0;               // {} vazio
  return strstr(p, "\"name\"") != NULL;  // sem titulo nao serve de ficha
}

int metaprov_busca_valida(const char *corpo) {
  const char *m, *p;
  if (!corpo || !(m = strstr(corpo, "\"metas\""))) return 0;
  p = pulaEsp(m + 7);
  if (*p != ':') return 0;
  return *pulaEsp(p + 1) == '[';
}

static int falhaDeRede(int st) { return st == 0 || st >= 500; }

char *metaprov_meta_com(const char *tipo, const char *id, int seg_cine,
                        MetaprovGet get, void *ctx, int *prov) {
  char url[400], *c;
  int st = 0, segN = seg_cine > 0 && seg_cine < METAPROV_NUVIO_S ? seg_cine
                                                                 : METAPROV_NUVIO_S;
  if (!get) get = metaprov_get_rede;
  if (prov) *prov = -1;
  if (!tipo || !id || !id[0]) return NULL;
  if (idDoNuvio(id) && !nuvioPausado()) {
    metaprov_url_meta(url, sizeof url, METAPROV_NUVIO, tipo, id, metaprov_idioma());
    c = get(url, segN, &st, ctx);
    if (c && metaprov_meta_valido(c)) {
      if (prov) *prov = METAPROV_NUVIO;
      printf("[meta] %s/%s: catalogo do Nuvio (%s)\n", tipo, id, metaprov_idioma());
      return c;
    }
    free(c);
    if (falhaDeRede(st)) nuvioPausar();
    printf("[meta] %s/%s: catalogo do Nuvio sem ficha (http %d); Cinemeta\n",
           tipo, id, st);
  }
  metaprov_url_meta(url, sizeof url, METAPROV_CINEMETA, tipo, id, NULL);
  c = get(url, seg_cine, &st, ctx);
  if (c) {
    if (prov) *prov = METAPROV_CINEMETA;
    printf("[meta] %s/%s: Cinemeta\n", tipo, id);
  }
  return c;
}

// Validate the entire response before either positive or negative caching.
// The shared metadata readers intentionally tolerate incomplete JSON.
static const char *cineJson(const char *p, int depth, const char **meta, int *error) {
  const char *q;
  char close;
  if (depth > 64) return NULL;
  p = pulaEsp(p);
  if (*p == '"') {
    for (p++; *p && *p != '"'; p++) {
      if ((unsigned char)*p < 32) return NULL;
      if (*p == '\\') {
        p++;
        if (!*p) return NULL;
        if (*p == 'u') {
          for (int i = 0; i < 4; i++) {
            p++;
            if (!*p || !strchr("0123456789abcdefABCDEF", *p)) return NULL;
          }
        } else if (!strchr("\"\\/bfnrt", *p)) return NULL;
      }
    }
    return *p == '"' ? p + 1 : NULL;
  }
  if (*p == '{' || *p == '[') {
    close = *p == '{' ? '}' : ']';
    p = pulaEsp(p + 1);
    if (*p == close) return p + 1;
    for (;;) {
      if (close == '}') {
        if (*p != '"' || !(q = cineJson(p, depth + 1, NULL, NULL))) return NULL;
        int isMeta = meta && q - p == 6 && !memcmp(p, "\"meta\"", 6);
        if (error && q - p == 7 && !memcmp(p, "\"error\"", 7)) *error = 1;
        p = pulaEsp(q);
        if (*p != ':') return NULL;
        p = pulaEsp(p + 1);
        if (isMeta) *meta = p;
      }
      if (!(p = cineJson(p, depth + 1, NULL, NULL))) return NULL;
      p = pulaEsp(p);
      if (*p == close) return p + 1;
      if (*p != ',') return NULL;
      p = pulaEsp(p + 1);
    }
  }
  if (!strncmp(p, "null", 4) || !strncmp(p, "true", 4)) return p + 4;
  if (!strncmp(p, "false", 5)) return p + 5;
  if (*p == '-') p++;
  if (*p == '0') p++;
  else {
    if (*p < '1' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  if (*p == '.') {
    p++;
    if (*p < '0' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  if (*p == 'e' || *p == 'E') {
    p++;
    if (*p == '+' || *p == '-') p++;
    if (*p < '0' || *p > '9') return NULL;
    do { p++; } while (*p >= '0' && *p <= '9');
  }
  return p;
}

char *metaprov_meta_cinemeta_com(const char *tipo, const char *id, int seg,
                                 MetaprovGet get, void *ctx, MetaprovCineResult *result) {
  char url[400], *c;
  int st = 0;
  if (result) *result = METAPROV_CINE_RETRY;
  if (!get) get = metaprov_get_rede;
  if (!tipo || !id || !id[0]) return NULL;
  metaprov_url_meta(url, sizeof url, METAPROV_CINEMETA, tipo, id, NULL);
  c = get(url, seg, &st, ctx);
  if (c && st >= 200 && st < 300) {
    int error = 0;
    const char *meta = NULL, *root = pulaEsp(c);
    const char *end = *root == '{' ? cineJson(root, 0, &meta, &error) : NULL;
    if (!end || *pulaEsp(end) || error || !meta || *meta != '{') {
      free(c); return NULL;
    }
    if (*pulaEsp(meta + 1) == '}') {
      if (result) *result = METAPROV_CINE_ABSENT;
      free(c); c = NULL;
    } else if (!metaprov_meta_valido(c)) {
      free(c); c = NULL;
    } else if (result) *result = METAPROV_CINE_FOUND;
  } else { free(c); c = NULL; }
  return c;
}

char *metaprov_meta(const char *tipo, const char *id, int seg_cine, int *prov) {
  return metaprov_meta_com(tipo, id, seg_cine, metaprov_get_rede, NULL, prov);
}

char *metaprov_busca_com(const char *tipo, const char *termo, int seg_nuvio,
                         int seg_cine, MetaprovGet get, void *ctx, int *prov) {
  char url[600], *c;
  int st = 0;
  if (!get) get = metaprov_get_rede;
  if (prov) *prov = -1;
  if (!tipo || !termo) return NULL;
  if (!nuvioPausado()) {
    metaprov_url_busca(url, sizeof url, METAPROV_NUVIO, tipo, termo, metaprov_idioma());
    c = get(url, seg_nuvio > 0 ? seg_nuvio : METAPROV_NUVIO_S, &st, ctx);
    if (c && metaprov_busca_valida(c)) {
      if (prov) *prov = METAPROV_NUVIO;
      return c;
    }
    free(c);
    if (falhaDeRede(st)) nuvioPausar();
    printf("[meta] busca %s: Nuvio falhou (http %d); Cinemeta\n", tipo, st);
  }
  if (seg_cine < 0) return NULL;   // #231: Cinemeta fora da busca
  metaprov_url_busca(url, sizeof url, METAPROV_CINEMETA, tipo, termo, NULL);
  c = get(url, seg_cine, &st, ctx);
  if (c && !metaprov_busca_valida(c)) { free(c); c = NULL; }
  if (c && prov) *prov = METAPROV_CINEMETA;
  return c;
}
