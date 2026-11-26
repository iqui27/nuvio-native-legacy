// Ver cachemarca.h.
#include "cachemarca.h"
#include "dobra.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static unsigned long dec(const unsigned char *p, int *n) {
  unsigned long cp;
  int len, i;
  if (*p < 0x80) { *n = 1; return *p; }
  if (*p >= 0xC2 && *p <= 0xDF) { len = 2; cp = *p & 0x1F; }
  else if (*p >= 0xE0 && *p <= 0xEF) { len = 3; cp = *p & 0x0F; }
  else if (*p >= 0xF0 && *p <= 0xF4) { len = 4; cp = *p & 0x07; }
  else { *n = 1; return 0xFFFD; }
  for (i = 1; i < len; i++) {
    if ((p[i] & 0xC0) != 0x80) { *n = 1; return 0xFFFD; }
    cp = cp << 6 | (p[i] & 0x3F);
  }
  *n = len;
  return cp;
}

// Uma linha -> forma de comparar: letra estilizada vira ASCII (o "Iɴsᴛᴀɴᴛ" do
// Core Zenith vira "INsTANT"), e somem os invisiveis que o Tamtaro poe em volta
// do ⏳ (ZWJ, ZWSP, WJ) e os seletores de variacao (⚡️ = ⚡).
static void normalizar(const char *ini, size_t n, char *out, size_t tam) {
  const unsigned char *p = (const unsigned char *)ini, *fim = p + n;
  size_t k = 0;
  while (p < fim && *p && k + 5 < tam) {
    int m;
    unsigned long c = dec(p, &m);
    // 🅛 (U+1F15B) e a marca de biblioteca do Chad: nao vira "L".
    char a = c >= 0x80 && c != 0x1F15B ? nv_dobra_estilizada(c) : 0;
    if (c == 0xFE0F || c == 0xFE0E || (c >= 0x200B && c <= 0x200D) ||
        (c >= 0x2060 && c <= 0x2064) || c == 0xFEFF) { p += m; continue; }
    if (a) out[k++] = a;
    else if (c == '\t') out[k++] = ' ';
    else { memcpy(out + k, p, (size_t)m); k += (size_t)m; }
    p += m;
  }
  out[k] = 0;
}

static int letra(char c) { return isalpha((unsigned char)c); }
// Comeca com `w`, sem diferenca de caixa.
static int ci(const char *p, const char *w) {
  for (; *w; p++, w++) if (tolower((unsigned char)*p) != tolower((unsigned char)*w)) return 0;
  return 1;
}
static int com(const char *p, const char *u) { return !strncmp(p, u, strlen(u)); }
static const char *espacos(const char *p) { while (*p == ' ') p++; return p; }

// Codigo de servico de debrid, em maiusculas, sem letra colada depois.
static size_t codigo(const char *p) {
  static const char *const COD[] = {
    "PUTIO", "TORBOX", "PIO", "TRB", "PKP", "SDR", "RD", "AD", "PM", "TB", "DL", "ED",
    "OC", "PP", "HW", "DR", "EN", "ST", "TR" };
  for (unsigned i = 0; i < sizeof COD / sizeof *COD; i++) {
    size_t l = strlen(COD[i]);
    if (!strncmp(p, COD[i], l) && !letra(p[l])) return l;
  }
  return 0;
}

// NOME DE ARQUIVO NUNCA DIZ O CACHE ("Instant.Family.2018.mkv",
// "Show.download].mkv"). Cada palavra que parece arquivo — termina em extensao
// de video ou tem 2+ pontos entre letras/numeros — vira espaco antes da busca.
static int pareceArquivo(const char *w, size_t n) {
  static const char *const EXT[] = { ".mkv", ".mp4", ".avi", ".m2ts", ".ts", ".webm", ".mov", ".m4v", ".iso" };
  int pontos = 0;
  for (unsigned i = 0; i < sizeof EXT / sizeof *EXT; i++) {
    size_t l = strlen(EXT[i]);
    if (n > l && !strncasecmp(w + n - l, EXT[i], l)) return 1;
  }
  for (size_t i = 1; i + 1 < n; i++)
    if (w[i] == '.' && isalnum((unsigned char)w[i - 1]) && w[i + 1] && !isspace((unsigned char)w[i + 1])) pontos++;
  return pontos >= 2;
}
static void tirarArquivos(char *l) {
  for (char *p = l; *p; ) {
    char *f;
    while (*p == ' ') p++;
    f = p;
    while (*f && *f != ' ') f++;
    if (f > p && pareceArquivo(p, (size_t)(f - p))) memset(p, ' ', (size_t)(f - p));
    p = f;
  }
}

// Dentro de colchetes ("[🔄] Comet", "[RD🔄] Comet Sync", "[❌] Comet"): aviso
// do addon, nao estado de cache.
static int emColchete(const char *ini, const char *p) {
  for (const char *q = p; q > ini && p - q < 10; q--) {
    if (q[-1] == ']') return 0;
    if (q[-1] == '[') return 1;
  }
  return 0;
}

typedef struct { int pos, neg, lib, codigoSo; } Marcas;

static void palavras(const char *ini, const char *p, Marcas *m) {
  int borda = p == ini || !letra(p[-1]);
  if (!borda) return;
  if (ci(p, "uncached")) m->neg = 1;
  else if (ci(p, "not cached") || ci(p, "not ready") || ci(p, "may not be available")) m->neg = 1;
  else if (ci(p, "good choice")) m->pos = 1;
  else if (ci(p, "your media") && p > ini && p[-1] == '(') { m->pos = 1; m->lib = 1; }
  else if (ci(p, "library") && p - ini >= 4 && !strncmp(p - 4, "\xE2\x80\xA2 ", 4)) { m->pos = 1; m->lib = 1; }   // "• Library"
  else if (ci(p, "instant") && !letra(p[7]) && p[7] != '.') {
    // INSTANT do Core Nexus, "Iɴsᴛᴀɴᴛ" do Zenith, "(Instant RD)" do TorBox,
    // "⚡ Instant" do Comet. "Instant Family" num titulo nao: precisa estar em
    // maiusculas ou logo depois de ( ⚡ 🚀.
    int maius = 1;
    for (int i = 0; i < 7; i++) if (!isupper((unsigned char)p[i]) && p[i] != 's') maius = 0;
    const char *q = p;
    while (q > ini && q[-1] == ' ') q--;
    if (maius || (q > ini && q[-1] == '(') || (q - ini >= 3 && com(q - 3, "\xE2\x9A\xA1")) ||
        (q - ini >= 4 && com(q - 4, "\xF0\x9F\x9A\x80"))) m->pos = 1;
  } else if (ci(p, "cached") && !letra(p[6])) {
    // "Cached" sozinho (MediaFusion "🧲 Torrent • Cached"). "⛔ Cached" e o
    // fora de cache do c8RdRgRP; "Not Cached" ja caiu acima.
    const char *q = p;
    while (q > ini && q[-1] == ' ') q--;
    if (q - ini >= 3 && com(q - 3, "\xE2\x9B\x94")) m->neg = 1;
    else if (!(q - ini >= 3 && ci(q - 3, "not"))) m->pos = 1;
  }
}

static int depois(const char *p, const char *const *ws) {
  p = espacos(p);
  for (; *ws; ws++) if (ci(p, *ws)) return 1;
  return 0;
}

static void simbolos(const char *ini, const char *p, int n, unsigned long c, Marcas *m) {
  static const char *const FORCADA[] = { "forced", NULL };
  static const char *const FAIXA[] = { "hdr", "sdr", "dv", "hlg", "dolby", NULL };
  static const char *const RES[] = { "fhd", "qhd", "uhd", "1080", "1440", "2160", "2k", "4k", "hd", NULL };
  const char *q = p + n;
  switch (c) {
    case 0x26A1: if (!depois(q, FORCADA)) m->pos = 1; break;                       // ⚡
    case 0x1F680: if (!depois(q, RES)) m->pos = 1; break;                         // 🚀
    case 0x2726: if (!depois(q, FAIXA)) m->pos = 1; break;                        // ✦
    case 0x2727: if (!depois(q, FAIXA)) m->neg = 1; break;                        // ✧
    case 0x2611: case 0x2705: case 0x27A0: case 0x25BA: case 0x25BB: case 0x1F3AB:  // ☑ ✅ ➠ ► ▻ 🎫
      m->pos = 1; break;
    case 0x23F3: case 0x2612: case 0x231B: case 0x25BC: case 0x25BD: case 0x1F39F:  // ⏳ ☒ ⌛ ▼ ▽ 🎟
      m->neg = 1; break;
    case 0x1F504: case 0x274C:                                                     // 🔄 ❌
      if (!emColchete(ini, p)) m->neg = 1;
      break;
    default: break;
  }
}

// Uma linha ja normalizada. `nome` liga os simbolos sozinhos (regra 2).
static int linha(const char *l, int nome, Marcas *m) {
  memset(m, 0, sizeof *m);
  for (const char *p = l; *p; ) {
    int n;
    unsigned long c = dec((const unsigned char *)p, &n);
    if (*p == '[') {
      size_t k = codigo(p + 1);
      if (k) {
        const char *q = p + 1 + k;
        if (*q == '+') m->pos = 1;
        else if (com(espacos(q), "\xE2\x9A\xA1")) m->pos = 1;                       // [RD⚡ [RD ⚡
        else if (ci(q, " download")) m->neg = 1;
        else if (com(espacos(q), "\xE2\xAC\x87") || com(espacos(q), "\xE2\x8F\xB3")) m->neg = 1;  // [RD⬇ [RD⏳
        else if (ci(q, " c]")) m->pos = 1;                                         // Comet Kodi
        else if (ci(q, " u]")) m->neg = 1;
        else if (*q == ']') m->codigoSo = 1;
      }
    } else if (c < 0x80 && isupper((unsigned char)*p) && (p == l || !letra(p[-1]))) {
      size_t k = codigo(p);
      if (k) {
        const char *q = espacos(p + k);
        if (com(q, "\xE2\x9A\xA1")) m->pos = 1;                                    // RD ⚡ (MediaFusion)
        else if (com(q, "\xE2\x8F\xB3")) m->neg = 1;                               // RD ⏳
        else {                                                                     // 🧲 RD (MediaFusion compacto)
          const char *b = p;
          while (b > l && b[-1] == ' ') b--;
          if (b - l >= 4 && com(b - 4, "\xF0\x9F\xA7\xB2")) m->codigoSo = 1;
        }
      }
    }
    if (c == 0x26A1) {                                                             // ⚡ [RD] / ⚡Ready
      const char *q = espacos(p + n);
      if ((*q == '[' && codigo(q + 1)) || ci(q, "ready")) m->pos = 1;
    }
    if (c == 0x2601 || c == 0x1F4CC || c == 0x1F5C4 || c == 0x2733 || c == 0x1F15B) {  // ☁ 📌 🗄 ✳ 🅛
      m->pos = 1; m->lib = 1;
    }
    if (nome) simbolos(l, p, n, c, m);
    if (c < 0x80 && letra(*p)) palavras(l, p, m);
    p += n;
  }
  return m->neg ? -1 : m->pos ? 1 : 0;
}

// Todas as linhas de um bloco: as que decidem tem de concordar.
static int bloco(const char *t, int nome, int *lib, int *codigoSo) {
  int res = 0, conflito = 0, deLib = 0;
  if (!t) return 0;
  for (const char *p = t; *p; ) {
    const char *f = strchr(p, '\n');
    size_t n = f ? (size_t)(f - p) : strlen(p);
    char *norm = malloc(n * 2 + 8);
    if (norm) {
      Marcas m;
      int r;
      normalizar(p, n, norm, n * 2 + 8);
      if (!nome) tirarArquivos(norm);
      r = linha(norm, nome, &m);
      if (m.codigoSo && codigoSo) *codigoSo = 1;
      if (r && res && r != res) conflito = 1;
      if (r && !res) { res = r; deLib = m.lib && r > 0; }
      free(norm);
    }
    if (!f) break;
    p = f + 1;
  }
  if (conflito) return 0;
  if (lib) *lib = deLib;
  return res;
}

static int contemCi(const char *s, const char *w) {
  if (!s) return 0;
  for (; *s; s++) if (ci(s, w)) return 1;
  return 0;
}

int cachemarca_fonte(const char *nome, const char *descricao, const char *provedor,
                     int *biblioteca) {
  int lib = 0, codigoSo = 0, r;
  if (biblioteca) *biblioteca = 0;
  r = bloco(nome, 1, &lib, &codigoSo);
  if (!r) {
    // Addons que so marcam o que esta em cache: "[RD]" sem ⚡ nem + e fora.
    static const char *const SO_POSITIVA[] = { "stremthru", "jackettio", "debridio", "mediafusion", "peerflix" };
    if (codigoSo && provedor && !contemCi(provedor, "aiostreams"))
      for (unsigned i = 0; i < sizeof SO_POSITIVA / sizeof *SO_POSITIVA; i++)
        if (contemCi(provedor, SO_POSITIVA[i])) return -1;
    r = bloco(descricao, 0, &lib, NULL);
  }
  if (biblioteca) *biblioteca = r > 0 && lib;
  return r;
}

int cachemarca_fora(const char *texto) {
  if (!texto) return 0;
  for (const char *p = texto; *p; ) {
    const char *f = strchr(p, '\n');
    size_t n = f ? (size_t)(f - p) : strlen(p);
    char *norm = malloc(n * 2 + 8);
    if (norm) {
      Marcas m;
      normalizar(p, n, norm, n * 2 + 8);
      linha(norm, 1, &m);
      free(norm);
      if (m.neg) return 1;
    }
    if (!f) break;
    p = f + 1;
  }
  return 0;
}
