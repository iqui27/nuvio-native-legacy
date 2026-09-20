#include "js.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *pula(const char *p) {
  while (*p && (unsigned char)*p <= ' ') p++;
  return p;
}

const char *js_fim(const char *p) {
  int prof = 0, texto = 0;
  char abre = *p, fecha = (abre == '[') ? ']' : '}';
  if (abre != '[' && abre != '{') return p;
  for (; *p; p++) {
    if (texto) { if (*p == '\\') p++; else if (*p == '"') texto = 0; continue; }
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
static const char *achaChaveEm(const char *ini, const char *fim, const char *busca, size_t n) {
  const char *p = ini;
  if (!fim) fim = ini + strlen(ini);
  while (p + n <= fim && (p = memchr(p, '"', (size_t)(fim - p))) != NULL) {
    if (p + n > fim) return NULL;
    if (!memcmp(p, busca, n)) return p;
    p++;
  }
  return NULL;
}
static const char *achaChave(const char *ini, const char *fim, const char *chave) {
  char busca[64];
  const char *p = ini;
  size_t n;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  n = strlen(busca);
  if (!fim) fim = ini + strlen(ini);
  while ((p = achaChaveEm(p, fim, busca, n)) != NULL) {
    { const char *q = pula(p + n);
      if (*q == ':') return q + 1; }
    p += n;
  }
  return NULL;
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
static int hex4(const char *p, unsigned *v) {
  int i, h; *v = 0;
  for (i = 0; i < 4; i++) { h = hexVal(p[i]); if (h < 0) return 0; *v = (*v << 4) | (unsigned)h; }
  return 1;
}
static int escapeU(const char *p, char *dst, size_t *k, size_t tam) {
  unsigned cp, lo;
  int usados = 4;
  if (!hex4(p, &cp)) { if (*k + 1 < tam) dst[(*k)++] = ' '; return 0; }
  if (cp >= 0xD800 && cp <= 0xDBFF && p[4] == '\\' && p[5] == 'u' && hex4(p + 6, &lo) &&
      lo >= 0xDC00 && lo <= 0xDFFF) {
    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
    usados = 10;
  }
  if (cp < 0x80) { if (*k + 1 < tam) dst[(*k)++] = (char)cp; }
  else if (cp < 0x800) { if (*k + 2 < tam) { dst[(*k)++] = (char)(0xC0 | (cp >> 6)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  else if (cp < 0x10000) { if (*k + 3 < tam) { dst[(*k)++] = (char)(0xE0 | (cp >> 12)); dst[(*k)++] = (char)(0x80 | ((cp >> 6) & 0x3F)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  else { if (*k + 4 < tam) { dst[(*k)++] = (char)(0xF0 | (cp >> 18)); dst[(*k)++] = (char)(0x80 | ((cp >> 12) & 0x3F)); dst[(*k)++] = (char)(0x80 | ((cp >> 6) & 0x3F)); dst[(*k)++] = (char)(0x80 | (cp & 0x3F)); } }
  return usados;
}

int js_texto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam) {
  const char *p = achaChave(ini, fim, chave);
  size_t k = 0;
  if (!p) return 0;
  p = pula(p);
  if (*p != '"') return 0;
  p++;
  while (*p && *p != '"' && k + 1 < tam) {
    if (*p == '\\' && p[1]) {
      p++;
      if (*p == 'u') { int u = escapeU(p + 1, dst, &k, tam); p += 1 + u; continue; }   /* invalido: so o "u" sai, o resto e texto */
      if (*p == 'n' || *p == 't' || *p == 'r') { p++; dst[k++] = ' '; continue; }
      if (*p == '/' ) { p++; dst[k++] = '/'; continue; }
    }
    dst[k++] = *p++;
  }
  dst[k] = 0;
  return k > 0;
}

double js_num(const char *ini, const char *fim, const char *chave, double padrao) {
  char busca[64];
  const char *p = ini;
  size_t n;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  n = strlen(busca);
  if (!fim) fim = ini + strlen(ini);
  while ((p = achaChaveEm(p, fim, busca, n)) != NULL) {
    const char *q;
    q = pula(p + n);
    if (*q == ':') {
      q = pula(q + 1);
      // O valor pode vir ENTRE ASPAS. O Cinemeta manda `"imdbRating": "8.1"`
      // como string, e recusar a aspa aqui fazia js_num devolver o padrao —
      // por isso a nota era sempre 0: nem o selo do IMDb no hero nem a aba de
      // avaliacoes chegavam a aparecer, sem erro nenhum no caminho.
      if (*q == '"') q++;
      if ((*q >= '0' && *q <= '9') || *q == '-' || *q == '.') return atof(q);
    }
    p += n;
  }
  return padrao;
}

const char *js_array(const char *ini, const char *fim, const char *chave) {
  const char *p = achaChave(ini, fim, chave);
  if (!p) return NULL;
  p = pula(p);
  if (*p != '[') return NULL;
  p = pula(p + 1);
  return (*p == '{' || *p == '"') ? p : NULL;
}

const char *js_prox(const char *fimAnterior) {
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
  const char *p = achaChave(ini, fim, chave);
  const char *f;
  size_t n;
  if (!p) return 0;
  p = pula(p);
  if (*p == '{' || *p == '[') {
    f = js_fim(p);
  } else if (*p == '"') {
    // String: o valor pode ser o proprio JSON serializado (o app web aceita as
    // duas formas). Devolve com as aspas; quem consome decide.
    const char *q = p + 1;
    while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; }
    f = *q ? q + 1 : q;
  } else {
    const char *q = p;
    while (*q && *q != ',' && *q != '}' && *q != ']') q++;
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
  nChave = strlen(chave);
  p = strchr(ini, '{');
  if (!p || (fim && p >= fim)) return 0;
  for (; *p && (!fim || p < fim); p++) {
    if (*p == '"') {
      const char *ini2 = p + 1;
      const char *q = ini2;
      while (*q && *q != '"') q += (*q == '\\' && q[1]) ? 2 : 1;
      if (prof == 1 && (size_t)(q - ini2) == nChave &&
          !strncmp(ini2, chave, nChave)) {
        const char *v = q + 1;
        while (*v == ' ' || *v == ':' || *v == '\n' || *v == '\t' || *v == '\r') v++;
        // Valor nao-string (numero, null, objeto) devolve 0 em vez de meia
        // leitura: quem chama decide o que fazer com a ausencia.
        if (*v != '"') return 0;
        { size_t k = 0;
          for (v++; *v && *v != '"' && k + 1 < tam; v++) {
            if (*v == '\\' && v[1]) {
              v++;
              // Mesma politica de js_texto: \uXXXX vira UTF-8 (escapeU).
              if (*v == 'u') { int u = escapeU(v + 1, dst, &k, tam); v += u; continue; }
              if (*v == 'n' || *v == 't' || *v == 'r') { dst[k++] = ' '; continue; }
              if (*v == '/') { dst[k++] = '/'; continue; }
            }
            dst[k++] = *v;
          }
          dst[k] = 0;
          return k > 0; }
      }
      p = *q ? q : q - 1;
      continue;
    }
    if (*p == '{' || *p == '[') prof++;
    else if (*p == '}' || *p == ']') { prof--; if (prof <= 0) break; }
  }
  return 0;
}
