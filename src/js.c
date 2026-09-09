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
static const char *achaChave(const char *ini, const char *fim, const char *chave) {
  char busca[64];
  const char *p = ini;
  size_t n;
  snprintf(busca, sizeof busca, "\"%s\"", chave);
  n = strlen(busca);
  while ((p = strstr(p, busca)) != NULL) {
    if (fim && p >= fim) return NULL;
    { const char *q = pula(p + n);
      if (*q == ':') return q + 1; }
    p += n;
  }
  return NULL;
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
      if (*p == 'u') { p += 5; dst[k++] = ' '; continue; }
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
  while ((p = strstr(p, busca)) != NULL) {
    const char *q;
    if (fim && p >= fim) break;
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
              // Mesma politica de js_texto: escape vira espaco em vez de
              // decodificar UTF-16, porque estes textos sao para exibicao.
              if (*v == 'u') { v += 4; dst[k++] = ' '; continue; }
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
