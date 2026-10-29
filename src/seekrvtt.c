#include "seekrvtt.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEEKR_SPRITES "https://sprites.seekr.tv"

int seekr_ler_lookup(const char *json, char *vtt, size_t tam) {
  char cru[2048];
  if (!json || !vtt || tam < 16) return 0;
  if (!js_texto_raiz(json, "vtt_url", cru, sizeof cru) || !cru[0]) return 0;
  // A pagina do servico avisa "vtt_url IS RELATIVE"; o SDK espera absoluta.
  // As duas formas passam.
  if (cru[0] == '/') {
    if ((size_t)snprintf(vtt, tam, "%s%s", SEEKR_SPRITES, cru) >= tam) return 0;
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

static int folhaIdx(SeekrVtt *v, const char *url, size_t n, int *cap) {
  int i; char *c;
  size_t tam = n + 1 + sizeof SEEKR_SPRITES;
  // Folha relativa ganha o host das miniaturas, como o vtt_url. A comparacao
  // e feita JA na forma absoluta, senao a mesma folha relativa duplicaria.
  c = malloc(tam);
  if (!c) return -1;
  if (url[0] == '/') snprintf(c, tam, "%s%.*s", SEEKR_SPRITES, (int)n, url);
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

int seekr_vtt_ler(const char *vtt, SeekrVtt *out) {
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
          int f = folhaIdx(out, c, (size_t)(hash - c), &capF);
          if (f < 0) break;
          if (out->nCues >= capC) {
            int nc = capC ? capC * 2 : 256;
            SeekrCue *n = realloc(out->cues, (size_t)nc * sizeof *n);
            if (!n) break;
            out->cues = n; capC = nc;
          }
          out->cues[out->nCues++] = (SeekrCue){ a, b, f, x, y, w, h };
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
