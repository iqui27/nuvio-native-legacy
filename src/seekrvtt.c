#include "seekrvtt.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEEKR_SPRITES "https://sprites.seekr.tv"

int seekr_ler_lookup(const char *json, char *vtt, size_t tam) {
  return seekr_ler_lookup_base(json, SEEKR_SPRITES, vtt, tam);
}

int seekr_ler_lookup_base(const char *json, const char *base, char *vtt, size_t tam) {
  char cru[2048];
  if (!json || !vtt || tam < 16 || !base) return 0;
  if (!js_texto_raiz(json, "vtt_url", cru, sizeof cru) || !cru[0]) return 0;
  // A pagina do servico avisa "vtt_url IS RELATIVE"; o SDK espera absoluta.
  // As duas formas passam.
  if (cru[0] == '/') {
    if ((size_t)snprintf(vtt, tam, "%s%s", base, cru) >= tam) return 0;
  } else if (!strncmp(cru, "https://", 8) || !strncmp(cru, "http://", 7)) {
    if ((size_t)snprintf(vtt, tam, "%s", cru) >= tam) return 0;
  } else return 0;
  return 1;
}

int seekr_url_lookup(char *dst, size_t tam, const char *imdb, int t, int e,
                     long durMs) {
  int n, nId;
  if (!dst || !imdb || strncmp(imdb, "tt", 2) || durMs <= 0) return 0;
  // O id do catalogo pode vir como "tt123:1:2"; a API quer so o imdb.
  nId = (int)strcspn(imdb, ":");
  if (t >= 0 && e > 0)
    n = snprintf(dst, tam,
                 "https://api.seekr.tv/sprites?duration_ms=%ld&show_imdb_id=%.*s&season=%d&episode=%d",
                 durMs, nId, imdb, t, e);
  else
    n = snprintf(dst, tam, "https://api.seekr.tv/sprites?duration_ms=%ld&imdb_id=%.*s",
                 durMs, nId, imdb);
  return n > 0 && (size_t)n < tam;
}

// "01:02:03.456" ou "02:03.456" em ms; -1 quando nao e um tempo.
static long tempoMs(const char *s, const char **fim) {
  long partes[3] = {0, 0, 0}; int n = 0; double seg = 0;
  const char *p = s;
  while (*p == ' ' || *p == '\t') p++;
  for (;;) {
    char *q; double v = strtod(p, &q);
    if (q == p) return -1;
    if (*q == ':' && n < 2) { partes[n++] = (long)v; p = q + 1; continue; }
    seg = v; p = q; break;
  }
  if (fim) *fim = p;
  if (n == 2) return partes[0] * 3600000L + partes[1] * 60000L + (long)(seg * 1000.0 + 0.5);
  if (n == 1) return partes[0] * 60000L + (long)(seg * 1000.0 + 0.5);
  return -1;
}

static int cmpCue(const void *a, const void *b) {
  long x = ((const SeekrCue *)a)->ini, y = ((const SeekrCue *)b)->ini;
  return x < y ? -1 : x > y;
}

// OpenSeek (CONTRACT-v1 "VTT semantics"): cada URL de quadro/folha leva o pts
// real como "&t=<ms>" antes do fragmento. A mesma folha aparece com t
// diferentes, entao a chave da folha e a URL SEM o t. Tira o parametro t de
// [url, url+n) para `dst` e devolve o valor (-1 sem t valido).
static long tirarT(const char *url, size_t n, char *dst, size_t tam) {
  const char *q = memchr(url, '?', n);
  long t = -1;
  size_t k = 0, i;
  if (tam < n + 1) return -2;
  if (!q) { memcpy(dst, url, n); dst[n] = 0; return -1; }
  k = (size_t)(q - url) + 1;
  memcpy(dst, url, k);
  for (i = k; i < n; ) {
    size_t f = i;
    while (f < n && url[f] != '&') f++;
    if (f - i > 2 && url[i] == 't' && url[i + 1] == '=') {
      long v = 0; size_t j = i + 2; int ok = 1;
      for (; j < f; j++) {
        if (url[j] < '0' || url[j] > '9' || v > 100000000L) { ok = 0; break; }
        v = v * 10 + (url[j] - '0');
      }
      if (ok) t = v;
    } else {
      if (dst[k - 1] != '?' && dst[k - 1] != '&') dst[k++] = '&';
      memcpy(dst + k, url + i, f - i); k += f - i;
    }
    i = f < n ? f + 1 : f;
  }
  while (k && (dst[k - 1] == '&' || dst[k - 1] == '?')) k--;
  dst[k] = 0;
  return t;
}

static int folhaIdx(SeekrVtt *v, const char *url, size_t n, int *cap, const char *base) {
  int i; char *c;
  size_t tam = n + 1 + strlen(base) + 1;
  // Folha relativa ganha o host das miniaturas, como o vtt_url. A comparacao
  // e feita JA na forma absoluta, senao a mesma folha relativa duplicaria.
  c = malloc(tam);
  if (!c) return -1;
  if (url[0] == '/') snprintf(c, tam, "%s%.*s", base, (int)n, url);
  else { memcpy(c, url, n); c[n] = 0; }
  // Normalmente a mesma folha serve dezenas de cues seguidas: olhar a ultima
  // primeiro torna a leitura linear.
  for (i = v->nFolhas - 1; i >= 0; i--)
    if (!strcmp(v->folhas[i], c)) { free(c); return i; }
  if (v->nFolhas >= *cap) {
    int nc = *cap ? *cap * 2 : 16;
    char **nf = realloc(v->folhas, (size_t)nc * sizeof *nf);
    if (!nf) { free(c); return -1; }
    v->folhas = nf; *cap = nc;
  }
  v->folhas[v->nFolhas] = c;
  return v->nFolhas++;
}

// A URL absoluta comeca na origem `o` (esquema://host[:porta]) seguida de "/".
static int naOrigem(const char *url, size_t n, const char *o) {
  size_t k = strlen(o);
  return k && n > k && !strncmp(url, o, k) && url[k] == '/';
}

// modo 0 = Seekr (o comportamento de sempre, com `base` para as relativas);
// modo 1 = OpenSeek: so URLs absolutas da origem `base`, t= vira rotulo.
static int lerVtt(const char *vtt, const char *base, int modo, SeekrVtt *out);

int seekr_vtt_ler(const char *vtt, SeekrVtt *out) {
  return lerVtt(vtt, SEEKR_SPRITES, 0, out);
}
int seekr_vtt_ler_base(const char *vtt, const char *base, SeekrVtt *out) {
  return lerVtt(vtt, base ? base : SEEKR_SPRITES, 0, out);
}
int seekr_vtt_ler_openseek(const char *vtt, const char *origem, SeekrVtt *out) {
  if (!origem || !*origem) { if (out) memset(out, 0, sizeof *out); return 0; }
  return lerVtt(vtt, origem, 1, out);
}

static int lerVtt(const char *vtt, const char *base, int modo, SeekrVtt *out) {
  const char *p;
  int capC = 0, capF = 0;
  if (!out) return 0;
  memset(out, 0, sizeof *out);
  if (!vtt) return 0;
  for (p = vtt; *p; ) {
    const char *eol = strchr(p, '\n'), *seta;
    const char *lfim = eol ? eol : p + strlen(p);
    seta = NULL;
    for (const char *q = p; q + 3 <= lfim; q++)
      if (q[0] == '-' && q[1] == '-' && q[2] == '>') { seta = q; break; }
    if (seta) {
      const char *r; long a = tempoMs(p, NULL), b = tempoMs(seta + 3, NULL);
      // A linha do conteudo: a primeira nao vazia depois do tempo.
      const char *c = eol ? eol + 1 : lfim, *cfim, *hash;
      while (*c == '\r' || *c == '\n' || *c == ' ' || *c == '\t') c++;
      cfim = strchr(c, '\n'); if (!cfim) cfim = c + strlen(c);
      while (cfim > c && (cfim[-1] == '\r' || cfim[-1] == ' ')) cfim--;
      hash = NULL;
      for (r = cfim; r > c; r--) if (r[-1] == '#') { hash = r - 1; break; }
      if (a >= 0 && b > a && hash && !strncmp(hash, "#xywh=", 6)) {
        int x, y, w, h;
        if (sscanf(hash + 6, "%d,%d,%d,%d", &x, &y, &w, &h) == 4 &&
            x >= 0 && y >= 0 && w > 0 && h > 0 && w <= 4096 && h <= 4096) {
          long rot = -1;
          int f;
          if (modo == 1) {
            // OpenSeek: so a origem pareada (o pino vale para ela) e so com t=.
            size_t nu = (size_t)(hash - c);
            char *sem = malloc(nu + 1);
            if (!sem) break;
            rot = naOrigem(c, nu, base) ? tirarT(c, nu, sem, nu + 1) : -1;
            f = rot >= 0 ? folhaIdx(out, sem, strlen(sem), &capF, base) : -2;
            free(sem);
          } else f = folhaIdx(out, c, (size_t)(hash - c), &capF, base);
          if (f == -1) break;
          if (f >= 0) {
            if (out->nCues >= capC) {
              int nc = capC ? capC * 2 : 256;
              SeekrCue *n = realloc(out->cues, (size_t)nc * sizeof *n);
              if (!n) break;
              out->cues = n; capC = nc;
            }
            out->cues[out->nCues++] = (SeekrCue){ a, b, f, x, y, w, h, rot };
          }
        }
      }
    }
    if (!eol) break;
    p = eol + 1;
  }
  if (out->nCues > 1) qsort(out->cues, (size_t)out->nCues, sizeof *out->cues, cmpCue);
  return out->nCues;
}

void seekr_vtt_liberar(SeekrVtt *v) {
  int i;
  if (!v) return;
  for (i = 0; i < v->nFolhas; i++) free(v->folhas[i]);
  free(v->folhas); free(v->cues);
  memset(v, 0, sizeof *v);
}

int seekr_vtt_cue(const SeekrVtt *v, long pos) {
  int lo, hi, i;
  if (!v || v->nCues < 1) return -1;
  if (pos <= v->cues[0].ini) return 0;
  // A ultima cue com inicio <= pos (busca binaria: um filme de 3 h tem ~1000).
  lo = 0; hi = v->nCues - 1;
  while (lo < hi) {
    int m = (lo + hi + 1) / 2;
    if (v->cues[m].ini <= pos) lo = m; else hi = m - 1;
  }
  i = lo;
  // Passada a metade, a seguinte esta mais perto. Comparacao sem dividir por
  // dois: as cues nao tem todas o mesmo tamanho.
  if (i + 1 < v->nCues && pos - v->cues[i].ini > v->cues[i].fim - pos) i++;
  return i;
}

int seekr_vtt_cue_estrita(const SeekrVtt *v, long pos) {
  int lo, hi;
  if (!v || v->nCues < 1 || pos < v->cues[0].ini) return -1;
  lo = 0; hi = v->nCues - 1;
  while (lo < hi) {
    int m = (lo + hi + 1) / 2;
    if (v->cues[m].ini <= pos) lo = m; else hi = m - 1;
  }
  // [ini, fim): fora de toda cue nao ha previa (SCRUBBING.md §3).
  return pos < v->cues[lo].fim ? lo : -1;
}
