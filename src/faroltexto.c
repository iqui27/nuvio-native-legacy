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
// ZWJ nao entra: fora de emoji ele e ortografia legitima (malayalam escreve
// com ele), e quem monta sequencia de emoji com ZWJ reprova pelo proprio
// emoji, que continua vedado.
static int ft_emoji(long cp) {
  if (cp >= 0x1F000 && cp <= 0x1FAFF) return 1;   // pictograficos, emojis, bandeiras
  if (cp >= 0x2600 && cp <= 0x27BF) return 1;     // simbolos diversos + dingbats
  if (cp >= 0x2B00 && cp <= 0x2BFF) return 1;     // estrelas e setas ornamentais
  if (cp >= 0x25A0 && cp <= 0x25FF) return 1;     // formas geometricas ("◽" e cia)
  if (cp >= 0x2300 && cp <= 0x23FF) return 1;     // tecnico diverso (relogio, midia)
  if (cp == 0x20E3) return 1;                     // tecla
  if (cp >= 0xFE00 && cp <= 0xFE0F) return 1;     // seletores de variacao
  if (cp >= 0xE0020 && cp <= 0xE007F) return 1;   // tags
  return 0;
}

// Marcas combinantes: saem da dobra, para "PARABÉNS" escrito com acento
// solto (NFD) comparar igual a "PARABÉNS" pre-composto (NFC).
static int ft_combinante(long cp) {
  return (cp >= 0x0300 && cp <= 0x036F) ||    // diacriticas
         (cp >= 0x1AB0 && cp <= 0x1AFF) ||    // estendidas
         (cp >= 0x20D0 && cp <= 0x20FF) ||    // para simbolos
         (cp >= 0xFE20 && cp <= 0xFE2F);      // meias marcas
}

// Brancos e pontuacao larga que a dobra trata como separador: viram um ' '
// so, para fronteira de palavra e para a frase proibida nao depender de
// quantos espacos (ou de qual linha) separam os termos.
static int ft_branco(long cp) {
  return cp == ' ' || cp == '\n' || cp == 0xA0 ||
         cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x3000;
}

static int ft_pontuacao(long cp) {
  return (cp >= 0x2000 && cp <= 0x206F) ||    // pontuacao geral (— … ‐ ZWSP)
         (cp >= 0x3001 && cp <= 0x303F) ||    // pontuacao CJK (、。〈〉「」)
         (cp >= 0xFE30 && cp <= 0xFE4F) ||    // pontuacao CJK de apresentacao
         (cp >= 0xFF61 && cp <= 0xFF65);      // pontuacao halfwidth
}

// Base ascii de um latim-1 (U+00C0..U+00FF); 0 quando o ponto nao tem base
// (Æ Ð × Ø Þ ß æ ð ÷ ø þ) — esses seguem a regra geral e ficam nos bytes
// crus, pesando como letra na fronteira de palavra.
static char ft_base_latim(long cp) {
  static const char T[0x40] = {
    'a','a','a','a','a','a', 0 ,'c',  // À Á Â Ã Ä Å Æ Ç
    'e','e','e','e','i','i','i','i',  // È É Ê Ë Ì Í Î Ï
     0 ,'n','o','o','o','o','o', 0 ,  // Ð Ñ Ò Ó Ô Õ Ö ×
     0 ,'u','u','u','u','y', 0 , 0 ,  // Ø Ù Ú Û Ü Ý Þ ß
    'a','a','a','a','a','a', 0 ,'c',  // à á â ã ä å æ ç
    'e','e','e','e','i','i','i','i',  // è é ê ë ì í î ï
     0 ,'n','o','o','o','o','o', 0 ,  // ð ñ ò ó ô õ ö ÷
     0 ,'u','u','u','u','y', 0 ,'y',  // ø ù ú û ü ý þ ÿ
  };
  return (cp >= 0xC0 && cp <= 0xFF) ? T[cp - 0xC0] : 0;
}

// ------------------------------- dobra acento -> ascii, minuscula (a mesma
// dobradura de tests/farol_textos.c, para a lista "nunca diz" e as URLs).
// A entrada ja passou pela validacao de estrutura, entao tem no maximo 2
// linhas de 62 pontos de codigo (497 bytes antes do NUL): 512 de saida
// examina o texto INTEIRO — nada fica atras de um corte cedo.
static void ft_dobrar(const char *in, char *out, size_t cap) {
  size_t o = 0;
  int pend = 0;                            // separador ainda nao emitido
  const char *p = in;
  while (*p && o + 5 < cap) {
    const char *ini = p;
    long cp = ft_utf8(&p);
    if (cp < 0) break;                     // nao acontece: ja foi validada
    if (ft_branco(cp) || ft_pontuacao(cp)) { if (o) pend = 1; continue; }
    if (ft_combinante(cp)) continue;
    if (pend) { out[o++] = ' '; pend = 0; }
    if (cp < 0x80) {
      char c = (char)cp;
      out[o++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
      continue;
    }
    char base = ft_base_latim(cp);
    if (base) { out[o++] = base; continue; }
    while (ini < p && o + 1 < cap) out[o++] = *ini++;   // cru: pesa como letra
  }
  out[o] = 0;
}

// Letra para a fronteira de palavra e de TLD: ascii a-z/0-9 ou byte >= 0x80
// (todo ponto nao ascii que chega cru a dobra — Ł, þ, ideograma — e letra).
// E o que impede "Łoi" de virar "oi" solto e "o×i" de virar "oi" ao apagar
// o sinal — e o que deixa ".com" reconhecer o rotulo nao ascii de 电影.com.
static int ft_letra(int c) {
  unsigned char u = (unsigned char)c;
  return (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9') || u >= 0x80;
}

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
  // jargao cru na primeira linha (ALMA 2: nada de MEDIA_ERR_DECODE,
  // "HTTP 206 Partial Content", HEVC Main10 DV P5 ou ring buffer)
  "http", "https", "media_err_decode", "hevc", "main10", "dv p5",
  "partial content", "ring buffer", "mediacodec", "dns", "tls",
  // culpa ao usuario (ALMA 2: nada de "sua internet esta ruim" nem
  // "voce escolheu uma fonte incompativel" — o fato neutro, sim)
  "internet esta ruim", "internet caiu", "fonte incompativel",
};

// URL no texto dobrado: esquema "letra(s)://", "www." abrindo palavra ou
// dominio com TLD — sempre com letra antes e fronteira depois, para
// "na TV." e "o app." nao cairem; "awww.tmp" e " ://" solto tambem nao
// caem, mas "ard.de" e "nü.de" caem pelo ccTLD de 2 letras e "电影.com"
// cai porque byte >= 0x80 conta como letra de rotulo. Aplicativos do dia
// a dia nao mandam link para a tela; link e canal de dados, nao de fala.
static int ft_tem_url(const char *d) {
  static const char *TLDS[] = {
    ".com", ".net", ".org", ".br", ".io", ".tv", ".me", ".app",
    ".dev", ".info", ".xyz", ".co", ".site", ".online", ".link",
  };
  for (const char *p = d; (p = ft_str(p, "://")); p += 3)
    if (p != d && p[-1] >= 'a' && p[-1] <= 'z') return 1;
  for (const char *p = d; (p = ft_str(p, "www.")); p += 4)
    if (p == d || !ft_letra(p[-1])) return 1;
  for (const char *p = d; (p = ft_str(p, ".")); p++) {
    if (p == d || !ft_letra(p[-1])) continue;    // ponto sem rotulo antes
    for (size_t i = 0; i < sizeof TLDS / sizeof *TLDS; i++) {
      const char *t = TLDS[i];
      size_t n = 0;
      int bate = 1;
      while (t[n]) n++;
      for (size_t j = 0; j < n; j++)
        if (p[j] != t[j]) { bate = 0; break; }
      if (bate && !ft_letra(p[n])) return 1;
    }
    // ccTLD: "." + 2 letras ascii com fronteira depois (ard.de, nü.de)
    if (p[1] >= 'a' && p[1] <= 'z' && p[2] >= 'a' && p[2] <= 'z' &&
        !ft_letra(p[3])) return 1;
  }
  return 0;
}

int faroltexto_aceita(const char *s) {
  if (!s || !*s) return 0;

  // Estrutura: UTF-8 valido, ate 2 linhas, cada uma com ate 62 caracteres e
  // ao menos um caractere que nao seja branco, sem emoji nem controle que
  // nao seja quebra de linha ('\n', U+2028, U+2029).
  const char *p = s;
  int linhas = 1, col = 0, vis = 0;
  while (*p) {
    long cp = ft_utf8(&p);
    if (cp < 0) return 0;
    if (cp == '\n' || cp == 0x2028 || cp == 0x2029) {
      if (++linhas > 2 || vis == 0) return 0;   // 3a linha ou linha vazia
      col = 0;
      vis = 0;
      continue;
    }
    if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return 0;  // C0, DEL e C1
    if (ft_emoji(cp)) return 0;
    if (++col > 62) return 0;
    if (cp != ' ' && cp != 0xA0) vis++;
  }
  if (vis == 0) return 0;                       // termina em '\n' ou so branco

  // Conteudo: dobrado para ascii-minuscula, procura URL e palavra proibida.
  char d[512];
  ft_dobrar(s, d, sizeof d);
  if (ft_tem_url(d)) return 0;
  for (size_t i = 0; i < sizeof FT_PROIBIDAS / sizeof *FT_PROIBIDAS; i++)
    if (ft_tem_palavra(d, FT_PROIBIDAS[i])) return 0;
  return 1;
}
