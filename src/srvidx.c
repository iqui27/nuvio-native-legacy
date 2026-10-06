// Personal-server matching index. Contract and rules in srvidx.h.
#include "srvidx.h"
#include "dados.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

void srvidx_iniciar(SrvIdx *x) { memset(x, 0, sizeof *x); }

void srvidx_limpar(SrvIdx *x) {
  free(x->v);
  free(x->tab);
  memset(x, 0, sizeof *x);
}

static unsigned chaveHash(char tipo, const char *item) {
  unsigned h = 2166136261u ^ (unsigned char)tipo;
  h *= 16777619u;
  for (; *item; item++) { h ^= (unsigned char)*item; h *= 16777619u; }
  return h;
}

static int tabRefazer(SrvIdx *x, int ntab) {
  int *t = calloc((size_t)ntab, sizeof *t), i;
  if (!t) return 0;
  free(x->tab);
  x->tab = t;
  x->ntab = ntab;
  for (i = 0; i < x->n; i++) {
    unsigned k = chaveHash(x->v[i].tipo, x->v[i].item) & (unsigned)(ntab - 1);
    while (t[k]) k = (k + 1) & (unsigned)(ntab - 1);
    t[k] = i + 1;
  }
  return 1;
}

static int achaLinha(const SrvIdx *x, char tipo, const char *item) {
  unsigned k;
  if (!x->tab || !x->ntab) return -1;
  k = chaveHash(tipo, item) & (unsigned)(x->ntab - 1);
  while (x->tab[k]) {
    const SrvIdxItem *e = &x->v[x->tab[k] - 1];
    if (e->tipo == tipo && !strcmp(e->item, item)) return x->tab[k] - 1;
    k = (k + 1) & (unsigned)(x->ntab - 1);
  }
  return -1;
}

int srvidx_por(SrvIdx *x, const SrvIdxItem *e) {
  int i;
  if (!e || !e->item[0] || (e->tipo != 'm' && e->tipo != 's' && e->tipo != 'e')) return 0;
  i = achaLinha(x, e->tipo, e->item);
  if (i >= 0) {
    x->v[i] = *e;
  } else {
    if (x->n >= SRVIDX_MAX) return 0;
    if (x->n == x->cap) {
      int cap = x->cap ? x->cap * 2 : 256;
      SrvIdxItem *v;
      if (cap > SRVIDX_MAX) cap = SRVIDX_MAX;
      v = realloc(x->v, sizeof *v * (size_t)cap);
      if (!v) return 0;
      x->v = v;
      x->cap = cap;
    }
    // Load factor under 1/2: the table doubles before it gets crowded.
    if ((x->n + 1) * 2 > x->ntab) {
      int nt = x->ntab ? x->ntab * 2 : 512;
      while ((x->n + 1) * 2 > nt) nt *= 2;
      if (!tabRefazer(x, nt)) return 0;
    }
    x->v[x->n] = *e;
    { unsigned k = chaveHash(e->tipo, e->item) & (unsigned)(x->ntab - 1);
      while (x->tab[k]) k = (k + 1) & (unsigned)(x->ntab - 1);
      x->tab[k] = x->n + 1; }
    x->n++;
  }
  if (e->atualizado > x->marca) x->marca = e->atualizado;
  return 1;
}

void srvidx_juntar(SrvIdx *x, const SrvIdx *de) {
  int i;
  for (i = 0; i < de->n; i++) srvidx_por(x, &de->v[i]);
  if (de->marca > x->marca) x->marca = de->marca;
}

int srvidx_n_tipo(const SrvIdx *x, char tipo) {
  int i, k = 0;
  for (i = 0; i < x->n; i++) if (x->v[i].tipo == tipo) k++;
  return k;
}

int srvidx_achar(const SrvIdx *x, const char *imdb, long tmdb, long tvdb, char tipo) {
  int i;
  if (imdb && imdb[0])
    for (i = 0; i < x->n; i++) if (x->v[i].tipo == tipo && x->v[i].imdb[0] && !strcmp(x->v[i].imdb, imdb)) return i;
  if (tmdb > 0)
    for (i = 0; i < x->n; i++) if (x->v[i].tipo == tipo && x->v[i].tmdb == tmdb) return i;
  if (tvdb > 0 && tipo == 's')
    for (i = 0; i < x->n; i++) if (x->v[i].tipo == tipo && x->v[i].tvdb == tvdb) return i;
  return -1;
}

int srvidx_achar_titulo(const SrvIdx *x, unsigned hash, int ano, char tipo) {
  int i, achado = -1;
  if (!hash || ano <= 0) return -1;
  for (i = 0; i < x->n; i++) {
    const SrvIdxItem *e = &x->v[i];
    if (e->tipo != tipo || e->tituloHash != hash || e->ano <= 0) continue;
    if (e->ano < ano - 1 || e->ano > ano + 1) continue;
    if (achado >= 0) return -2;
    achado = i;
  }
  return achado;
}

int srvidx_achar_ep(const SrvIdx *x, const char *serie, int temporada, int episodio) {
  int i;
  if (!serie || !serie[0]) return -1;
  for (i = 0; i < x->n; i++) {
    const SrvIdxItem *e = &x->v[i];
    if (e->tipo == 'e' && e->temporada == temporada && e->episodio == episodio && !strcmp(e->serie, serie))
      return i;
  }
  return -1;
}

int srvidx_n_eps(const SrvIdx *x, const char *serie) {
  int i, k = 0;
  for (i = 0; i < x->n; i++) if (x->v[i].tipo == 'e' && !strcmp(x->v[i].serie, serie)) k++;
  return k;
}

// ------------------------------------------------------------------ titles
// Latin-1 Supplement (U+00C0..U+00FF, UTF-8 C3 80..C3 BF) folded to ASCII.
static char dobrar(unsigned char b) {
  static const char tab[65] = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  return tab[b - 0x80];
}

unsigned srvidx_titulo_hash(const char *titulo) {
  static const char *const artigos[] = { "the", "a", "an", "o", "os", "as", "el", "la", "los", "las",
                                         "le", "les", "il", "lo", "der", "die", "das", NULL };
  char norm[256], palavra[16];
  size_t k = 0, i, w;
  unsigned h = 2166136261u;
  const unsigned char *p = (const unsigned char *)(titulo ? titulo : "");
  // 1. ASCII lowercase, accents folded, every run of non-alnum = one space.
  while (*p && k + 2 < sizeof norm) {
    char c;
    if (*p == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) { c = dobrar(p[1]); p += 2; }
    else if (*p >= 0x80) { p++; continue; }       // other scripts: dropped, not guessed
    else c = (char)*p++;
    c = (char)tolower((unsigned char)c);
    if (isalnum((unsigned char)c)) norm[k++] = c;
    else if (k && norm[k - 1] != ' ') norm[k++] = ' ';
  }
  while (k && norm[k - 1] == ' ') k--;
  norm[k] = 0;
  // 2. One leading article, when something follows it.
  for (w = 0; norm[w] && norm[w] != ' ' && w + 1 < sizeof palavra; w++) palavra[w] = norm[w];
  palavra[w] = 0;
  i = 0;
  if (norm[w] == ' ')
    for (size_t a = 0; artigos[a]; a++) if (!strcmp(palavra, artigos[a])) { i = w + 1; break; }
  // 3. FNV-1a over the letters and digits only.
  for (k = 0; norm[i]; i++) {
    if (norm[i] == ' ') continue;
    h ^= (unsigned char)norm[i];
    h *= 16777619u;
    k++;
  }
  return k ? (h ? h : 1u) : 0u;
}

int srvidx_id_titulo(const char *id, char imdb[16], long *tmdb, int *temp, int *ep) {
  const char *p;
  int a = 0, b = 0;
  imdb[0] = 0;
  *tmdb = 0;
  if (temp) *temp = 0;
  if (ep) *ep = 0;
  if (!id) return 0;
  if (id[0] == 't' && id[1] == 't' && isdigit((unsigned char)id[2])) {
    size_t n = strspn(id + 2, "0123456789");
    if (n == 0 || n > 10) return 0;
    snprintf(imdb, 16, "%.*s", (int)(n + 2), id);
    p = id + 2 + n;
  } else if (!strncmp(id, "tmdb:", 5) && isdigit((unsigned char)id[5])) {
    char *e;
    *tmdb = strtol(id + 5, &e, 10);
    if (*tmdb <= 0) return 0;
    p = e;
  } else return 0;
  if (*p == ':' && sscanf(p, ":%d:%d", &a, &b) == 2) { if (temp) *temp = a; if (ep) *ep = b; }
  else if (*p) return 0;
  return 1;
}

static long long diasDesde1970(int a, int m, int d) {
  // Days from civil (Howard Hinnant), proleptic Gregorian.
  long long y = a - (m <= 2), era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long)doe - 719468;
}

long long srvidx_iso(const char *iso) {
  int a, m, d, H = 0, M = 0, S = 0;
  if (!iso || sscanf(iso, "%4d-%2d-%2d", &a, &m, &d) != 3) return 0;
  if (a < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  if (iso[10] == 'T' || iso[10] == ' ') sscanf(iso + 11, "%2d:%2d:%2d", &H, &M, &S);
  return diasDesde1970(a, m, d) * 86400LL + H * 3600 + M * 60 + S;
}

void srvidx_iso_de(long long t, char *dst, size_t tam) {
  long long z = t / 86400 + 719468, era, s = t % 86400;
  unsigned doe, yoe, doy, mp, d, m;
  long long y;
  if (t < 0) { if (tam) dst[0] = 0; return; }
  era = z / 146097;
  doe = (unsigned)(z - era * 146097);
  yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  y = (long long)yoe + era * 400;
  doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  snprintf(dst, tam, "%04lld-%02u-%02uT%02lld:%02lld:%02lldZ", y + (m <= 2), m, d, s / 3600, s / 60 % 60, s % 60);
}

int srvidx_ano_de(const char *meta) {
  int i;
  if (!meta) return 0;
  for (i = 0; meta[i]; i++)
    if (isdigit((unsigned char)meta[i]) && isdigit((unsigned char)meta[i + 1]) &&
        isdigit((unsigned char)meta[i + 2]) && isdigit((unsigned char)meta[i + 3]) &&
        !isdigit((unsigned char)meta[i + 4]) && (i == 0 || !isdigit((unsigned char)meta[i - 1]))) {
      int a = atoi(meta + i);
      if (a >= 1870 && a <= 2200) return a;
    }
  return 0;
}

static int ipv4Privado(const char *h, size_t n) {
  unsigned a, b, c, d;
  char t[40];
  if (n >= sizeof t) return 0;
  memcpy(t, h, n); t[n] = 0;
  if (sscanf(t, "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return 0;
  return a == 10 || a == 127 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31) ||
         (a == 169 && b == 254) || (a == 100 && b >= 64 && b <= 127);   // CGNAT: Tailscale & co.
}

int srvidx_host_local(const char *url) {
  const char *h, *f;
  size_t n;
  if (!url) return 0;
  h = strstr(url, "://");
  h = h ? h + 3 : url;
  f = h + strcspn(h, ":/?#");
  n = (size_t)(f - h);
  if (!n) return 0;
  if (*h == '[') return !strncasecmp(h, "[::1]", 5) || !strncasecmp(h, "[fe80", 5) ||
                        !strncasecmp(h, "[fd", 3) || !strncasecmp(h, "[fc", 3);
  if (ipv4Privado(h, n)) return 1;
  if (n == 9 && !strncasecmp(h, "localhost", 9)) return 1;
  if (n > 6 && !strncasecmp(f - 6, ".local", 6)) return 1;
  if (!memchr(h, '.', n)) return 1;                       // bare LAN name ("nas")
  // "10-0-0-5.<hash>.plex.direct": the first label is the dashed address.
  if (n > 12 && !strncasecmp(f - 12, ".plex.direct", 12)) {
    char ip[40];
    size_t k = strcspn(h, "."), i;
    if (k < sizeof ip) {
      for (i = 0; i < k; i++) ip[i] = h[i] == '-' ? '.' : h[i];
      ip[k] = 0;
      return ipv4Privado(ip, k);
    }
  }
  return 0;
}

// -------------------------------------------------------------------- text
// Header: "srvidx 1 <tag> <marca> <total>\n"; then one row per line,
// tab-separated: kind, item, series, imdb, tmdb, tvdb, year, season, episode,
// height, hdr, watched, title hash, save time.
char *srvidx_serializar(const SrvIdx *x, const char *tag) {
  size_t cap = 96 + (size_t)x->n * 200, k;
  char *t = malloc(cap);
  int i;
  if (!t) return NULL;
  k = (size_t)snprintf(t, cap, "srvidx 1 %s %lld %d\n", tag && *tag ? tag : "-", x->marca, x->total);
  for (i = 0; i < x->n && k < cap; i++) {
    const SrvIdxItem *e = &x->v[i];
    k += (size_t)snprintf(t + k, cap - k, "%c\t%s\t%s\t%s\t%ld\t%ld\t%d\t%d\t%d\t%u\t%u\t%u\t%u\t%lld\n",
                          e->tipo, e->item, e->serie[0] ? e->serie : "-", e->imdb[0] ? e->imdb : "-",
                          e->tmdb, e->tvdb, e->ano, e->temporada, e->episodio, e->altura, e->hdr,
                          e->visto, e->tituloHash, e->atualizado);
  }
  if (k >= cap) { free(t); return NULL; }
  return t;
}

static int campoId(const char *s, size_t n, char *dst, size_t tam) {
  size_t i;
  if (n == 1 && s[0] == '-') { dst[0] = 0; return 1; }
  if (!n || n >= tam) return 0;
  for (i = 0; i < n; i++) if (!isalnum((unsigned char)s[i]) && s[i] != '_' && s[i] != '-') return 0;
  memcpy(dst, s, n); dst[n] = 0;
  return 1;
}

int srvidx_desserializar(SrvIdx *x, const char *txt, const char *tag) {
  char t[64];
  long long marca;
  int total, lidas = 0;
  const char *p;
  if (!txt || sscanf(txt, "srvidx 1 %63s %lld %d", t, &marca, &total) != 3) return 0;
  if (tag && *tag && strcmp(t, tag)) return 0;          // another server's index
  p = strchr(txt, '\n');
  while (p && *++p) {
    const char *f[14], *q = p, *fim = strchr(p, '\n');
    size_t nf[14];
    int k = 0;
    SrvIdxItem e;
    if (!fim) fim = p + strlen(p);
    while (k < 14 && q <= fim) {
      const char *tb = memchr(q, '\t', (size_t)(fim - q));
      f[k] = q;
      nf[k] = (size_t)((tb ? tb : fim) - q);
      k++;
      if (!tb) break;
      q = tb + 1;
    }
    p = *fim ? fim : NULL;
    if (k != 14 || nf[0] != 1) continue;
    memset(&e, 0, sizeof e);
    e.tipo = f[0][0];
    if (!campoId(f[1], nf[1], e.item, sizeof e.item) || !e.item[0] ||
        !campoId(f[2], nf[2], e.serie, sizeof e.serie) || !campoId(f[3], nf[3], e.imdb, sizeof e.imdb))
      continue;
    if (e.imdb[0] && (e.imdb[0] != 't' || e.imdb[1] != 't')) continue;
    e.tmdb = strtol(f[4], NULL, 10);
    e.tvdb = strtol(f[5], NULL, 10);
    e.ano = (short)atoi(f[6]);
    e.temporada = (short)atoi(f[7]);
    e.episodio = (short)atoi(f[8]);
    e.altura = (unsigned short)atoi(f[9]);
    e.hdr = (unsigned char)atoi(f[10]);
    e.visto = (unsigned char)(atoi(f[11]) != 0);
    e.tituloHash = (unsigned)strtoul(f[12], NULL, 10);
    e.atualizado = strtoll(f[13], NULL, 10);
    if (srvidx_por(x, &e)) lidas++;
  }
  x->marca = marca > x->marca ? marca : x->marca;
  x->total = total;
  return lidas;
}

static void nomeArquivo(char *dst, size_t tam, int perfil, const char *tipoSrv) {
  snprintf(dst, tam, "srvidx-p%d-%s.txt", perfil, tipoSrv);
}

int srvidx_gravar(const SrvIdx *x, int perfil, const char *tipoSrv, const char *tag) {
  char nome[64], *t = srvidx_serializar(x, tag);
  int ok;
  if (!t) return 0;
  nomeArquivo(nome, sizeof nome, perfil, tipoSrv);
  ok = dados_gravar_leve(nome, t);
  free(t);
  return ok;
}

int srvidx_gravar_texto(int perfil, const char *tipoSrv, const char *txt) {
  char nome[64];
  if (!txt) return 0;
  nomeArquivo(nome, sizeof nome, perfil, tipoSrv);
  return dados_gravar_leve(nome, txt);
}

int srvidx_ler(SrvIdx *x, int perfil, const char *tipoSrv, const char *tag) {
  char nome[64], *t;
  int n;
  nomeArquivo(nome, sizeof nome, perfil, tipoSrv);
  t = dados_ler(nome);
  if (!t) return 0;
  n = srvidx_desserializar(x, t, tag);
  free(t);
  return n;
}

void srvidx_apagar(int perfil, const char *tipoSrv) {
  char nome[64];
  nomeArquivo(nome, sizeof nome, perfil, tipoSrv);
  dados_apagar(nome);
}
