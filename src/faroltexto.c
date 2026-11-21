// faroltexto.c — o filtro do texto livre do Farol (PLANO-GOSTO 4.1, issue 35).
//
// A cena que o LLM manda pode trazer {"x": "frase pronta"}; "x" so sobe na
// tela se faroltexto_aceita() devolver 1. A regra e a mesma das falas da
// casa: UTF-8 valido, no maximo 2 linhas, cada linha com no maximo 62
// caracteres, sem URL, sem emoji e sem nenhuma palavra da lista "nunca diz"
// do ALMA 2 (em pt e em en — a mesma lista de tests/farol_textos.c).
// Reprovou, quem compoe cai no modelo "m" de reserva que o LLM manda junto.
//
// Modulo puro: sem malloc, sem rede, sem dados.h, sem SDL — so <stddef.h>.

#include "faroltexto.h"
#include <stddef.h>

// ---------------------------------------------------------- UTF-8 estrito
// Le um ponto de codigo; devolve -1 em sequencia invalida (truncada, byte
// solto, overlong, surrogate ou > U+10FFFF). Avanca *p alem do ponto lido.
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
  // overlong e limites por comprimento
  if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
  if (cp >= 0xD800 && cp <= 0xDFFF) return -1;   // surrogate
  if (cp > 0x10FFFF) return -1;
  *p += extra + 1;
  return cp;
}

// Emoji e amigos de emoji: o texto livre nao carrega nenhum destes blocos.
static int ft_emoji(long cp) {
  if (cp >= 0x1F000 && cp <= 0x1FAFF) return 1;   // pictograficos, emojis, bandeiras
  if (cp >= 0x2600 && cp <= 0x27BF) return 1;     // simbolos diversos + dingbats
  if (cp >= 0x2B00 && cp <= 0x2BFF) return 1;     // estrelas e setas ornamentais
  if (cp >= 0x2300 && cp <= 0x23FF) return 1;     // tecnico diverso (relogio, midia)
  if (cp == 0x200D || cp == 0x20E3) return 1;     // ZWJ e tecla
  if (cp >= 0xFE00 && cp <= 0xFE0F) return 1;     // seletores de variacao
  if (cp >= 0xE0020 && cp <= 0xE007F) return 1;   // tags
  return 0;
}

// ------------------------------- dobra acento -> ascii, minuscula (a mesma
// dobradura de tests/farol_textos.c, para a lista "nunca diz" e as URLs).
static void ft_dobrar(const char *in, char *out, size_t cap) {
  size_t o = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && o + 2 < cap; p++) {
    unsigned char c = *p;
    if (c == 0xC3 && p[1]) {          // U+00C0..U+00FF em UTF-8
      unsigned char d = p[1] & 0x3F;
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
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    out[o++] = (char)c;
  }
  out[o] = 0;
}

static int ft_letra(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

static const char *ft_str(const char *feno, const char *agulha) {
  for (; *feno; feno++) {
    const char *a = feno, *b = agulha;
    while (*b && *a == *b) { a++; b++; }
    if (!*b) return feno;
  }
  return NULL;
}

// agulha aparece como palavra inteira no texto ja dobrado
static int ft_tem_palavra(const char *dobrada, const char *agulha) {
  size_t n = 0;
  while (agulha[n]) n++;
  for (const char *p = dobrada; (p = ft_str(p, agulha)); p += n) {
    int esq = p == dobrada || !ft_letra(p[-1]);
    int dir = !ft_letra(p[n]);
    if (esq && dir) return 1;
  }
  return 0;
}

// Lista do ALMA 2 + os mesmos vicios em ingles — identica a PROIBIDAS do
// teste; se a lista do ALMA mudar, muda nos dois lugares.
static const char *FT_PROIBIDAS[] = {
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

// URL no texto dobrado: esquema "xxx://", "www." ou dominio com TLD comum
// ".com", ".net"... sempre com fronteira de palavra depois do TLD (para
// "na TV." e "o app." nao cairem). Aplicativos do dia a dia nao mandam link
// para a tela; link e canal de dados, nao de fala.
static int ft_tem_url(const char *d) {
  if (ft_str(d, "://") || ft_str(d, "www.")) return 1;
  static const char *TLDS[] = {
    ".com", ".net", ".org", ".br", ".io", ".tv", ".me", ".app",
    ".dev", ".info", ".xyz", ".co", ".site", ".online", ".link",
  };
  for (size_t i = 0; i < sizeof TLDS / sizeof *TLDS; i++) {
    for (const char *p = d; (p = ft_str(p, TLDS[i])); p++) {
      size_t n = 0;
      while (TLDS[i][n]) n++;
      // TLD tem que ser palavra antes (letra ou numero) e fronteira depois.
      if (p != d && ft_letra(p[-1]) && !ft_letra(p[n])) return 1;
    }
  }
  return 0;
}

int faroltexto_aceita(const char *s) {
  if (!s || !*s) return 0;

  // Estrutura: UTF-8 valido, ate 2 linhas, cada uma com ate 62 caracteres,
  // sem emoji nem controle que nao seja '\n'.
  const char *p = s;
  int linhas = 1, col = 0;
  while (*p) {
    long cp = ft_utf8(&p);
    if (cp < 0) return 0;
    if (cp == '\n') {
      if (++linhas > 2 || col == 0) return 0;   // 3a linha ou linha vazia
      col = 0;
      continue;
    }
    if (cp < 0x20 || cp == 0x7F) return 0;      // controle (tab inclusa)
    if (ft_emoji(cp)) return 0;
    if (++col > 62) return 0;
  }
  if (col == 0) return 0;                       // termina em '\n'

  // Conteudo: dobrado para ascii-minuscula, procura URL e palavra proibida.
  char d[384];
  ft_dobrar(s, d, sizeof d);
  if (ft_tem_url(d)) return 0;
  for (size_t i = 0; i < sizeof FT_PROIBIDAS / sizeof *FT_PROIBIDAS; i++)
    if (ft_tem_palavra(d, FT_PROIBIDAS[i])) return 0;
  return 1;
}
