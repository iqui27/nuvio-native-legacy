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
//      artificial" e "inesperado" — com ou sem acento, em pt ou en;
//   4. os argumentos das chaves formatadas caem no PAPEL certo: a duracao
//      de FT_ACHA_L1 dentro da clausula de tempo em zh, e o formato de
//      FT_FONTE_TROCADA sem preposicao temporal grudada — renderizado pelo
//      caminho real (i18n de src/idioma.c + snprintf), nao por grep;
//   5. toda fala da casa passa pelo MESMO filtro do texto livre do LLM,
//      faroltexto_aceita() de src/faroltexto.c (issue 35) — se a frase da
//      casa nao passasse, o filtro reprovaria o proprio Farol.
// A varredura das 29 irmas (traducao nao vazia, mesmos marcadores e \n)
// fica com tools/idiomas.py, chamado pelo farol_textos.sh junto com a
// checagem de valor nao vazio para cada chave FT_*.
#include "faroltexto.h"
#include "idioma.h"
#include "idiomacod.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Duple: o teste nao sobe ajustes.c inteiro so para saber o idioma —
// i18n() pergunta por aqui e o teste troca antes de cada render.
static int lg_teste = IDIOMA_EN;
int ajustes_idioma(void) { return lg_teste; }
int ajustes_idioma_ingles(void) { return lg_teste == IDIOMA_EN; }

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

// ---- semantica dos argumentos, renderizada pela i18n de verdade ----
//
// FT_ACHA_L1 recebe (quantidade, genero, duracao). Em chines o limite de
// tempo mora numa clausula marcada (短于/短於, 不到/未滿...), com o genero
// fora dela. A sequencia %d %s %s e a mesma do portugues, entao os
// validadores de marcador nao pegam a inversao que a revisao pegou:
// "3 个短于 <genero> 的 <duracao>" lia "3 duracoes mais curtas que um
// suspense". A regra: genero antes do marcador de tempo, duracao depois.
static const char *const MARC_DUR_ZH[] = {
  "短于", "短於", "不到", "未滿", "未满", "以內", "以内", "小於", "小于",
};

static void confere_acha_zh(int lg, const char *cod) {
  char buf[256];
  const char *pg, *pd, *pm = NULL;
  size_t i;
  lg_teste = lg;
  snprintf(buf, sizeof buf, i18n(FT_ACHA_L1), 3, "SUSPQQ", "1H40QQ");
  pg = strstr(buf, "SUSPQQ");
  pd = strstr(buf, "1H40QQ");
  if (!pg || !pd) {
    printf("FALHOU: %s FT_ACHA_L1 nao recebeu os dois argumentos: \"%s\"\n",
           cod, buf);
    falhas++;
    return;
  }
  for (i = 0; i < sizeof MARC_DUR_ZH / sizeof *MARC_DUR_ZH; i++) {
    const char *p = strstr(buf, MARC_DUR_ZH[i]);
    if (p && (!pm || p < pm)) pm = p;
  }
  if (!pm || pg > pm || pd < pm) {
    printf("FALHOU: %s FT_ACHA_L1 com a duracao fora da clausula de tempo: "
           "\"%s\"\n", cod, buf);
    falhas++;
  }
}

// FT_FONTE_TROCADA recebe o FORMATO da fonte nova no %s ("4K HDR10"). A
// palavra encostada no dado nao pode ser marcador de TEMPO — em dezenas de
// idiomas "em/no formato" e a mesma preposicao de "em/no tempo", e a
// revisao pegou "za", "за", "خلال", "alatt", "içinde", "per", "dalam",
// "trong" e "på" lendo "troquei durante 4K HDR10".
static const char *const TEMPO[] = {
  "za", "за", "خلال", "alatt", "içinde", "per", "dalam", "trong", "på",
};

// copia o token espaco-delimitado, dobra A-Z e corta pontuacao ASCII
static void palavra(const char *a, const char *b, char *w, size_t cap) {
  size_t n = 0;
  for (; a < b && n + 1 < cap; a++) {
    unsigned char c = (unsigned char)*a;
    if (c < 0x80 && c != '_' && !(c >= 'a' && c <= 'z') &&
        !(c >= '0' && c <= '9')) {
      if (c >= 'A' && c <= 'Z') { w[n++] = (char)(c + 32); continue; }
      if (n) break;          // pontuacao depois da palavra
      continue;              // pontuacao antes: pula
    }
    w[n++] = (char)c;
  }
  w[n] = 0;
}

static int tempo_ao_lado(const char *buf, const char *sinal) {
  const char *p = strstr(buf, sinal), *a, *b;
  char w[96];
  size_t i;
  if (!p) return 0;
  a = p;
  while (a > buf && a[-1] == ' ') a--;
  for (b = a; a > buf && a[-1] != ' '; a--) ;
  palavra(a, b, w, sizeof w);
  for (i = 0; i < sizeof TEMPO / sizeof *TEMPO; i++)
    if (!strcmp(w, TEMPO[i])) return 1;
  a = p + strlen(sinal);
  while (*a == ' ') a++;
  for (b = a; *b && *b != ' '; b++) ;
  palavra(a, b, w, sizeof w);
  for (i = 0; i < sizeof TEMPO / sizeof *TEMPO; i++)
    if (!strcmp(w, TEMPO[i])) return 1;
  return 0;
}

static void confere_fonte_trocada(void) {
  static const char *const cod[IDIOMA_N] = {
    "pt", "en", "ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr",
    "ptpt", "sv", "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr",
    "bg", "el", "id", "vi", "ja", "zhcn", "zhtw", "ar",
  };
  char buf[256];
  int lg;
  for (lg = 0; lg < IDIOMA_N; lg++) {
    lg_teste = lg;
    snprintf(buf, sizeof buf, i18n(FT_FONTE_TROCADA), "ZZFMTZZ");
    if (!strstr(buf, "ZZFMTZZ")) {
      printf("FALHOU: %s FT_FONTE_TROCADA sem o formato: \"%s\"\n",
             cod[lg], buf);
      falhas++;
    } else if (tempo_ao_lado(buf, "ZZFMTZZ")) {
      printf("FALHOU: %s FT_FONTE_TROCADA le o formato como tempo "
             "decorrido: \"%s\"\n", cod[lg], buf);
      falhas++;
    }
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
    if (!faroltexto_aceita(pt)) {
      printf("FALHOU: pt \"%s\" reprovada por faroltexto_aceita\n", pt);
      falhas++;
    }
    if (en && !faroltexto_aceita(en)) {
      printf("FALHOU: en \"%s\" reprovada por faroltexto_aceita\n", en);
      falhas++;
    }
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

  confere_acha_zh(IDIOMA_ZHCN, "zhcn");
  confere_acha_zh(IDIOMA_ZHTW, "zhtw");
  confere_fonte_trocada();

  if (falhas) { printf("farol_textos: %d falha(s)\n", falhas); return 1; }
  printf("farol_textos: %zu frases ok\n", n);
  return 0;
}
