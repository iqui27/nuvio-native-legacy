#include "deeplink.h"
#include <pthread.h>
#include <string.h>

static int cmpPrefixo(const char *s, const char *p) {
  for (; *p; s++, p++) {
    char c = *s;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    if (c != *p) return 0;
  }
  return 1;
}

static int ehDigito(char c) { return c >= '0' && c <= '9'; }

// Numero decimal de 1 a `max` ocupando o segmento inteiro [s, s+n).
static int numero(const char *s, size_t n, long max, long *v) {
  long r = 0;
  size_t i;
  if (n == 0 || n > 9) return 0;
  for (i = 0; i < n; i++) {
    if (!ehDigito(s[i])) return 0;
    r = r * 10 + (s[i] - '0');
  }
  if (r > max) return 0;
  *v = r;
  return 1;
}

static int tipoMidia(const char *s, size_t n, int *serie) {
  if ((n == 5 && !strncmp(s, "movie", 5)) || (n == 4 && !strncmp(s, "film", 4))) { *serie = 0; return 1; }
  if ((n == 6 && !strncmp(s, "series", 6)) || (n == 2 && !strncmp(s, "tv", 2)) ||
      (n == 4 && !strncmp(s, "show", 4))) { *serie = 1; return 1; }
  return 0;
}

// "tt1234567", "tmdb:603" ou "603" (TMDB). Charset fechado: o id vai para URLs.
static int idTitulo(const char *s, size_t n, DeepLink *o) {
  long v;
  if (n >= 4 && n <= 12 && s[0] == 't' && s[1] == 't') {
    size_t i;
    for (i = 2; i < n; i++) if (!ehDigito(s[i])) return 0;
    memcpy(o->imdb, s, n); o->imdb[n] = 0;
    return 1;
  }
  if (n > 5 && !strncmp(s, "tmdb:", 5)) { s += 5; n -= 5; }
  if (!numero(s, n, 99999999L, &v) || v <= 0) return 0;
  o->tmdb = v;
  return 1;
}

static int hex(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// q=... : '+' e %XX decodificados; controles viram espaco; cortado sem meia
// sequencia UTF-8; espacos das pontas aparados. 1 se sobrou texto.
static int consultaDe(const char *query, DeepLink *o) {
  const char *p = strstr(query, "q=");
  size_t w = 0;
  if (!p || (p != query && p[-1] != '&')) return 0;
  for (p += 2; *p && *p != '&' && *p != '#' && w + 1 < sizeof o->consulta; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '%' && hex(p[1]) >= 0 && hex(p[2]) >= 0) { c = (unsigned char)(hex(p[1]) * 16 + hex(p[2])); p += 2; }
    else if (c == '+') c = ' ';
    if (c < 32 || c == 127) c = ' ';
    o->consulta[w++] = (char)c;
  }
  if (*p && *p != '&' && *p != '#' && w > 0) {   // cortou: nao deixa meia sequencia UTF-8
    while (w > 1 && ((unsigned char)o->consulta[w - 1] & 0xC0) == 0x80) w--;
    if ((unsigned char)o->consulta[w - 1] >= 0xC0) w--;
  }
  while (w > 0 && o->consulta[w - 1] == ' ') w--;
  o->consulta[w] = 0;
  { size_t i = 0; while (i < w && o->consulta[i] == ' ') i++;
    if (i) memmove(o->consulta, o->consulta + i, w - i + 1); }
  return o->consulta[0] != 0;
}

// Segmentos de `path` (sem query/fragmento) em seg[]/len[]; max `cap`.
// -1 se passar de `cap`. Segmento vazio no meio ("a//b") e invalido.
static int segmentos(const char *path, size_t pn, const char **seg, size_t *len, int cap) {
  int n = 0;
  size_t i = 0;
  while (i < pn && path[i] == '/') i++;
  while (i < pn) {
    size_t j = i;
    while (j < pn && path[j] != '/') j++;
    if (n == cap) return -1;
    seg[n] = path + i; len[n] = j - i; n++;
    if (j < pn) { j++; if (j < pn && path[j] == '/') return -1; }
    i = j;
  }
  return n;
}

int deeplink_parse(const char *url, DeepLink *o) {
  const char *seg[6], *query, *fim, *p;
  size_t len[6], pn;
  int n, nativo = 0, imdbWeb = 0;
  long t, e;

  memset(o, 0, sizeof *o);
  if (!url || strnlen(url, DL_URL_MAX + 1) > DL_URL_MAX) return 0;

  if (cmpPrefixo(url, "nuvio-legacy://")) { p = url + 15; nativo = 1; }
  else if (cmpPrefixo(url, "stremio://")) p = url + 10;
  else if (cmpPrefixo(url, "https://www.imdb.com/") || cmpPrefixo(url, "https://m.imdb.com/")) {
    p = strchr(url + 8, '/') + 1; imdbWeb = 1;
  } else if (cmpPrefixo(url, "https://imdb.com/")) { p = url + 17; imdbWeb = 1; }
  else return 0;

  fim = p + strcspn(p, "?#");
  query = *fim == '?' ? fim + 1 : "";
  pn = (size_t)(fim - p);
  n = segmentos(p, pn, seg, len, 6);
  if (n < 1) return 0;

  if (imdbWeb) {   // /title/tt0111161[/...]
    if (len[0] != 5 || strncmp(seg[0], "title", 5) || n < 2) return 0;
    if (len[1] < 4 || !idTitulo(seg[1], len[1], o) || !o->imdb[0]) goto lixo;
    o->tipo = DL_TITULO;
    return 1;
  }
  if (nativo && len[0] == 6 && !strncmp(seg[0], "search", 6)) {
    if (n != 1 || !consultaDe(query, o)) goto lixo;
    o->tipo = DL_BUSCA;
    return 1;
  }
  if (nativo ? (len[0] == 4 && !strncmp(seg[0], "meta", 4)) : (len[0] == 6 && !strncmp(seg[0], "detail", 6))) {
    // nuvio-legacy://meta/{type}/{id}; stremio:///detail/{type}/{id}[/{videoId}]
    if (n < 3 || n > (nativo ? 3 : 4)) goto lixo;
    o->tipo = DL_TITULO;
  } else if (nativo && len[0] == 4 && !strncmp(seg[0], "play", 4)) {
    // nuvio-legacy://play/{type}/{id}[/{season}/{episode}]
    if (n != 3 && n != 5) goto lixo;
    o->tipo = DL_TOCAR;
    if (n == 5) {
      if (!numero(seg[3], len[3], 9999, &t) || t < 1 || !numero(seg[4], len[4], 99999, &e) || e < 1) goto lixo;
      o->temporada = (int)t; o->episodio = (int)e;
    }
  } else goto lixo;
  if (!tipoMidia(seg[1], len[1], &o->serie) || !idTitulo(seg[2], len[2], o)) goto lixo;
  if (o->tipo == DL_TOCAR && o->temporada && !o->serie) goto lixo;   // filme nao tem episodio
  return 1;
lixo:
  memset(o, 0, sizeof *o);
  return 0;
}

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static DeepLink pendente;

void deeplink_postar(const char *url) {
  DeepLink d;
  if (!deeplink_parse(url, &d)) return;
  pthread_mutex_lock(&trava); pendente = d; pthread_mutex_unlock(&trava);
}

int deeplink_pegar(DeepLink *out) {
  int ok;
  pthread_mutex_lock(&trava);
  ok = pendente.tipo != DL_NENHUM;
  if (ok) { *out = pendente; memset(&pendente, 0, sizeof pendente); }
  pthread_mutex_unlock(&trava);
  return ok;
}

#ifdef NV_ANDROID
#include <jni.h>
JNIEXPORT void JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeDeepLink(JNIEnv *env, jobject act, jstring url) {
  const char *s;
  (void)act;
  if (!url) return;
  s = (*env)->GetStringUTFChars(env, url, NULL);
  if (!s) return;
  deeplink_postar(s);
  (*env)->ReleaseStringUTFChars(env, url, s);
}
#endif
