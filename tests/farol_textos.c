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
// A MESMA dobradura de src/faroltexto.c (o filtro do texto livre): ponto de
// codigo por vez, latim-1 na base, marcas combinantes fora, branco e
// pontuacao larga num so ' ' e o resto cru — com byte >= 0x80 pesando como
// letra, para "Łoi"/"Þoi" nao abrirem fronteira e "o×i" nao virar "oi".
static long ft_utf8(const char **p) {
  const unsigned char *s = (const unsigned char *)*p;
  unsigned char c = s[0];
  long cp;
  int extra;
  if (c < 0x80) { *p += 1; return c; }
  if (c >= 0xC2 && c <= 0xDF)      { cp = c & 0x1F; extra = 1; }
  else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
  else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
  else return -1;
  for (int i = 1; i <= extra; i++) {
    if ((s[i] & 0xC0) != 0x80) return -1;
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
  if (cp >= 0xD800 && cp <= 0xDFFF) return -1;
  if (cp > 0x10FFFF) return -1;
  *p += extra + 1;
  return cp;
}
static int ft_combinante(long cp) {
  return (cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x1AB0 && cp <= 0x1AFF) ||
         (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE20 && cp <= 0xFE2F);
}
static int ft_branco(long cp) {
  return cp == ' ' || cp == '\n' || cp == 0xA0 ||
         cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x3000;
}
static int ft_pontuacao(long cp) {
  if (cp >= 0xA1 && cp <= 0xBF)
    return cp != 0xAA && cp != 0xB5 && cp != 0xBA;
  if (cp >= 0x25A0 && cp <= 0x25FF) return 1;
  return (cp >= 0x055A && cp <= 0x055F) || (cp >= 0x0589 && cp <= 0x058A) ||
         cp == 0x05BE || cp == 0x05C0 || cp == 0x05C3 || cp == 0x05C6 ||
         (cp >= 0x0609 && cp <= 0x060A) || (cp >= 0x060C && cp <= 0x060D) ||
         cp == 0x061B || (cp >= 0x061D && cp <= 0x061F) ||
         (cp >= 0x066A && cp <= 0x066C) || cp == 0x06D4 ||
         (cp >= 0x0700 && cp <= 0x070D) || (cp >= 0x0964 && cp <= 0x0965) ||
         cp == 0x0970 || (cp >= 0x104A && cp <= 0x104F) || cp == 0x10FB ||
         (cp >= 0x1360 && cp <= 0x1368) || cp == 0x166E ||
         (cp >= 0x16EB && cp <= 0x16ED) || (cp >= 0x1735 && cp <= 0x1736) ||
         (cp >= 0x17D4 && cp <= 0x17D6) || (cp >= 0x17D8 && cp <= 0x17DA) ||
         (cp >= 0x1800 && cp <= 0x180A) || (cp >= 0x1944 && cp <= 0x1945) ||
         (cp >= 0x2000 && cp <= 0x206F) ||
         (cp >= 0x2E00 && cp <= 0x2E7F && cp != 0x2E2F) ||
         (cp >= 0x3001 && cp <= 0x303F) || (cp >= 0xFE10 && cp <= 0xFE1F) ||
         (cp >= 0xFE30 && cp <= 0xFE6F) ||
         (cp >= 0xFF01 && cp <= 0xFF0F) || (cp >= 0xFF1A && cp <= 0xFF20) ||
         (cp >= 0xFF3B && cp <= 0xFF40) || (cp >= 0xFF5B && cp <= 0xFF65);
}
static char ft_base_latim(long cp) {
  static const char T[0x40] = {
    'a','a','a','a','a','a', 0 ,'c',
    'e','e','e','e','i','i','i','i',
     0 ,'n','o','o','o','o','o', 0 ,
     0 ,'u','u','u','u','y', 0 , 0 ,
    'a','a','a','a','a','a', 0 ,'c',
    'e','e','e','e','i','i','i','i',
     0 ,'n','o','o','o','o','o', 0 ,
     0 ,'u','u','u','u','y', 0 ,'y',
  };
  return (cp >= 0xC0 && cp <= 0xFF) ? T[cp - 0xC0] : 0;
}
static void dobrar(const char *in, char *out, size_t cap) {
  size_t o = 0;
  int pend = 0;
  const char *p = in;
  while (*p && o + 5 < cap) {
    const char *ini = p;
    long cp = ft_utf8(&p);
    if (cp < 0) break;
    if (ft_branco(cp))    { pend = 1; continue; }
    if (ft_pontuacao(cp)) { pend = 2; continue; }
    if (ft_combinante(cp)) continue;
    if (pend) {
      if (o) out[o++] = ' ';
      if (pend == 2 && cp == '.') out[o++] = 'x';
      pend = 0;
    }
    if (cp < 0x80) {
      char c = (char)cp;
      out[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
      continue;
    }
    char base = ft_base_latim(cp);
    if (base) { out[o++] = base; continue; }
    if (cp >= 0x249C && cp <= 0x24B5) { out[o++] = (char)('a' + cp - 0x249C); continue; }
    if (cp >= 0x24B6 && cp <= 0x24CF) { out[o++] = (char)('a' + cp - 0x24B6); continue; }
    if (cp >= 0x24D0 && cp <= 0x24E9) { out[o++] = (char)('a' + cp - 0x24D0); continue; }
    while (ini < p && o + 1 < cap) out[o++] = *ini++;
  }
  out[o] = 0;
}
static int letra(int c) {
  unsigned char u = (unsigned char)c;
  return (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u >= 0x80;
}
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
  // jargao cru na primeira linha (ALMA 2: nada de MEDIA_ERR_DECODE,
  // "HTTP 206 Partial Content", HEVC Main10 DV P5 ou ring buffer)
  "http", "https", "media_err_decode", "hevc", "main10", "dv p5",
  "partial content", "ring buffer", "mediacodec", "dns", "tls",
  // culpa ao usuario (ALMA 2: nada de "sua internet esta ruim" nem
  // "voce escolheu uma fonte incompativel" — o fato neutro, sim)
  "internet esta ruim", "internet caiu", "fonte incompativel",
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

static const char *const COD[IDIOMA_N] = {
  "pt", "en", "ro", "uk", "ru", "fr", "de", "es", "it", "nl", "pl", "tr",
  "ptpt", "sv", "da", "no", "cs", "sk", "sl", "hu", "lt", "bs", "sr",
  "bg", "el", "id", "vi", "ja", "zhcn", "zhtw", "ar",
};

static void confere_fonte_trocada(void) {
  char buf[256];
  int lg;
  for (lg = 0; lg < IDIOMA_N; lg++) {
    lg_teste = lg;
    snprintf(buf, sizeof buf, i18n(FT_FONTE_TROCADA), "ZZFMTZZ");
    if (!strstr(buf, "ZZFMTZZ")) {
      printf("FALHOU: %s FT_FONTE_TROCADA sem o formato: \"%s\"\n",
             COD[lg], buf);
      falhas++;
    } else if (tempo_ao_lado(buf, "ZZFMTZZ")) {
      printf("FALHOU: %s FT_FONTE_TROCADA le o formato como tempo "
             "decorrido: \"%s\"\n", COD[lg], buf);
      falhas++;
    }
  }
}

// ---- semantica dos argumentos 2x%d, renderizada pela i18n de verdade ----
//
// FT_ACHA_CURTOS_L1 recebe (quantidade, teto de minutos). A revisao pegou o
// ja ligando a contagem na clausula de tempo e os minutos no contador:
// "(3, 100)" rendia "3 分未満の短編 100 本" — 100 curtas de menos de 3
// minutos. A regra: o numero imediatamente antes do contador 本 e a
// quantidade; o que precede 分 e o teto de minutos.
static int numero_antes(const char *buf, const char *marc) {
  const char *p = strstr(buf, marc);
  int v = 0;
  if (!p) return -1;
  while (p > buf && p[-1] == ' ') p--;
  if (p == buf || p[-1] < '0' || p[-1] > '9') return -1;
  while (p > buf && p[-1] >= '0' && p[-1] <= '9') p--;
  while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
  return v;
}

static void confere_acha_curtos_ja(void) {
  char buf[256];
  lg_teste = IDIOMA_JA;
  snprintf(buf, sizeof buf, i18n(FT_ACHA_CURTOS_L1), 3, 100);
  if (numero_antes(buf, "本") != 3 || numero_antes(buf, "分") != 100) {
    printf("FALHOU: ja FT_ACHA_CURTOS_L1 troca contagem e duracao: \"%s\"\n",
           buf);
    falhas++;
  }
}

// FT_INFO_DUAS_NOITES recebe (noites, minutos por noite). A revisao pegou o
// hu lendo a duracao como frequencia: "(2, 50)" rendia "2 este, 50
// percenként." — "a cada 50 minutos". A regra: os minutos medem a noite,
// nao a frequencia ("percenként" e "por minuto", nao "por noite").
static void confere_duas_noites_hu(void) {
  char buf[256];
  lg_teste = IDIOMA_HU;
  snprintf(buf, sizeof buf, i18n(FT_INFO_DUAS_NOITES), 2, 50);
  if (strstr(buf, "percenként") || !strstr(buf, "50 perc")) {
    printf("FALHOU: hu FT_INFO_DUAS_NOITES le duracao como frequencia: "
           "\"%s\"\n", buf);
    falhas++;
  }
}

// ---- o filtro de producao sobre as regressoes da revisao -----------------
// As aspas e a pontuacao latina sao separador, nao letra: « » e ¡ ¿ nao
// reabrem a fronteira de URL nem de palavra proibida; o circulo textual
// U+25CB segue fora da lista de emoji (so os pontos com apresentacao emoji
// do bloco geometrico sao vedados), mas pesa como separador na dobra —
// "○exemplo.com○" nao esconde dominio nem saudacao; o esquema de URL e a
// faixa [a-z0-9+.-] com letra antes de "://" — "...ftp://" e "-ftp://"
// contam, "://" solto e "123://" nao; a pontuacao arabe (، ؛ ؟) tambem e
// separador; e U+2E2F (ⸯ, letra Lm) segue letra — mas nao alonga TLD.
// O separador tokeniza palavra, nao apaga rotulo: pontuacao ou geometrico
// imediatamente a esquerda do ponto ainda conta como rotulo, e dominio
// sem esquema como "○○.com" ou "filme،.com" segue fora do texto.
static void confere_filtro_pontuacao(void) {
  static const struct { const char *s; int ok; } C[] = {
    { "Veja «exemplo.com» para saber mais.", 0 },
    { "Veja «www.exemplo.org» para saber mais.", 0 },
    { "«Hello!» Uma sugestão para hoje.", 0 },
    { "¡Hello! Uma sugestão para hoje.", 0 },
    { "¿Hello! Uma sugestão para hoje.", 0 },
    { "Veja rtsp2://servidor/filme.", 0 },
    { "Curto para hoje ▪", 0 },
    { "Veja (...ftp://servidor/filme).", 0 },
    { "Veja (-ftp://servidor/filme).", 0 },
    { "Veja (...rtsp2://servidor/filme).", 0 },
    { "候補はこちら：○exemplo.com○", 0 },
    { "候補はこちら：○www.exemplo.org○", 0 },
    { "候補はこちら：○Hello!○", 0 },
    { "شاهد هنا،www.exemplo.com، ثم اختر.", 0 },
    { "هل نجرب؟Hello! الفيلم جاهز.", 0 },
    { "Veja ○○.com/filme.", 0 },
    { "Veja ●.com/filme.", 0 },
    { "Veja filme○.com/filme.", 0 },
    { "Veja ֆילմ։.com/filme.", 0 },
    { "Veja ֆילմ֊.com/filme.", 0 },
    { "Veja filme،.com/filme.", 0 },
    { "Veja 人々.com/filme.", 0 },
    { "Veja 映画〆.com/filme.", 0 },
    { "Veja example.coⓂ/filme.", 0 },
    { "Veja ⸯexample.comⸯ agora.", 0 },
    { "Veja 例ⸯ.com/filme.", 0 },
    { "今夜の候補は○で印をつけました。", 1 },
    { "A resenha diz «imperdível».", 1 },
    { "O título é Łoi.", 1 },
    { "O título é ⸯHello.", 1 },
    { "Use apenas o sinal 123://", 1 },
    { "Veja .com/filme.", 1 },
  };
  for (size_t i = 0; i < sizeof C / sizeof *C; i++)
    if (!!faroltexto_aceita(C[i].s) != C[i].ok) {
      printf("FALHOU: filtro %s \"%s\"\n",
             C[i].ok ? "recusou" : "aceitou", C[i].s);
      falhas++;
    }
}

// ---- largura e aceitacao dos modelos do gosto nas 31 linguas -------------
//
// A cauda de FAROL_FRASES (de FT_FIL_PORQUE_TERMINOU em diante) e o lote da
// issue 35: a revisao conferiu os 122 modelos x 31 idiomas e so o grego
// passava de 62 (FT_INFO_SOBE, 71 pontos de codigo). Aqui vai a mesma regra
// em C: literal e renderizado com os valores da spec tem de caber em 2
// linhas de 62 e passar por faroltexto_aceita().

// Argumentos da spec (PLANO-GOSTO 4.2); o que nao esta aqui renderiza com
// %d=2 e %s="Duna", como a auditoria da revisao.
static const struct {
  const char *pt;
  int d[4];
  const char *s[4];
  int nd, ns;
} GOSTO_ARGS[] = {
  { FT_ACHA_CURTOS_L1,    {3, 100},    {0},                                  2, 0 },
  { FT_INFO_SOBE,         {2, 6, 45},  {0},                                  3, 0 },
  { FT_INFO_DUAS_NOITES,  {2, 50},     {0},                                  2, 0 },
  { FT_INFO_QUALI,        {0},         {"4K", "Dolby Vision", "português"},  0, 3 },
  { FT_INFO_QUALI_AUDIO,  {0},         {"4K", "português"},                  0, 2 },
  { FT_INFO_TERMINA,      {0},         {"23:40"},                            0, 1 },
  { FT_INFO_RETOMADA,     {2, 4},      {"7 semanas"},                        2, 1 },
  { FT_MOT_AMIGO,         {8},         {"Ana"},                              1, 1 },
  { FT_MOT_TERMINOU_TEMA, {0},         {"Ruptura", "mistério de escritório"}, 0, 2 },
  { FT_MOT_DIRETOR,       {2},         {"Duna"},                             1, 1 },
  { FT_DICA_PAROU_L1,     {2, 4},      {"Dark"},                             2, 1 },
  { FT_DICA_PAROU_L2,     {5},         {0},                                  1, 0 },
  { FT_DICA_SALVO_L1,     {0},         {"5 meses"},                          0, 1 },
  { FT_DICA_SALVO_L2,     {0},         {"1h32"},                             0, 1 },
  { FT_DICA_FALTA_L1,     {2, 3},      {"Dark"},                             2, 1 },
  { FT_FIL_PESSOA,        {0},         {"Frances McDormand"},                0, 1 },
  { FT_FIL_SUB_TEMA,      {0},         {"viagem no tempo"},                  0, 1 },
  { FT_ESC_PASSO,         {1, 3},      {0},                                  2, 0 },
};

// renderiza fmt com os pares (d,s) acima; %d e %s consomem na ordem
static void render_fmt(const char *fmt, const int *d, int nd,
                       const char *const *s, int ns, char *out, size_t cap) {
  size_t o = 0;
  int a = 0, b = 0;
  for (const char *p = fmt; *p && o + 1 < cap; p++) {
    if (p[0] == '%' && p[1] == 'd') {
      o += snprintf(out + o, cap - o, "%d", a < nd ? d[a] : 2);
      a++;
      p++;
      continue;
    }
    if (p[0] == '%' && p[1] == 's') {
      o += snprintf(out + o, cap - o, "%s", b < ns ? s[b] : "Duna");
      b++;
      p++;
      continue;
    }
    out[o++] = *p;
  }
  out[o] = 0;
}

static void confere_gosto_idiomas(const char *const *FRASES, size_t n,
                                  size_t gosto_ini) {
  char buf[512], nome[96];
  for (int lg = 0; lg < IDIOMA_N; lg++) {
    lg_teste = lg;
    for (size_t i = gosto_ini; i < n; i++) {
      const char *t = i18n(FRASES[i]);
      const int *d = NULL;
      const char *const *s = NULL;
      int nd = 0, ns = 0;
      snprintf(nome, sizeof nome, "%s FT[%zu]", COD[lg], i);
      confere_linhas(nome, t);
      if (!faroltexto_aceita(t)) {
        printf("FALHOU: %s literal reprovada por faroltexto_aceita: \"%s\"\n",
               nome, t);
        falhas++;
      }
      for (size_t j = 0; j < sizeof GOSTO_ARGS / sizeof *GOSTO_ARGS; j++)
        if (!strcmp(FRASES[i], GOSTO_ARGS[j].pt)) {
          d = GOSTO_ARGS[j].d;
          s = GOSTO_ARGS[j].s;
          nd = GOSTO_ARGS[j].nd;
          ns = GOSTO_ARGS[j].ns;
          break;
        }
      render_fmt(t, d, nd, s, ns, buf, sizeof buf);
      confere_linhas(nome, buf);
      if (!faroltexto_aceita(buf)) {
        printf("FALHOU: %s renderizada reprovada por faroltexto_aceita: "
               "\"%s\"\n", nome, buf);
        falhas++;
      }
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
  confere_acha_curtos_ja();
  confere_duas_noites_hu();
  confere_filtro_pontuacao();
  // a cauda de FAROL_FRASES e o lote do gosto (issue 35)
  size_t gosto_ini = n;
  for (size_t i = 0; i < n; i++)
    if (!strcmp(FRASES[i], FT_FIL_PORQUE_TERMINOU)) { gosto_ini = i; break; }
  if (gosto_ini == n) {
    printf("FALHOU: FT_FIL_PORQUE_TERMINOU fora de FAROL_FRASES\n");
    falhas++;
  } else {
    confere_gosto_idiomas(FRASES, n, gosto_ini);
  }
  confere_fonte_trocada();

  if (falhas) { printf("farol_textos: %d falha(s)\n", falhas); return 1; }
  printf("farol_textos: %zu frases ok\n", n);
  return 0;
}
