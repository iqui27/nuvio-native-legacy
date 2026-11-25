// Ver formatador.h.
#include "formatador.h"
#include "limpa.h"
#include <stdio.h>
#include <string.h>

typedef struct { const char *utf8, *padrao, *minimalista; int cor; } Icone;
static const Icone TAB[] = {
#include "formatador_icones.inc"
};
#define N_TAB ((int)(sizeof TAB / sizeof *TAB))

// Idioma -> bandeira, da lista de idiomas do AIOStreams (formatador_idiomas.inc).
typedef struct { const char *chave, *cc; } Idioma;
static const Idioma IDIOMAS[] = {
#include "formatador_idiomas.inc"
};

// Icones depois dos quais o texto e uma lista de idiomas (legenda ou audio).
static int iconeDeIdioma(int idx) {
  static const char *const IC[] = {
    "\xF0\x9F\x92\xAC", "\xF0\x9F\x93\x9D", "\xF0\x9F\x94\xA4", "\xF0\x9F\xA6\xBB",  // 💬 📝 🔤 🦻
    "\xF0\x9F\x97\xA3", "\xF0\x9F\x8C\x90", "\xF0\x9F\x8C\x8E", "\xF0\x9F\x8C\x8D",  // 🗣 🌐 🌎 🌍
    "\xF0\x9F\x8E\x99", "\xF0\x9F\x91\x82", "\xE2\x9A\x90", "\xE2\x9A\x91",              // 🎙 👂 ⚐ ⚑
    "\xE2\x98\xB0" };                                                                        // ☰
  if (idx < 0 || idx >= N_TAB) return 0;
  for (unsigned i = 0; i < sizeof IC / sizeof *IC; i++) if (!strcmp(TAB[idx].utf8, IC[i])) return 1;
  return 0;
}

int formatador_icones_n(void) { return N_TAB; }

// Mesmo decodificador estrito de limpa.c: sequencia invalida = U+FFFD, 1 byte.
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
  if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) ||
      (cp >= 0xD800 && cp <= 0xDFFF)) { *n = 1; return 0xFFFD; }
  *n = len;
  return cp;
}
static int ehVS(unsigned long c) { return c == 0xFE0F || c == 0xFE0E; }
static int ehTom(unsigned long c) { return c >= 0x1F3FB && c <= 0x1F3FF; }

// Depois do icone: seletor de variacao, tom de pele e o resto de uma sequencia
// ZWJ desconhecida (ZWJ + pictograma) vao junto. Sem isso "🕵️‍♀️" deixava um
// "♀" solto, que nv_limpar_texto transforma em " · ".
static const unsigned char *cauda(const unsigned char *q) {
  for (;;) {
    int n, n2;
    unsigned long c;
    if (!*q) return q;
    c = dec(q, &n);
    if (ehVS(c) || ehTom(c)) { q += n; continue; }
    if (c == 0x200D && q[n]) {
      unsigned long c2 = dec(q + n, &n2);
      if (c2 >= 0x2000 && c2 != 0xFFFD) { q += n + n2; continue; }
    }
    return q;
  }
}

int formatador_icone_em(const char *p, size_t *len) {
  const unsigned char *s = (const unsigned char *)p;
  if (!s || *s < 0xC2) return -1;
  for (int k = 0; k < N_TAB; k++) {
    const unsigned char *e = (const unsigned char *)TAB[k].utf8, *q = s;
    int ok = 1;
    if (*e != *s) continue;
    while (*e) {
      int ne, nq = 1;
      unsigned long ce = dec(e, &ne), cq = 0;
      for (;;) {
        if (!*q) { ok = 0; break; }
        cq = dec(q, &nq);
        // O texto pode trazer VS16/VS15 e tom de pele onde a tabela nao tem.
        if (ehVS(cq) || (ehTom(cq) && !ehTom(ce))) { q += nq; continue; }
        break;
      }
      if (!ok || cq != ce) { ok = 0; break; }
      e += ne; q += nq;
    }
    if (!ok) continue;
    q = cauda(q);
    if (len) *len = (size_t)(q - s);
    return k;
  }
  return -1;
}

const char *formatador_icone_nome(int idx, int minimalista, int *cor) {
  if (idx < 0 || idx >= N_TAB) { if (cor) *cor = 0; return NULL; }
  if (cor) *cor = minimalista ? 0 : TAB[idx].cor;
  return minimalista ? TAB[idx].minimalista : TAB[idx].padrao;
}
const char *formatador_icone_utf8(int idx) {
  return idx >= 0 && idx < N_TAB ? TAB[idx].utf8 : "";
}

// Bandeira no comeco de `s`: par de indicadores regionais ("GB") ou 🏴 com as
// etiquetas de Inglaterra, Escocia e Gales ("GB-ENG"). 0 = nao e bandeira.
static size_t bandeiraEm(const unsigned char *s, char cc[8]) {
  int n1, n2;
  unsigned long a = dec(s, &n1), b;
  if (a >= 0x1F1E6 && a <= 0x1F1FF && s[n1]) {
    b = dec(s + n1, &n2);
    if (b >= 0x1F1E6 && b <= 0x1F1FF) {
      cc[0] = (char)('A' + (a - 0x1F1E6)); cc[1] = (char)('A' + (b - 0x1F1E6)); cc[2] = 0;
      return (size_t)(n1 + n2);
    }
    return 0;
  }
  if (a == 0x1F3F4) {
    static const struct { const char *tag, *cc; } SUB[] = {
      { "gbeng", "GB-ENG" }, { "gbsct", "GB-SCT" }, { "gbwls", "GB-WLS" } };
    char tag[8];
    int t = 0;
    const unsigned char *q = s + n1;
    for (;;) {
      int n;
      unsigned long c;
      if (!*q) return 0;
      c = dec(q, &n);
      if (c == 0xE007F) { q += n; break; }
      if (c < 0xE0020 || c > 0xE007E || t >= 7) return 0;
      tag[t++] = (char)(c - 0xE0000);
      q += n;
    }
    tag[t] = 0;
    for (unsigned i = 0; i < sizeof SUB / sizeof *SUB; i++)
      if (!strcmp(tag, SUB[i].tag)) { strcpy(cc, SUB[i].cc); return (size_t)(q - s); }
  }
  return 0;
}

static int poe(char *out, size_t cap, size_t *k, const char *rec, size_t n) {
  if (*k + n + 1 > cap) return 0;
  memcpy(out + *k, rec, n);
  *k += n;
  out[*k] = 0;
  return 1;
}
static int poeTexto(char *out, size_t cap, size_t *k, char tipo, const char *t, size_t l) {
  if (*k + l + 3 > cap) return 0;
  out[(*k)++] = tipo;
  memcpy(out + *k, t, l);
  *k += l;
  out[(*k)++] = 0;
  out[*k] = 0;
  return 1;
}

// UM ITEM DE IDIOMA -> pais da bandeira, ou NULL. Codigo (EN, PT-BR, JPN,
// ES-419) so em MAIUSCULAS, como os formatadores escrevem: "it", "no" e "is"
// minusculos sao palavras. Nome (English, Portuguese (Brazil), Latino) sem
// diferenca de caixa.
static const char *idiomaPais(const char *w, size_t n) {
  char k[48];
  int maius = 1, letras = 0;
  size_t i;
  if (!n || n >= sizeof k) return NULL;
  // Bytes fora do ASCII passam como vieram: nomes nativos ("العربية",
  // "Español", "日本語") estao na tabela com so o ASCII dobrado (ver
  // tools/idiomas-bandeira.py).
  for (i = 0; i < n; i++) {
    unsigned char c = (unsigned char)w[i];
    if (c >= 0x80) { k[i] = (char)c; letras++; continue; }
    if (c >= 'a' && c <= 'z') { maius = 0; letras++; }
    else if (c >= 'A' && c <= 'Z') letras++;
    k[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
  }
  k[n] = 0;
  if (!letras) return NULL;
  {
    size_t base = strcspn(k, "-_");
    if (base <= 3 && !maius) return NULL;   // "en", "it": palavra, nao codigo
  }
  for (i = 0; i < n; i++) if (k[i] == '_') k[i] = '-';
  {
    int lo = 0, hi = (int)(sizeof IDIOMAS / sizeof *IDIOMAS) - 1;
    while (lo <= hi) {
      int m = (lo + hi) / 2, c = strcmp(k, IDIOMAS[m].chave);
      if (!c) return IDIOMAS[m].cc;
      if (c < 0) hi = m - 1; else lo = m + 1;
    }
  }
  return NULL;
}

// Separador de itens numa lista de idiomas.
static size_t sepIdioma(const char *p) {
  if (*p == ',' || *p == '|' || *p == '/' || *p == ';' || *p == '+' || *p == '&') return 1;
  if (!strncmp(p, "\xC2\xB7", 2)) return 2;          // ·
  if (!strncmp(p, "\xE2\x80\xA2", 3)) return 3;      // •
  return 0;
}

// Escrita da direita para a esquerda (hebraico, arabe, persa, urdu: U+0590-07FF
// e as formas de apresentacao U+FB1D-FEFF) em `s`.
static int temRtl(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 0xD6 && c <= 0xDF) return 1;
    if (c == 0xEF && i + 1 < n && (unsigned char)s[i + 1] >= 0xAC && (unsigned char)s[i + 1] <= 0xBB) return 1;
  }
  return 0;
}

// Texto limpo depois de um icone de legenda/audio: os idiomas viram bandeira no
// Padrao. Escreve "S" + pecas do Padrao + "M" + o texto como veio + "E"; sem
// nenhum idioma reconhecido, nada (devolve 0) e o chamador poe o texto comum.
//
// SEPARADOR AO LADO DE TEXTO RTL vira peca propria: cada peca e desenhada (e
// passa pelo bidi) sozinha, e "العربية |" numa peca so sai "| العربية" — a
// barra pula para o lado errado. No LTR ele continua colado ao texto, como antes.
static int idiomas(const char *limpo, char *out, size_t cap, size_t *k,
                   int (*existe)(const char *cc)) {
  size_t k0 = *k, l = strlen(limpo);
  const char *p = limpo;
  int bandeiras = 0;
  char pend[2048];
  size_t np = 0;
  const char *sepP = NULL;   // separador RTL ainda nao escrito (ver acima)
  size_t sepN = 0;
#define SEP_SOLTA() do { if (sepN) { if (!poeTexto(out, cap, k, 'T', sepP, sepN)) goto cheio; sepN = 0; } } while (0)
  if (!poe(out, cap, k, "S", 1)) goto cheio;
  while (*p) {
    size_t s, n;
    const char *f = p, *cc;
    while (*p == ' ') p++;
    f = p;
    while (*f && !sepIdioma(f)) f++;
    n = (size_t)(f - p);
    while (n && p[n - 1] == ' ') n--;
    // O item inteiro ("Portuguese (Brazil)", "PT-BR"), ou palavra a palavra
    // ("EN PT JA"); o que nao e idioma fica texto ("Commentary", "Multi").
    cc = n ? idiomaPais(p, n) : NULL;
    if (cc && (!existe || existe(cc))) {
      if (np) { if (!poeTexto(out, cap, k, 'T', pend, np)) goto cheio; np = 0; }
      SEP_SOLTA();
      if (!poeTexto(out, cap, k, 'B', cc, strlen(cc))) goto cheio;
      bandeiras++;
    } else {
      const char *w = p, *fim = p + n;
      while (w < fim) {
        const char *e = w;
        while (e < fim && *e != ' ') e++;
        cc = idiomaPais(w, (size_t)(e - w));
        if (cc && (!existe || existe(cc))) {
          if (np) { if (!poeTexto(out, cap, k, 'T', pend, np)) goto cheio; np = 0; }
          SEP_SOLTA();
          if (!poeTexto(out, cap, k, 'B', cc, strlen(cc))) goto cheio;
          bandeiras++;
        } else if (e > w && np + (size_t)(e - w) + 2 < sizeof pend) {
          SEP_SOLTA();
          if (np) pend[np++] = ' ';
          memcpy(pend + np, w, (size_t)(e - w)); np += (size_t)(e - w);
        }
        w = e;
        while (w < fim && *w == ' ') w++;
      }
    }
    p = f;
    if ((s = sepIdioma(p))) {
      // separador entre dois textos fica; entre bandeiras some
      if (np && temRtl(pend, np)) {
        if (!poeTexto(out, cap, k, 'T', pend, np)) goto cheio;
        np = 0; sepP = p; sepN = s;   // sai so se vier algo depois
      } else if (np && np + s + 3 < sizeof pend) { pend[np++] = ' '; memcpy(pend + np, p, s); np += s; }
      p += s;
    }
  }
#undef SEP_SOLTA
  while (np && (pend[np - 1] == ' ' || sepIdioma(pend + np - 1))) np--;
  if (np && !poeTexto(out, cap, k, 'T', pend, np)) goto cheio;
  if (!bandeiras) { *k = k0; out[*k] = 0; return 0; }
  if (!poe(out, cap, k, "M", 1) || !poeTexto(out, cap, k, 'T', limpo, l) || !poe(out, cap, k, "E", 1)) goto cheio;
  return 1;
cheio:
  *k = k0; out[*k] = 0;
  return -1;
}

// Texto entre dois icones -> uma peca 'T' limpa (ou nada, se so tinha enfeite).
// `idioma`: veio depois de um icone de legenda/audio (ver idiomas()).
static int texto(const char *ini, size_t n, char *out, size_t cap, size_t *k, int idioma,
                 int (*existe)(const char *cc)) {
  char cru[2048], limpo[2048];
  int r;
  if (!n) return 1;
  if (n >= sizeof cru) n = sizeof cru - 1;
  memcpy(cru, ini, n); cru[n] = 0;
  nv_limpar_texto(cru, limpo, sizeof limpo, NV_LIMPA_UMA_LINHA);
  nv_aparar_separadores(limpo);
  if (!limpo[0]) return 1;
  if (idioma && (r = idiomas(limpo, out, cap, k, existe)) != 0) return r < 0 ? -1 : 2;
  return poeTexto(out, cap, k, 'T', limpo, strlen(limpo)) ? 2 : -1;
}

int formatador_pecas(const char *in, size_t n, char *out, size_t cap, size_t *k,
                     int (*bandeira_existe)(const char *cc)) {
  char linha[4096];
  size_t i = 0, seg = 0;
  int pecas = 0, r, idioma = 0;
  if (!in || !out || !k || *k >= cap) return 0;
  if (n >= sizeof linha) n = sizeof linha - 1;
  memcpy(linha, in, n); linha[n] = 0;
  while (i < n) {
    size_t len = 0;
    char cc[8], rec[16];
    int ic = (unsigned char)linha[i] >= 0xC2 ? formatador_icone_em(linha + i, &len) : -1;
    size_t lb = ic < 0 && (unsigned char)linha[i] == 0xF0 ? bandeiraEm((const unsigned char *)linha + i, cc) : 0;
    if (lb && bandeira_existe && !bandeira_existe(cc)) lb = 0;
    if (ic >= 0 || lb) {
      if ((r = texto(linha + seg, i - seg, out, cap, k, idioma, bandeira_existe)) < 0) return pecas;
      pecas += r == 2;
      if (ic >= 0) {
        snprintf(rec, sizeof rec, "I%03d", ic);
        if (!poe(out, cap, k, rec, 4)) return pecas;
        idioma = iconeDeIdioma(ic);
      } else {
        // Padrao: a bandeira. Minimalista: o codigo de idioma de sempre ("EN").
        char cod[4];
        size_t k0 = *k;
        const char *id = nv_bandeira_idioma(cc, cod);
        if (!poe(out, cap, k, "S", 1) || !poeTexto(out, cap, k, 'B', cc, strlen(cc)) ||
            !poe(out, cap, k, "M", 1) || !poeTexto(out, cap, k, 'T', id, strlen(id)) ||
            !poe(out, cap, k, "E", 1)) { *k = k0; out[*k] = 0; return pecas; }
        i += lb;
        // VS16 colado na bandeira (raro, mas acontece em copia de texto).
        while (i < n) { int m; unsigned long c = dec((const unsigned char *)linha + i, &m); if (!ehVS(c)) break; i += (size_t)m; }
        seg = i; pecas++;
        continue;
      }
      pecas++;
      i += len; seg = i;
      continue;
    }
    { int m; dec((const unsigned char *)linha + i, &m); i += (size_t)m; }
  }
  if ((r = texto(linha + seg, i - seg, out, cap, k, idioma, bandeira_existe)) == 2) pecas++;
  return pecas;
}
