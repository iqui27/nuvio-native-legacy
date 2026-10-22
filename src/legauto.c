#include "legauto.h"
#include "legsync.h"
#include "linguas.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

#define LA_TOK 40   // o grupo vem no FIM do nome: cortar cedo o perde
#define LA_TOKLEN 16

// Palavras minusculas de 2+ letras/digitos ("1080p", "web", "dl", "yify").
// `s` ate o fim ou ate '?'; para o URL, so depois da ultima '/'.
static int palavras(const char *s, char t[LA_TOK][LA_TOKLEN], int ehUrl) {
  int n = 0, k = 0;
  const char *p;
  char cur[LA_TOKLEN];
  if (!s) return 0;
  if (ehUrl) {
    const char *q = strchr(s, '?'), *b = s, *x;
    size_t len = q ? (size_t)(q - s) : strlen(s);
    for (x = s; x < s + len; x++) if (*x == '/') b = x + 1;
    s = b;
    // o limite do '?' vale: copia ate ele
    for (p = s; p < s + len && *p; p++) {
      unsigned char c = (unsigned char)*p;
      if (isalnum(c)) { if (k < LA_TOKLEN - 1) cur[k++] = (char)tolower(c); }
      else { if (k >= 2 && n < LA_TOK) { cur[k] = 0; memcpy(t[n++], cur, (size_t)k + 1); } k = 0; }
    }
    if (k >= 2 && n < LA_TOK) { cur[k] = 0; memcpy(t[n++], cur, (size_t)k + 1); }
    return n;
  }
  for (p = s;; p++) {
    unsigned char c = (unsigned char)*p;
    if (c && isalnum(c)) { if (k < LA_TOKLEN - 1) cur[k++] = (char)tolower(c); }
    else {
      if (k >= 2 && n < LA_TOK) { cur[k] = 0; memcpy(t[n++], cur, (size_t)k + 1); }
      k = 0;
      if (!c) break;
    }
  }
  return n;
}

static int generica(const char *w) {
  static const char *const G[] = { "srt", "sub", "ass", "vtt", "mkv", "mp4", "avi", "www", "com", "en", "pt", "br", "por", "eng", "pob" };
  size_t i;
  for (i = 0; i < sizeof G / sizeof *G; i++) if (!strcmp(w, G[i])) return 1;
  return 0;
}

// Palavras da midia (sem as genericas) em que `arquivo` tambem aparece: um bit
// por palavra da midia. Mais de 64 nao cabem (LA_TOK ja corta antes).
static uint64_t casados(const char *arquivo, char m[LA_TOK][LA_TOKLEN], int nm) {
  char a[LA_TOK][LA_TOKLEN];
  int na = palavras(arquivo, a, 0), i, j;
  uint64_t r = 0;
  for (i = 0; i < na; i++) {
    if (generica(a[i])) continue;
    for (j = 0; j < nm; j++) if (!strcmp(a[i], m[j])) r |= 1ull << j;
  }
  return r;
}

static int popcnt(uint64_t x) { int n = 0; for (; x; x &= x - 1) n++; return n; }

// Marcas de release: o que separa um corte de outro (fonte, resolucao, codec).
static int marca(const char *w) {
  static const char *const M[] = { "web", "dl", "webdl", "webrip", "bluray", "brrip", "bdrip", "hdrip",
    "hdtv", "dvdrip", "remux", "x264", "x265", "h264", "h265", "hevc", "avc", "480p", "720p", "1080p",
    "2160p", "4k", "hdr", "amzn", "nf", "dsnp", "hmax", "atvp", "proper", "repack", "extended" };
  size_t i;
  for (i = 0; i < sizeof M / sizeof *M; i++) if (!strcmp(w, M[i])) return 1;
  return 0;
}

static int midia_palavras(const char *midia, char m[LA_TOK][LA_TOKLEN]) {
  return palavras(midia, m, strchr(midia ? midia : "", '/') != NULL);
}

int legauto_afinidade_release(const char *arquivo, const char *midia) {
  char m[LA_TOK][LA_TOKLEN];
  int nm = midia_palavras(midia, m);
  return popcnt(casados(arquivo, m, nm));
}

// O arquivo que toca: o nome declarado pela fonte (behaviorHints.filename), que
// o app.c informa; o URL de debrid/torrent quase nunca carrega o nome do release.
static char fonteNome[512];
void legauto_fonte(const char *arquivo) { snprintf(fonteNome, sizeof fonteNome, "%s", arquivo ? arquivo : ""); }
const char *legauto_midia(const char *url) { return fonteNome[0] ? fonteNome : url; }

int legauto_escolher(const Legenda *v, int n, const char *pref, const char *midia,
                     const uint64_t *excl, int nExcl, uint64_t lembrada) {
  char m[LA_TOK][LA_TOKLEN];
  int nm = midia_palavras(midia, m), df[LA_TOK] = {0}, elegiveis = 0, i, j, k, melhor = -1;
  long ms = -1;
  unsigned char ok[LEG_MAX];
  if (n > LEG_MAX) n = LEG_MAX;
  // 1. quem entra, e em quantas candidatas cada palavra da midia aparece.
  for (i = 0; i < n; i++) {
    uint64_t h, c;
    int af, jaFoi = 0;
    ok[i] = 0;
    if (!v[i].url[0]) continue;
    af = ling_afinidade(v[i].idioma, pref);
    if (af <= 0 || !v[i].idioma[0]) continue;
    h = legsync_hash_url(v[i].url);
    for (k = 0; k < nExcl; k++) if (excl[k] == h) jaFoi = 1;
    if (jaFoi) continue;
    ok[i] = 1; elegiveis++;
    c = casados(v[i].arquivo, m, nm);
    for (j = 0; j < nm; j++) if (c >> j & 1) df[j]++;
  }
  // 2. marca de release (WEB-DL, 1080p, x265...) pesa 4; palavra que so algumas
  // candidatas tem (o grupo) pesa 2; a que TODAS tem (o titulo do filme) nao
  // diferencia: 1.
  for (i = 0; i < n; i++) {
    long s;
    uint64_t c;
    int pesoRel = 0;
    if (!ok[i]) continue;
    c = casados(v[i].arquivo, m, nm);
    for (j = 0; j < nm; j++)
      if (c >> j & 1) pesoRel += marca(m[j]) ? 4 : df[j] < elegiveis ? 2 : 1;
    s = (long)ling_afinidade(v[i].idioma, pref) * 1000 + (long)pesoRel * 10 + (v[i].arquivo[0] ? 1 : 0);
    if (lembrada && legsync_hash_url(v[i].url) == lembrada) s += 100000;
    if (s > ms) { ms = s; melhor = i; }
  }
  return melhor;
}

static struct { char chave[96]; uint64_t hash; unsigned ordem; } mem[4];
static unsigned ordemMem;

static void chave(const char *t, const char *l, char *d, size_t n) {
  char f[8];
  ling_normalizar(l, f, sizeof f);
  // So a familia ("pob", "pt-BR" e "pt" sao a mesma chave): a pessoa pede "pt",
  // a legenda aplicada diz "pob".
  snprintf(d, n, "%s|%.2s", t ? t : "", ling_selo(f));
}

void legauto_lembrar(const char *titulo, const char *idioma, uint64_t hash) {
  char k[96];
  int i, v = 0;
  if (!titulo || !*titulo || !hash) return;
  chave(titulo, idioma, k, sizeof k);
  for (i = 0; i < 4; i++) if (!strcmp(mem[i].chave, k)) { v = i; goto grava; }
  for (i = 1; i < 4; i++) if (mem[i].ordem < mem[v].ordem) v = i;
grava:
  snprintf(mem[v].chave, sizeof mem[v].chave, "%s", k);
  mem[v].hash = hash; mem[v].ordem = ++ordemMem;
}

uint64_t legauto_lembrada(const char *titulo, const char *idioma) {
  char k[96];
  int i;
  if (!titulo || !*titulo) return 0;
  chave(titulo, idioma, k, sizeof k);
  for (i = 0; i < 4; i++) if (mem[i].chave[0] && !strcmp(mem[i].chave, k)) return mem[i].hash;
  return 0;
}
