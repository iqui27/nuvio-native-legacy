#include "marcacena.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEG_MAX (99 * 3600)

int marcacena_valida(const MarcaCena *m) {
  size_t i, n;
  if (!m || m->seg < 0 || m->seg > SEG_MAX || m->durSeg < 0 || m->durSeg > SEG_MAX) return 0;
  if (m->tipo != 'm' && m->tipo != 's') return 0;
  if (m->temporada < 0 || m->episodio < 0 || m->temporada > 999 || m->episodio > 9999) return 0;
  n = strlen(m->id);
  if (n < 3 || n >= sizeof m->id) return 0;
  for (i = 0; i < n; i++) {
    char c = m->id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ':' || c == '_' || c == '-')) return 0;
  }
  return 1;
}

char *marcacena_tempo(int seg, char *dst, size_t tam) {
  if (seg < 0) seg = 0;
  if (seg >= 3600) snprintf(dst, tam, "%d:%02d:%02d", seg / 3600, seg / 60 % 60, seg % 60);
  else snprintf(dst, tam, "%d:%02d", seg / 60, seg % 60);
  return dst;
}

int marcacena_texto(const MarcaCena *m, char *dst, size_t tam) {
  char buf[96];
  int n;
  if (!marcacena_valida(m) || !dst || !tam) return 0;
  n = snprintf(buf, sizeof buf, "cena %dh%02dm%02ds", m->seg / 3600, m->seg / 60 % 60, m->seg % 60);
  if (m->episodio > 0)
    n += snprintf(buf + n, sizeof buf - (size_t)n, " t%d e%d", m->temporada, m->episodio);
  if (m->durSeg > 0) n += snprintf(buf + n, sizeof buf - (size_t)n, " d%d", m->durSeg);
  if (n > MARCACENA_TEXTO_MAX || (size_t)n >= tam) return 0;
  memcpy(dst, buf, (size_t)n + 1);
  return 1;
}

// Le um inteiro decimal curto; avanca *p. -1 se nao houver digito.
static long num(const char **p) {
  long v = 0; int d = 0;
  while (**p >= '0' && **p <= '9' && d < 7) { v = v * 10 + (**p - '0'); (*p)++; d++; }
  if (!d || (**p >= '0' && **p <= '9')) return -1;
  return v;
}

int marcacena_ler_texto(const char *texto, MarcaCena *m) {
  const char *p = texto;
  long h, mi, s, t = 0, e = 0, d = 0;
  if (!p || !m || strncmp(p, "cena ", 5)) return 0;
  p += 5;
  if ((h = num(&p)) < 0 || *p++ != 'h') return 0;
  if ((mi = num(&p)) < 0 || mi > 59 || *p++ != 'm') return 0;
  if ((s = num(&p)) < 0 || s > 59 || *p++ != 's') return 0;
  if (!strncmp(p, " t", 2)) {
    p += 2;
    if ((t = num(&p)) < 0 || strncmp(p, " e", 2)) return 0;
    p += 2;
    if ((e = num(&p)) < 1) return 0;
  }
  if (!strncmp(p, " d", 2)) { p += 2; if ((d = num(&p)) < 1) return 0; }
  if (*p) return 0;
  if (h * 3600 + mi * 60 + s > SEG_MAX || t > 999 || e > 9999 || d > SEG_MAX) return 0;
  m->seg = (int)(h * 3600 + mi * 60 + s);
  m->temporada = (short)t; m->episodio = (short)e; m->durSeg = (int)d;
  return 1;
}

int marcacena_link(const MarcaCena *m, char *dst, size_t tam) {
  int n;
  if (!marcacena_valida(m) || !dst || !tam) return 0;
  if (m->tipo == 's' && m->episodio > 0)
    n = snprintf(dst, tam, "nuvio-legacy://play/series/%s/%d/%d?t=%d", m->id, m->temporada, m->episodio, m->seg);
  else
    n = snprintf(dst, tam, "nuvio-legacy://play/%s/%s?t=%d", m->tipo == 's' ? "series" : "movie", m->id, m->seg);
  if (n < 0 || (size_t)n >= tam) { dst[0] = 0; return 0; }
  return 1;
}

int marcacena_ancorar(int seg, int durOrigem, int durLocal, int *incerto) {
  if (incerto) *incerto = 0;
  if (seg < 0) return -1;
  if (durLocal > 0 && seg >= durLocal) return -1;
  if (incerto && durOrigem > 0 && durLocal > 0) {
    long dif = (long)durOrigem - durLocal;
    if (dif < 0) dif = -dif;
    if (dif * 100 > (long)durOrigem * 3) *incerto = 1;
  }
  return seg;
}

static int mesmaCena(const MarcaCena *a, const MarcaCena *b) {
  int d = a->seg - b->seg;
  if (d < 0) d = -d;
  return d < MARCACENA_JUNTA_SEG && a->temporada == b->temporada &&
         a->episodio == b->episodio && !strcmp(a->id, b->id);
}

int marcacena_inserir(MarcaCena *v, int *n, int max, const MarcaCena *m) {
  int i;
  if (!v || !n || max < 1 || *n < 0 || *n > max || !marcacena_valida(m)) return -1;
  for (i = 0; i < *n; i++)
    if (mesmaCena(&v[i], m)) { memmove(&v[i], &v[i + 1], (size_t)(*n - i - 1) * sizeof *v); (*n)--; break; }
  if (*n == max) { memmove(&v[0], &v[1], (size_t)(max - 1) * sizeof *v); (*n)--; }
  v[*n] = *m;
  return (*n)++;
}

size_t marcacena_serializar(const MarcaCena *v, int n, char *dst, size_t tam) {
  size_t u = 0;
  int i;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  for (i = 0; i < n; i++) {
    int w;
    if (!marcacena_valida(&v[i])) continue;
    w = snprintf(dst + u, tam - u, "%s %c %d %d %d %d %lld\n", v[i].id, v[i].tipo,
                 v[i].temporada, v[i].episodio, v[i].seg, v[i].durSeg, v[i].quando);
    if (w < 0 || (size_t)w >= tam - u) { dst[u] = 0; break; }
    u += (size_t)w;
  }
  return u;
}

int marcacena_carregar(const char *txt, MarcaCena *v, int max) {
  int n = 0;
  while (txt && *txt && n < max) {
    MarcaCena m;
    int t, e;
    const char *fim = strchr(txt, '\n');
    memset(&m, 0, sizeof m);
    if (sscanf(txt, "%23s %c %d %d %d %d %lld", m.id, &m.tipo, &t, &e, &m.seg, &m.durSeg, &m.quando) == 7 &&
        t >= 0 && t <= 999 && e >= 0 && e <= 9999) {
      m.temporada = (short)t; m.episodio = (short)e;
      if (m.quando >= 0) marcacena_inserir(v, &n, max, &m);
    }
    if (!fim) break;
    txt = fim + 1;
  }
  return n;
}
