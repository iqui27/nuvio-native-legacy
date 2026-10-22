#include "js.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static const char *pula(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

const char *js_fim(const char *p) {
  int prof = 0, texto = 0;
  char abre, fecha;
  if (!p) return NULL;
  abre = *p; fecha = (abre == '[') ? ']' : '}';
  if (abre != '[' && abre != '{') return p;
  for (; *p; p++) {
    if (texto) { if (*p == '\\' && p[1]) p++; else if (*p == '"') texto = 0; continue; }
    if (*p == '"') texto = 1;
    else if (*p == abre) prof++;
    else if (*p == fecha && --prof == 0) return p + 1;
  }
  return p;
}

// Acha `"chave"` dentro da faixa, ignorando ocorrencias dentro de textos.
//
// BUSCA LIMITADA A [ini,fim), e nao strstr. Medido no registro de uma Samsung
// (Tizen 9, 2 GB, 20/09/2026): 256 pastas de colecao vindas da conta custavam
// 12,4 s de fio principal parado — `upd=12737` no [quadro] — e 157 pastas,
// 1,2 s. O strstr procurava a chave ate o FIM DO DOCUMENTO inteiro e so
// depois a faixa recusava o achado: cada chave ausente numa pasta
// (heroBackdropUrl, focusGifUrl, genre...) era uma varredura do blob inteiro,
// e sao oito chaves assim por pasta. Quadratico no numero de pastas, e a
// escolha de perfil e o sync pagavam por ele. Agora a varredura para em `fim`.
static const char *pulaEm(const char *p, const char *fim) {
  while (p < fim && *p && (unsigned char)*p <= ' ') p++;
  return p;
}

/* `p` e a aspa inicial. O limite tambem vale para strings e seus escapes,
 * inclusive quando a faixa nao tem NUL (objeto dentro de uma resposta). */
static const char *fimTextoEm(const char *p, const char *fim) {
  for (p++; p < fim && *p; p++) {
    if (*p == '"') return p;
    if (*p == '\\') {
      if (fim - p < 2 || !p[1]) return NULL;
      p++;
    }
  }
  return NULL;
}
static const char *achaChave(const char *ini, const char *fim, const char *chave) {
  const char *p = ini;
  size_t n;
  if (!ini || !chave) return NULL;
  n = strlen(chave);
  if (!fim) fim = ini + strlen(ini);
  while (p < fim && (p = memchr(p, '"', (size_t)(fim - p))) != NULL) {
    const char *q = fimTextoEm(p, fim), *v;
    if (!q) return NULL;
    v = pulaEm(q + 1, fim);
    if ((size_t)(q - p - 1) == n && !memcmp(p + 1, chave, n) &&
        v < fim && *v == ':') return v + 1;
    p = q + 1;
  }
  return NULL;
}

int js_tem(const char *ini, const char *fim, const char *chave) {
  return achaChave(ini, fim, chave) != NULL;
}

// \uXXXX VIRA UTF-8, e nao espaco. PHP json_encode — o que todo painel Xtream
// Codes roda — escapa TODO caractere fora do ASCII por padrao, entao
// "Not\u00edcias" e o caso comum e nao a excecao, e virava "Not cias" na tela.
// Par de substitutos (emoji, e o que os addons de canal poem no nome) vira um
// codepoint de 4 bytes. Sequencia invalida vira espaco, como antes. Devolve
// quantos caracteres de `p` (depois do 'u') foram consumidos: 4 ou 10.
static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static int hex4(const char *p, const char *fim, unsigned *v) {
  int i, h; *v = 0;
  if (fim - p < 4) return 0;
  for (i = 0; i < 4; i++) { h = hexVal(p[i]); if (h < 0) return 0; *v = (*v << 4) | (unsigned)h; }
  return 1;
}
static int escapeU(const char *p, const char *fim, char *dst, size_t *k, size_t tam) {
  unsigned cp, lo;
  int usados = 4;
  if (!hex4(p, fim, &cp)) { if (*k + 1 < tam) dst[(*k)++] = ' '; return 0; }
  if (cp >= 0xD800 && cp <= 0xDBFF && fim - p >= 10 && p[4] == '\\' && p[5] == 'u' && hex4(p + 6, fim, &lo) &&
      lo >= 0xDC00 && lo <= 0xDFFF) {
    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
    usados = 10;
  }
  if (cp >= 0xD800 && cp <= 0xDFFF) cp = ' ';
  if (cp < 0x80) { if (*k + 1 < tam) dst[(*k)++] = (char)cp; }
  else if (cp < 0x800) { if (*k + 2 < tam) { dst[(*k)++] = (char)(0xC0 | (cp >> 6)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  else if (cp < 0x10000) { if (*k + 3 < tam) { dst[(*k)++] = (char)(0xE0 | (cp >> 12)); dst[(*k)++] = (char)(0x80 | ((cp >> 6) & 0x3F)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  else { if (*k + 4 < tam) { dst[(*k)++] = (char)(0xF0 | (cp >> 18)); dst[(*k)++] = (char)(0x80 | ((cp >> 12) & 0x3F)); dst[(*k)++] = (char)(0x80 | ((cp >> 6) & 0x3F)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  return usados;
}

static int lerTextoEm2(const char *p, const char *fim, char *dst, size_t tam, int linhas) {
  const char *fecha;
  size_t k = 0;
  if (p >= fim || *p != '"' || !(fecha = fimTextoEm(p, fim))) return 0;
  p++;
  while (p < fecha && k + 1 < tam) {
    if (*p == '\\' && fecha - p >= 2) {
      p++;
      if (*p == 'u') { int u = escapeU(p + 1, fecha, dst, &k, tam); p += 1 + u; continue; }   /* invalido: so o "u" sai, o resto e texto */
      // linhas: o \n do JSON fica como quebra de linha (o texto de fonte do
      // addon, que a folha desenha linha a linha); \r some, \t vira espaco.
      if (linhas && *p == 'n') { p++; dst[k++] = '\n'; continue; }
      if (linhas && *p == 'r') { p++; continue; }
      if (*p == 'n' || *p == 't' || *p == 'r') { p++; dst[k++] = ' '; continue; }
      if (*p == '/' ) { p++; dst[k++] = '/'; continue; }
    }
    dst[k++] = *p++;
  }
  // Cortou por FALTA DE ESPACO no meio de um caractere de 2-4 bytes? Uma
  // sinopse em russo ou ucraniano gasta 2 bytes por letra e estoura o buffer
  // de 900 com facilidade; o byte solto no fim vira um quadrado na tela. Volta
  // ate a fronteira do ultimo caractere inteiro.
  if (*p && *p != '"' && k > 0 && ((unsigned char)dst[k - 1] & 0x80)) {
    size_t j = k;
    while (j > 0 && ((unsigned char)dst[j - 1] & 0xC0) == 0x80) j--;
    if (j > 0) {
      unsigned char lead = (unsigned char)dst[j - 1];
      size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (k - (j - 1) < need) k = j - 1;
    }
  }
  dst[k] = 0;
  return k > 0;
}

static int lerTextoEm(const char *p, const char *fim, char *dst, size_t tam) {
  return lerTextoEm2(p, fim, dst, tam, 0);
}

// Elemento de texto que ja se tem na mao (p na aspa de abertura), sem chave
// para procurar: "genre":["Not\u00edcias"]. Mesmo decodificador do js_texto.
int js_cadeia(const char *p, char *dst, size_t tam) {
  if (!p || tam == 0) return 0;
  return lerTextoEm(p, p + strlen(p), dst, tam);
}

int js_texto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam) {
  const char *p;
  if (!dst || !tam) return 0;
  if (!ini || !chave) return 0;
  if (!fim) fim = ini + strlen(ini);
  p = achaChave(ini, fim, chave);
  return p ? lerTextoEm(pulaEm(p, fim), fim, dst, tam) : 0;
}

// Como js_texto, mas o \n do JSON vira '\n' (nao espaco).
int js_texto_linhas(const char *ini, const char *fim, const char *chave,
                    char *dst, size_t tam) {
  const char *p;
  if (!dst || !tam) return 0;
  if (!ini || !chave) return 0;
  if (!fim) fim = ini + strlen(ini);
  p = achaChave(ini, fim, chave);
  return p ? lerTextoEm2(pulaEm(p, fim), fim, dst, tam, 1) : 0;
}

double js_num(const char *ini, const char *fim, const char *chave, double padrao) {
  const char *p = ini;
  if (!ini || !chave) return padrao;
  if (!fim) fim = ini + strlen(ini);
  while ((p = achaChave(p, fim, chave)) != NULL) {
    const char *q = pulaEm(p, fim);
    if (q < fim) {
      // O valor pode vir ENTRE ASPAS. O Cinemeta manda `"imdbRating": "8.1"`
      // como string, e recusar a aspa aqui fazia js_num devolver o padrao —
      // por isso a nota era sempre 0: nem o selo do IMDb no hero nem a aba de
      // avaliacoes chegavam a aparecer, sem erro nenhum no caminho.
      if (*q == '"') q++;
      if (q < fim && ((*q >= '0' && *q <= '9') || *q == '-' || *q == '.')) {
        char numero[128], *fimNumero;
        const char *e = q;
        size_t n;
        double v;
        while (e < fim && ((*e >= '0' && *e <= '9') || *e == '-' || *e == '+' ||
                           *e == '.' || *e == 'e' || *e == 'E')) e++;
        n = (size_t)(e - q);
        if (n >= sizeof numero) return padrao;
        memcpy(numero, q, n); numero[n] = 0;
        v = strtod(numero, &fimNumero);
        return fimNumero != numero && isfinite(v) ? v : padrao;
      }
    }
  }
  return padrao;
}

const char *js_array(const char *ini, const char *fim, const char *chave) {
  const char *p;
  if (!ini) return NULL;
  if (!fim) fim = ini + strlen(ini);
  p = achaChave(ini, fim, chave);
  if (!p) return NULL;
  p = pulaEm(p, fim);
  if (p >= fim || *p != '[') return NULL;
  p = pulaEm(p + 1, fim);
  return p < fim && (*p == '{' || *p == '"') ? p : NULL;
}

const char *js_prox(const char *fimAnterior) {
  if (!fimAnterior) return NULL;
  const char *p = pula(fimAnterior);
  if (*p == ',') {
    p = pula(p + 1);
    return (*p == '{' || *p == '"') ? p : NULL;
  }
  return NULL;
}

const char *js_raiz_array(const char *corpo) {
  const char *p;
  if (!corpo) return NULL;
  p = pula(corpo);
  if (*p != '[') return NULL;
  p = pula(p + 1);
  return (*p == '{' || *p == '"') ? p : NULL;
}

int js_bruto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam) {
  const char *p;
  const char *f;
  size_t n;
  if (!dst || !tam) return 0;
  if (!ini || !chave) return 0;
  if (!fim) fim = ini + strlen(ini);
  p = achaChave(ini, fim, chave);
  if (!p) return 0;
  p = pulaEm(p, fim);
  if (p >= fim) return 0;
  if (*p == '{' || *p == '[') {
    int prof = 0;
    char abre = *p, fecha = abre == '[' ? ']' : '}';
    for (f = p; f < fim && *f; f++) {
      if (*f == '"') {
        f = fimTextoEm(f, fim);
        if (!f) return 0;
      } else if (*f == abre) prof++;
      else if (*f == fecha && --prof == 0) { f++; break; }
    }
    if (prof) return 0;
  } else if (*p == '"') {
    // String: o valor pode ser o proprio JSON serializado (o app web aceita as
    // duas formas). Devolve com as aspas; quem consome decide.
    const char *q = fimTextoEm(p, fim);
    if (!q) return 0;
    f = q + 1;
  } else {
    const char *q = p;
    while (q < fim && *q && *q != ',' && *q != '}' && *q != ']') q++;
    while (q > p && (unsigned char)q[-1] <= ' ') q--;
    f = q;
  }
  n = (size_t)(f - p);
  if (n + 1 > tam) return 0;
  memcpy(dst, p, n);
  dst[n] = 0;
  return 1;
}

// Ver a nota em js.h. Veio de syncprog.c, onde era private, quando o segundo
// consumidor apareceu (o `paused_at` do Trakt).
long long js_ms_iso(const char *s) {
  struct tm tm;
  int ano, mes, dia, h = 0, m = 0, seg = 0, frac = 0, n;
  char sep;
  time_t t;
  if (!s || !*s) return 0;
  n = sscanf(s, "%d-%d-%d%c%d:%d:%d", &ano, &mes, &dia, &sep, &h, &m, &seg);
  if (n < 3) return 0;
  memset(&tm, 0, sizeof tm);
  tm.tm_year = ano - 1900; tm.tm_mon = mes - 1; tm.tm_mday = dia;
  tm.tm_hour = h; tm.tm_min = m; tm.tm_sec = seg;
  t = timegm(&tm);
  if (t < 0) return 0;
  { const char *p = strchr(s, '.');
    if (p) { int k = 0; p++; while (*p >= '0' && *p <= '9' && k < 3) { frac = frac * 10 + (*p - '0'); p++; k++; }
             while (k < 3) { frac *= 10; k++; } } }
  return (long long)t * 1000 + frac;
}

// Ver a nota em js.h. Veio de addons.c, onde era o leitor do "id" do manifesto,
// quando o segundo consumidor apareceu (o titulo localizado do TMDB).
int js_texto_raiz(const char *corpo, const char *chave, char *dst, size_t tam) {
  return js_texto_raiz_em(corpo, NULL, chave, dst, tam);
}

// Ver a nota em js.h. Nasceu quando o mesmo defeito do "id" do manifesto
// apareceu UM NIVEL ABAIXO: o "name" de cada catalogo era lido com js_texto
// sobre a faixa do objeto, e o Bingecat escreve `extra` ANTES de `name` — a
// fileira saia batizada de "Skip", "Genre" ou "Search", que sao os nomes dos
// EXTRAS (skip e o de paginacao do Stremio). Foi esse rotulo que fez o relato
// da issue #24 parecer "catalogo que so responde com parametro".
int js_texto_raiz_em(const char *ini, const char *fim, const char *chave,
                     char *dst, size_t tam) {
  const char *p;
  size_t nChave;
  int prof = 0;
  if (!ini || !chave || !dst || tam == 0) return 0;
  dst[0] = 0;
  if (!fim) fim = ini + strlen(ini);
  nChave = strlen(chave);
  if (fim <= ini) return 0;
  p = memchr(ini, '{', (size_t)(fim - ini));
  if (!p) return 0;
  for (; p < fim && *p; p++) {
    if (*p == '"') {
      const char *ini2 = p + 1;
      const char *q = fimTextoEm(p, fim);
      if (!q) return 0;
      if (prof == 1 && (size_t)(q - ini2) == nChave &&
          !strncmp(ini2, chave, nChave)) {
        const char *v = pulaEm(q + 1, fim);
        if (v >= fim || *v != ':') { p = q; continue; }
        v = pulaEm(v + 1, fim);
        // Valor nao-string (numero, null, objeto) devolve 0 em vez de meia
        // leitura: quem chama decide o que fazer com a ausencia.
        return lerTextoEm(v, fim, dst, tam);
      }
      p = q;
      continue;
    }
    if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') { prof--; if (prof <= 0) break; }
  }
  return 0;
}
