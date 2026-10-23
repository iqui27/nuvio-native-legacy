// Ver limpa.h.
#include "limpa.h"
#include "dobra.h"
#include <string.h>

typedef enum {
  K_VIS,    // desenhavel: sai como esta (ou como `rep`)
  K_ESP,    // espaco de qualquer largura
  K_NL,     // quebra de linha
  K_SEP,    // decoracao que so separa: vira " · " entre dois textos
  K_NADA,   // invisivel ou sem glifo em fonte nenhuma: some sem deixar marca
  K_BAND    // indicador regional (metade de uma bandeira)
} Tipo;

// Bandeira -> idioma. O que o formatador quer dizer com 🇬🇧 ao lado do audio
// e "ingles", nao "Reino Unido"; "GB" ainda seria lido como gigabyte ao lado
// de "1.77 GB". Pais que nao esta aqui sai com o proprio codigo de pais.
static const struct { char pais[3], idioma[4]; } BANDEIRAS[] = {
  { "GB", "EN" }, { "US", "EN" }, { "AU", "EN" }, { "CA", "EN" }, { "IE", "EN" },
  { "BR", "PT" }, { "PT", "PT" }, { "ES", "ES" }, { "MX", "ES" }, { "AR", "ES" },
  { "CO", "ES" }, { "CL", "ES" }, { "FR", "FR" }, { "BE", "FR" }, { "DE", "DE" },
  { "AT", "DE" }, { "CH", "DE" }, { "IT", "IT" }, { "NL", "NL" }, { "RU", "RU" },
  { "UA", "UK" }, { "PL", "PL" }, { "TR", "TR" }, { "JP", "JA" }, { "KR", "KO" },
  { "CN", "ZH" }, { "TW", "ZH" }, { "HK", "ZH" }, { "IN", "HI" }, { "SA", "AR" },
  { "AE", "AR" }, { "EG", "AR" }, { "SE", "SV" }, { "NO", "NO" }, { "DK", "DA" },
  { "FI", "FI" }, { "GR", "EL" }, { "CZ", "CS" }, { "HU", "HU" }, { "RO", "RO" },
  { "IL", "HE" }, { "TH", "TH" }, { "VN", "VI" }, { "ID", "ID" },
};

static const char *idiomaDe(char a, char b, char *bufPais) {
  unsigned i;
  for (i = 0; i < sizeof BANDEIRAS / sizeof *BANDEIRAS; i++)
    if (BANDEIRAS[i].pais[0] == a && BANDEIRAS[i].pais[1] == b) return BANDEIRAS[i].idioma;
  bufPais[0] = a; bufPais[1] = b; bufPais[2] = 0;
  return bufPais;
}

// Decodificador UTF-8 ESTRITO: sequencia invalida, curta, sobre-longa ou
// substituta devolve U+FFFD com n=1, e o laco avanca um byte. Assim a saida
// nunca herda um byte solto da entrada.
static unsigned long decodifica(const unsigned char *p, int *n) {
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

// Classifica um codepoint. `rep` recebe o texto de troca (ASCII ou simbolo da
// Inter) quando o caractere e VISIVEL mas nao pode sair como veio.
static Tipo classifica(unsigned long cp, const char **rep) {
  char c;
  *rep = NULL;
  if (cp < 0x80) {
    if (cp == '\n') return K_NL;
    if (cp == '\t' || cp == '\r' || cp == '\v' || cp == '\f') return cp == '\r' ? K_NADA : K_ESP;
    if (cp < 0x20 || cp == 0x7F) return K_NADA;
    return cp == ' ' ? K_ESP : K_VIS;
  }
  // Letra estilizada: versalete, matematicas, largura cheia, circuladas,
  // sobrescrito/subscrito, quadradas. Vira a letra comum.
  c = nv_dobra_estilizada(cp);
  if (c) { static char um[2]; um[0] = c; um[1] = 0; *rep = um; return K_VIS; }
  if (cp == 0xFFFD || cp < 0xA0) return K_NADA;      // C1 e substituto
  if (cp == 0xA0) return K_ESP;
  if (cp == 0xAD) return K_NADA;                     // hifen opcional
  if (cp < 0x300) return K_VIS;                      // latim
  if (cp <= 0x36F) {                                 // combinantes
    if (cp == 0x34F || (cp >= 0x332 && cp <= 0x338)) return K_NADA;  // riscado/sublinhado
    return K_VIS;
  }
  if (cp < 0x2000) return cp == 0x180E ? K_NADA : K_VIS;   // grego..arabe..tailandes
  if (cp <= 0x206F) {                                // pontuacao geral
    if (cp <= 0x200A || cp == 0x202F || cp == 0x205F) return K_ESP;
    if (cp <= 0x200F) return K_NADA;                 // largura zero, ZWJ, marcas
    if (cp == 0x2028 || cp == 0x2029) return K_NL;
    if (cp >= 0x202A && cp <= 0x202E) return K_NADA;
    if (cp >= 0x2060) return K_NADA;
    if (cp >= 0x2010 && cp <= 0x2012) { *rep = "-"; return K_VIS; }
    if (cp == 0x2015) { *rep = "\xe2\x80\x94"; return K_VIS; }
    if (cp == 0x2016) { *rep = "|"; return K_VIS; }
    if (cp == 0x2023) { *rep = "\xe2\x80\xa2"; return K_VIS; }
    switch (cp) {
      case 0x2013: case 0x2014: case 0x2018: case 0x2019: case 0x201A:
      case 0x201C: case 0x201D: case 0x201E: case 0x2020: case 0x2021:
      case 0x2022: case 0x2026: case 0x2030: case 0x2032: case 0x2033:
      case 0x2039: case 0x203A: return K_VIS;
      default: return K_SEP;
    }
  }
  if (cp <= 0x2BFF) {                                // simbolos, setas, formas, dingbats
    switch (cp) {
      case 0x20AC: case 0x2116: case 0x2122: case 0x2190: case 0x2191:
      case 0x2192: case 0x2193: case 0x2212: case 0x2248: case 0x2264:
      case 0x2265: case 0x221E: case 0x2605: case 0x2713:
        return K_VIS;
      case 0x2705: case 0x2714: case 0x2611: *rep = "\xe2\x9c\x93"; return K_VIS;   // ✅ ✔ ☑ -> ✓
      case 0x274C: case 0x274E: case 0x2716: case 0x2715: case 0x2717:
      case 0x2718: case 0x2612: *rep = "\xc3\x97"; return K_VIS;                    // ❌ ❎ ✖ ✕ ✗ ✘ ☒ -> ×
      case 0x2B50: *rep = "\xe2\x98\x85"; return K_VIS;                             // ⭐ -> ★
      case 0x27A1: case 0x279C: case 0x2794: case 0x27A4: case 0x27A2:
      case 0x2799: case 0x279E: case 0x2B95: case 0x21D2: *rep = "\xe2\x86\x92"; return K_VIS;  // -> →
      default: break;
    }
    if (cp >= 0x2150 && cp <= 0x215E) return K_VIS;          // fracoes
    if (cp >= 0x20A0 && cp <= 0x20CF) return K_NADA;         // moedas raras
    if (cp >= 0x2500 && cp <= 0x25FF) return K_SEP;          // caixa, blocos, formas
    if (cp >= 0x2190 && cp <= 0x21FF) return K_NADA;         // demais setas
    if (cp >= 0x20D0 && cp <= 0x20FF) return K_NADA;         // combinantes de simbolo (keycap)
    return K_SEP;                                            // ⚡ ⚙ ☁ ♪ ✨ ⏳ ⬇ ...
  }
  if (cp >= 0x2E00 && cp <= 0x2E7F) return K_NADA;   // pontuacao suplementar
  if (cp < 0x3000) return K_VIS;                     // glagolitico, copta, radicais CJK
  if (cp == 0x3000) return K_ESP;
  if (cp < 0xD800) return K_VIS;                     // CJK, hangul
  if (cp < 0xF900) return K_SEP;                     // uso privado (icones de fonte)
  if (cp >= 0xFE00 && cp <= 0xFE0F) return K_NADA;   // seletores de variacao
  if (cp >= 0xFE20 && cp <= 0xFE2F) return K_NADA;
  if (cp == 0xFEFF) return K_NADA;
  if (cp >= 0xFFF0 && cp < 0x10000) return K_NADA;
  if (cp < 0x10000) return K_VIS;
  // Fora do BMP. Nenhuma fonte embarcada tem glifo aqui: o que nao e
  // mapeavel some.
  if (cp >= 0x1F1E6 && cp <= 0x1F1FF) return K_BAND;
  if (cp >= 0x1F3FB && cp <= 0x1F3FF) return K_NADA;       // tom de pele
  if (cp == 0x1F31F) { *rep = "\xe2\x98\x85"; return K_VIS; }   // 🌟 -> ★
  if (cp >= 0xE0000 && cp <= 0xE007F) return K_NADA;       // etiquetas de bandeira
  if (cp >= 0x1F000 && cp <= 0x1FBFF) return K_SEP;       // emoji e pictogramas
  return K_NADA;
}

static int semSepAntes(char c) { return c && strchr(")]},.;:!?", c) != NULL; }
static int semSepDepois(char c) { return c && strchr("([{:/-", c) != NULL; }
static int ehSep(char c) { return c && strchr("|\xc2", c) != NULL; }

size_t nv_limpar_texto(const char *in, char *out, size_t tam, int flags) {
  const unsigned char *p = (const unsigned char *)in;
  size_t k = 0;
  int pend = 0;          // 0 nada, 1 espaco visto, 2 separador pendente
  int quebra = 0;        // \n mantido pendente (sem UMA_LINHA)
  char ultima = 0;       // ultimo byte visivel escrito (ASCII ou 0xC2 de "·")
  int ultimaBand = 0;
  int semTexto = 1;      // nada visivel nesta linha ainda
  if (!out || !tam) return 0;
  out[0] = 0;
  if (!in) return 0;
  while (*p) {
    int n = 1;
    unsigned long cp = decodifica(p, &n);
    const char *rep = NULL;
    Tipo t = classifica(cp, &rep);
    const unsigned char *fonte = p;
    char band[8];
    p += n;
    if (t == K_BAND) {
      // Metade de bandeira: junta com a seguinte. Sozinha vira a letra.
      int n2 = 1;
      unsigned long c2 = *p ? decodifica(p, &n2) : 0;
      char a = (char)('A' + (cp - 0x1F1E6));
      if (c2 >= 0x1F1E6 && c2 <= 0x1F1FF) {
        char pb[3];
        char b = (char)('A' + (c2 - 0x1F1E6));
        const char *id = idiomaDe(a, b, pb);
        p += n2;
        strcpy(band, id);
      } else { band[0] = a; band[1] = 0; }
      rep = band; t = K_VIS;
      // Duas bandeiras coladas ("🇬🇧🇧🇷") viram "EN PT", nao "ENPT".
      if (ultimaBand && pend == 0) pend = 1;
    } else ultimaBand = 0;
    if (t == K_NADA) continue;
    if (t == K_ESP) { if (!pend) pend = 1; continue; }
    if (t == K_NL) {
      if (flags & NV_LIMPA_UMA_LINHA) { if (!semTexto) pend = 2; }
      else if (!semTexto) { quebra = 1; pend = 0; semTexto = 1; ultima = 0; }
      continue;
    }
    if (t == K_SEP) { if (!semTexto) pend = 2; continue; }
    {
      const char *txt = rep ? rep : (const char *)fonte;
      size_t len = rep ? strlen(rep) : (size_t)n;
      char primeira = txt[0];
      char eSep = (primeira == '|') || ((unsigned char)primeira == 0xC2 && len > 1 &&
                                        (unsigned char)txt[1] == 0xB7);
      size_t pre = 0;
      char pfx[8];
      if (quebra) { pfx[pre++] = '\n'; quebra = 0; }
      else if (!semTexto) {
        if (pend == 2) {
          if (semSepAntes(primeira)) { /* "1080p )" nao leva separador */ }
          else if (eSep || ehSep(ultima) || semSepDepois(ultima)) pfx[pre++] = ' ';
          else { pfx[pre++] = ' '; pfx[pre++] = '\xc2'; pfx[pre++] = '\xb7'; pfx[pre++] = ' '; }
        } else if (pend) pfx[pre++] = ' ';
      }
      if (k + pre + len + 1 > tam) break;      // nunca no meio de um codepoint
      memcpy(out + k, pfx, pre); k += pre;
      memcpy(out + k, txt, len); k += len;
      pend = 0; semTexto = 0;
      ultimaBand = rep == band;
      ultima = (unsigned char)txt[len - 1] < 0x80 ? txt[len - 1]
             : (len == 2 && (unsigned char)txt[0] == 0xC2 && (unsigned char)txt[1] == 0xB7) ? '\xc2' : 0;
    }
  }
  out[k] = 0;
  // "(🌐)" e "[⚡]" ficam "()" e "[]" quando so havia decoracao dentro: sai o par.
  { size_t r, w = 0;
    for (r = 0; r < k; r++) {
      if ((out[r] == '(' && out[r + 1] == ')') || (out[r] == '[' && out[r + 1] == ']')) {
        if (w && out[w - 1] == ' ' && (out[r + 2] == ' ' || !out[r + 2])) w--;
        r++;
        continue;
      }
      out[w++] = out[r];
    }
    out[w] = 0; k = w; }
  return k;
}

// ---- nv_aparar_separadores -------------------------------------------------
static size_t sepLen(const char *p) {   // length of a separator char at p, else 0
  if (*p == '|' || *p == '-' || *p == '/' || *p == ',') return 1;
  if (!strncmp(p, "\xE2\x80\xA2", 3)) return 3;   // •
  if (!strncmp(p, "\xC2\xB7", 2)) return 2;       // ·
  return 0;
}
static int soSeparador(const char *a, const char *b) {
  size_t l;
  if (a == b) return 0;
  while (a < b && (l = sepLen(a))) a += l;
  return a == b;
}
size_t nv_aparar_separadores(char *s) {
  char *w = s;
  const char *p = s;
  int ultimaSep = 1;   // "start of line": a separator here is dangling
  const char *pendSep = NULL; size_t pendLen = 0;
  while (*p) {
    const char *ini, *fim;
    size_t l;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) break;
    ini = p;
    while (*p && *p != ' ' && *p != '\t') p++;
    fim = p;
    if (soSeparador(ini, fim)) {
      if (!ultimaSep) { pendSep = ini; pendLen = (size_t)(fim - ini); ultimaSep = 1; }
      else if (pendSep) { pendSep = ini; pendLen = (size_t)(fim - ini); }   // doubled: keep the last
      continue;
    }
    // Separators glued to the edges of the word at the line's start/end go away.
    if (w == s) while (ini < fim && (l = sepLen(ini))) ini += l;
    { const char *q = fim;
      while (*q == ' ' || *q == '\t') q++;
      if (!*q) for (;;) {
        const char *b = ini; size_t last = 0;
        for (const char *t = ini; t < fim; ) { size_t k = sepLen(t); last = k ? (size_t)(fim - t) : 0; if (k && t + k == fim) { fim = t; last = 1; break; } t += k ? k : 1; last = 0; }
        if (!last || fim == b) break;
      } }
    if (ini >= fim) continue;
    if (pendSep && w != s) { *w++ = ' '; memmove(w, pendSep, pendLen); w += pendLen; }
    if (w != s) *w++ = ' ';
    memmove(w, ini, (size_t)(fim - ini)); w += fim - ini;
    pendSep = NULL; ultimaSep = 0;
  }
  *w = 0;
  return (size_t)(w - s);
}
