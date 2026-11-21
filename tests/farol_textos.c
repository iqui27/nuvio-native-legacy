// Farol — as falas (src/faroltexto.h) seguem o tom do ALMA e viram chave i18n.
//
//   cc tests/farol_textos.c -Isrc -o /tmp/t-faroltextos && /tmp/t-faroltextos
//
// Confere o que a maquina garante melhor que o revisor humano:
//   1. todo FT_* e uma chave de idioma_tab.h (a tabela mestra responde;
//      senao a tela mostra portugues em qualquer idioma);
//   2. no maximo 2 linhas por fala e nenhuma linha com mais de 62
//      caracteres em pt E em en (o teto que a Ilha comporta);
//   3. nenhuma palavra da lista "o que o Farol NUNCA diz" (ALMA 2):
//      saudacao de bot, desculpa corporativa, bajulacao, "inteligencia
//      artificial" e "inesperado" — com ou sem acento, em pt ou en.
// A varredura das 29 irmas (traducao nao vazia, mesmos marcadores e \n)
// fica com tools/idiomas.py, chamado pelo farol_textos.sh junto com a
// checagem de valor nao vazio para cada chave FT_*.
#include "faroltexto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *pt, *en; } TABELA[] = {
#include "idioma_tab.h"
};

static int falhas;

// ---- largura em caracteres UTF-8 (continuacao 0x80..0xBF nao e letra) ----
static int largura_linha(const char *s, const char *fim) {
  int n = 0;
  while (s < fim && *s && *s != '\n') {
    if ((*s & 0xC0) != 0x80) n++;
    s++;
  }
  return n;
}

static void confere_linhas(const char *nome, const char *s) {
  int linhas = 1;
  const char *p = s, *ini = s;
  for (; *p; p++) if (*p == '\n') linhas++;
  if (linhas > 2) { printf("FALHOU: %s tem %d linhas\n", nome, linhas); falhas++; }
  while (*ini) {
    const char *fim = ini;
    while (*fim && *fim != '\n') fim++;
    if (largura_linha(ini, fim) > 62) {
      printf("FALHOU: %s linha com %d caracteres (max 62): \"%.*s\"\n",
             nome, largura_linha(ini, fim), (int)(fim - ini), ini);
      falhas++;
    }
    ini = *fim ? fim + 1 : fim;
  }
}

// ---- "nunca diz": dobra acento -> ascii, minuscula, fronteira de palavra --
static char dobra(unsigned char c) {
  // blocos latinos 1: A-grave..z-tilde e cedilhas, mapeados pela paridade.
  if (c >= 'A' && c <= 'Z') return (char)(c + 32);
  return (char)c;
}
static void dobrar(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 2 < cap; p++) {
    unsigned char c = *p;
    if (c == 0xC3 && p[1]) {          // U+00C0..U+00FF em UTF-8
      unsigned char d = p[1] & 0x3F;  // ponto dentro do bloco
      char base;
      if (d <= 0x05 || (d >= 0x20 && d <= 0x25)) base = 'a';
      else if (d == 0x07 || d == 0x27) base = 'c';
      else if (d <= 0x0B || (d >= 0x28 && d <= 0x2B)) base = 'e';
      else if (d <= 0x0F || (d >= 0x2C && d <= 0x2F)) base = 'i';
      else if (d == 0x11 || d == 0x31) base = 'n';
      else if (d >= 0x12 && d <= 0x16) base = 'o';
      else if (d >= 0x32 && d <= 0x36) base = 'o';
      else if (d >= 0x19 && d <= 0x1C) base = 'u';
      else if (d >= 0x39 && d <= 0x3C) base = 'u';
      else if (d == 0x1D || d == 0x3D || d == 0x3F) base = 'y';
      else base = 0;
      p++;
      if (!base) continue;
      out[o++] = base;
      continue;
    }
    out[o++] = dobra(c);
  }
  out[o] = 0;
}
static int letra(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
static int tem_palavra(const char *dobrada, const char *agulha) {
  size_t n = strlen(agulha);
  for (const char *p = dobrada; (p = strstr(p, agulha)); p += n) {
    int esq = p == dobrada || !letra(p[-1]);
    int dir = !letra(p[n]);
    if (esq && dir) return 1;
  }
  return 0;
}

// Lista do ALMA 2 + os mesmos vicios em ingles (a fala inglesa tambem passa
// pela regra, senao a tela en virava o esconderijo das frases proibidas).
static const char *PROIBIDAS[] = {
  "desculpe", "desculpas", "sorry", "apologize", "apologies",
  "ola", "oi", "hello", "hi there", "hey",
  "prontinho", "there you go", "all done",
  "otima escolha", "otima", "great choice", "excellent choice",
  "uau", "wow", "parabens",
  "inteligencia artificial", "artificial intelligence",
  "inesperado", "inesperada", "unexpected",
  "estou aqui para ajudar", "how can i help", "how may i help",
  "tudo certinho",
};

static void confere_proibidas(const char *nome, const char *s) {
  char d[512];
  dobrar(s, d, sizeof d);
  for (size_t i = 0; i < sizeof PROIBIDAS / sizeof *PROIBIDAS; i++)
    if (tem_palavra(d, PROIBIDAS[i])) {
      printf("FALHOU: %s usa a palavra proibida \"%s\": \"%s\"\n",
             nome, PROIBIDAS[i], s);
      falhas++;
    }
}

// ---- cada FT_ tem de existir na mestra (ordem strcmp = busca binaria) ----
static int cmp_pt(const void *a, const void *b) {
  return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static const char *en_de(const char *pt) {
  const char *k = pt;
  const void *r = bsearch(&k, TABELA, sizeof TABELA / sizeof *TABELA,
                          sizeof *TABELA, cmp_pt);
  return r ? ((const struct { const char *pt, *en; } *)r)->en : NULL;
}

int main(void) {
  static const char *FRASES[] = {
#define X(nome) FT_##nome,
    FAROL_FRASES(X)
#undef X
  };
  size_t n = sizeof FRASES / sizeof *FRASES;

  // A mestra tem de estar ordenada para a busca binaria valer (e para o
  // proprio i18n); o idiomas.py confere de novo, aqui e pre-condicao.
  for (size_t i = 1; i < sizeof TABELA / sizeof *TABELA; i++) {
    if (strcmp(TABELA[i - 1].pt, TABELA[i].pt) >= 0) {
      printf("FALHOU: mestra fora de ordem em \"%s\"\n", TABELA[i].pt);
      return 1;
    }
  }

  for (size_t i = 0; i < n; i++) {
    const char *pt = FRASES[i];
    const char *en = en_de(pt);
    char nome[64];
    snprintf(nome, sizeof nome, "FT_%zu", i);
    if (!en) { printf("FALHOU: \"%s\" nao e chave da mestra\n", pt); falhas++; continue; }
    if (!*en) { printf("FALHOU: \"%s\" sem ingles\n", pt); falhas++; }
    confere_linhas(pt, pt);
    confere_linhas(pt, en);
    confere_proibidas(pt, pt);
    confere_proibidas(pt, en);
  }

  // A chave antiga fica ate a issue 23 trocar o rotulo visivel.
  if (!en_de("Resolver um problema")) {
    printf("FALHOU: \"Resolver um problema\" sumiu da mestra\n"); falhas++;
  }
  // O nome proprio responde como chave tambem (Beacon na mestra).
  const char *en = en_de(FT_NOME);
  if (!en || strcmp(en, "Beacon") != 0) {
    printf("FALHOU: FT_NOME nao responde \"Beacon\" (responde \"%s\")\n",
           en ? en : "(sem chave)"); falhas++;
  }

  if (falhas) { printf("farol_textos: %d falha(s)\n", falhas); return 1; }
  printf("farol_textos: %zu frases ok\n", n);
  return 0;
}
