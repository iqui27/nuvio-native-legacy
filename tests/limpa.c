// #144: texto de addon limpo so tem glifo que a Inter embarcada desenha, em
// UTF-8 valido, mesmo cortado em buffer pequeno. Corpus em limpa_corpus.h.
//   uso: nuvio-limpa <arquivo com os codepoints da Inter, um hex por linha>
// (tests/limpa.sh gera o arquivo com tools/glifos.py).
#include "../src/limpa.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *inter;   // 0x110000 flags

static unsigned long proximo(const unsigned char **pp) {
  const unsigned char *p = *pp;
  unsigned long cp; int n;
  if (*p < 0x80) { cp = *p; n = 1; }
  else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; n = 2; }
  else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; n = 3; }
  else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; n = 4; }
  else return 0xFFFFFFFFul;                 // byte solto
  for (int i = 1; i < n; i++) {
    if ((p[i] & 0xC0) != 0x80) return 0xFFFFFFFFul;   // sequencia quebrada
    cp = cp << 6 | (p[i] & 0x3F);
  }
  *pp = p + n;
  return cp;
}

// A saida so pode ter: UTF-8 valido; glifo da Inter; ou letra de outra escrita
// (>= U+0370, fora das faixas de simbolo/emoji), que text.c manda para a
// fonte de reserva.
static void confere(const char *s, const char *contexto) {
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned long cp = proximo(&p);
    if (cp == 0xFFFFFFFFul) { printf("UTF-8 quebrado em: %s\n", contexto); abort(); }
    if (cp == '\n') continue;
    if (cp < 0x110000 && inter[cp]) continue;
    if (cp >= 0x370 && !(cp >= 0x2000 && cp <= 0x2BFF) && !(cp >= 0xFE00 && cp <= 0xFE0F) &&
        cp < 0x10000) continue;
    printf("sem glifo U+%04lX em: %s\n", cp, contexto);
    abort();
  }
}

// Corpus: tests/limpa_corpus.txt (ver o cabecalho dele).
typedef struct { char in[1024], uma[1024], multi[1024]; } CasoLimpa;
static CasoLimpa *CASOS;
static size_t nCasos;

static void desescapa(char *s) {
  char *w = s;
  for (; *s; s++) {
    if (*s == '\\' && s[1] == 'n') { *w++ = '\n'; s++; }
    else if (*s == '\\' && s[1] == 'r') { *w++ = '\r'; s++; }
    else if (*s == '\\' && s[1] == 't') { *w++ = '\t'; s++; }
    else if (*s == '\\' && s[1] == 'x' && s[2] && s[3]) {
      char h[3] = { s[2], s[3], 0 };
      *w++ = (char)strtoul(h, NULL, 16); s += 3;
    } else *w++ = *s;
  }
  *w = 0;
}

static void lerCorpus(const char *caminho) {
  FILE *f = fopen(caminho, "r");
  char linha[1024];
  int etapa = 0;
  assert(f);
  CASOS = calloc(256, sizeof *CASOS);
  while (fgets(linha, sizeof linha, f)) {
    size_t n = strlen(linha);
    if (n && linha[n - 1] == '\n') linha[--n] = 0;
    if (linha[0] == '#' || (etapa == 0 && !n)) continue;
    if (etapa == 3) { assert(!strcmp(linha, "--")); etapa = 0; nCasos++; continue; }
    desescapa(linha);
    strcpy(etapa == 0 ? CASOS[nCasos].in : etapa == 1 ? CASOS[nCasos].uma : CASOS[nCasos].multi, linha);
    etapa++;
  }
  fclose(f);
}

int main(int argc, char **argv) {
  char out[512];
  FILE *f;
  char linha[32];
  size_t i;
  assert(argc == 3);
  lerCorpus(argv[2]);
  inter = calloc(0x110000, 1);
  f = fopen(argv[1], "r");
  assert(f);
  while (fgets(linha, sizeof linha, f)) inter[strtoul(linha, NULL, 16)] = 1;
  fclose(f);
  assert(inter['A'] && inter[0xE9]);          // o arquivo e de verdade

  assert(nCasos >= 20);
  for (i = 0; i < nCasos; i++) {
    nv_limpar_texto(CASOS[i].in, out, sizeof out, NV_LIMPA_UMA_LINHA);
    if (strcmp(out, CASOS[i].uma)) {
      printf("caso %zu (uma linha)\n  esperado: %s\n  saiu:     %s\n", i, CASOS[i].uma, out);
      return 1;
    }
    confere(out, CASOS[i].uma);
    nv_limpar_texto(CASOS[i].in, out, sizeof out, 0);
    if (strcmp(out, CASOS[i].multi)) {
      printf("caso %zu (multilinha)\n  esperado: %s\n  saiu:     %s\n", i, CASOS[i].multi, out);
      return 1;
    }
    confere(out, CASOS[i].multi);
    // Idempotente: limpar de novo nao muda nada.
    { char de_novo[512];
      nv_limpar_texto(out, de_novo, sizeof de_novo, 0);
      assert(!strcmp(out, de_novo)); }
  }

  // Cortar em buffer pequeno nunca parte um codepoint, em nenhum tamanho.
  for (i = 0; i < nCasos; i++) {
    size_t tam;
    for (tam = 1; tam < 64; tam++) {
      char pequeno[64];
      size_t n = nv_limpar_texto(CASOS[i].in, pequeno, tam, NV_LIMPA_UMA_LINHA);
      assert(n < tam && pequeno[n] == 0);
      confere(pequeno, "corte");
    }
  }

  // Entradas de borda.
  assert(nv_limpar_texto(NULL, out, sizeof out, 0) == 0 && !out[0]);
  assert(nv_limpar_texto("", out, sizeof out, 0) == 0);
  assert(nv_limpar_texto("abc", out, 0, 0) == 0);
  { char um[1] = { 'x' }; assert(nv_limpar_texto("abc", um, 1, 0) == 0 && !um[0]); }
  // nv_aparar_separadores (add-on text mode)
  { static const char *const C[][2] = {
      { "31.4 GB | 43.2 Mbps |", "31.4 GB | 43.2 Mbps" },
      { "| A |  | B |", "A | B" },
      { "  -  A  \xE2\x80\xA2 \xC2\xB7 B / ", "A \xC2\xB7 B" },
      { "WEB-DL x265-ELiTE", "WEB-DL x265-ELiTE" },
      { "EN | PT,", "EN | PT" },
      { "|", "" }, { " | - ", "" }, { "", "" },
      { "A   B", "A B" },
    };
    for (i = 0; i < (int)(sizeof C / sizeof C[0]); i++) {
      char b[128];
      snprintf(b, sizeof b, "%s", C[i][0]);
      nv_aparar_separadores(b);
      if (strcmp(b, C[i][1])) { fprintf(stderr, "aparar: '%s' -> '%s' (want '%s')\n", C[i][0], b, C[i][1]); return 1; }
    } }
  puts("limpa: ok");
  return 0;
}
