// Capitulos do Matroska: o parser e EBML escrito a mao e todo erro dele e
// SILENCIOSO — um deslocamento de um byte devolve zero capitulos, ou pior, um
// tempo mil vezes errado sem parecer errado. Este teste monta arquivos em
// memoria com a arvore certa e confere o que sai.
#include "../src/mkv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// mkv.c le por rede; aqui exercitamos o parser pelo caminho publico, entao o
// teste monta o buffer e chama a funcao interna via include do fonte.
#define main mkv_main_nao_usado
#include "../src/mkv.c"
#undef main

static int falhas;
static void ok(int cond, const char *o) {
  printf("  %-46s %s\n", o, cond ? "ok" : "FALHOU");
  if (!cond) falhas++;
}

// --- montagem de EBML -------------------------------------------------------
static unsigned char buf[4096];
static long n;

static void put(const unsigned char *b, long k) { memcpy(buf + n, b, k); n += k; }
static void id1(unsigned char v) { put(&v, 1); }
static void id2(unsigned long v) { unsigned char b[2]={(unsigned char)(v>>8),(unsigned char)v}; put(b,2); }
static void id4(unsigned long v) {
  unsigned char b[4]={(unsigned char)(v>>24),(unsigned char)(v>>16),
                      (unsigned char)(v>>8),(unsigned char)v};
  put(b,4);
}
// Tamanho em 4 bytes: marcador 0x10 no primeiro, para nao ter que calcular
// larguras diferentes a cada remendo.
static long tam4(void) { long p = n; unsigned char b[4]={0x10,0,0,0}; put(b,4); return p; }
static void fecha(long p) {
  long v = n - p - 4;
  buf[p]   = (unsigned char)(0x10 | ((v >> 24) & 0x0F));
  buf[p+1] = (unsigned char)(v >> 16);
  buf[p+2] = (unsigned char)(v >> 8);
  buf[p+3] = (unsigned char)v;
}
static void uint8b(unsigned long long v) {
  unsigned char b[9]; int i;
  b[0] = 0x88;                       // tamanho 8
  for (i = 0; i < 8; i++) b[1+i] = (unsigned char)(v >> (56 - 8*i));
  put(b, 9);
}
static void str(const char *s) {
  size_t k = strlen(s);
  unsigned char t = (unsigned char)(0x80 | k);   // cabe em 1 byte ate 127
  put(&t, 1); put((const unsigned char *)s, (long)k);
}

// Um capitulo: ChapterAtom { ChapterTimeStart, ChapterDisplay { ChapString } }
static void capitulo(unsigned long long ns, const char *nome) {
  long a;
  id1(0xB6); a = tam4();
    id1(0x91); uint8b(ns);
    if (nome) { long d; id1(0x80); d = tam4(); id1(0x85); str(nome); fecha(d); }
  fecha(a);
}

static void comeca(void) { n = 0; }
static long abreChapters(void) { long c; id4(0x1043A770UL); c = tam4(); return c; }

int main(void) {
  MkvCap caps[MKV_MAX_CAPS];
  int k;

  printf("capitulos do matroska\n");

  // 1. Tres capitulos, o ultimo nomeado "End Credits".
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL,              "Opening");
      capitulo(1800ULL*1000000000ULL, "Act Two");
      capitulo(5820ULL*1000000000ULL, "End Credits");
    fecha(e); fecha(c); }
  // Pula o ID e o tamanho do Chapters para entregar o conteudo, como faz
  // acharTracks quando encontra o elemento.
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3, "tres capitulos lidos");
  ok(k == 3 && caps[0].inicio == 0.0, "primeiro comeca em 0s");
  // NANOSSEGUNDOS, nao unidades de TimecodeScale: 5820s e 1h37, nao 5,8s.
  ok(k == 3 && caps[2].inicio > 5819.0 && caps[2].inicio < 5821.0,
     "ultimo em 5820s (nanossegundos, nao escala)");
  ok(k == 3 && !strcmp(caps[2].nome, "End Credits"), "nome do ultimo lido");

  // 2. acharCreditos pelo NOME, mesmo nao sendo o ultimo.
  ok(mkv_creditos_nomeados(caps, k) > 5819.0, "creditos achados pelo nome");

  // 3. Nome em portugues e com acento.
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, "Abertura");
      capitulo(4200ULL*1000000000ULL, "Créditos finais");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2 && mkv_creditos_nomeados(caps, k) > 4199.0, "creditos em portugues com acento");

  // 4. Capitulos SEM nome: acharCreditos nao chuta — quem decide pela posicao
  //    e video_creditos(), que conhece a duracao.
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, NULL);
      capitulo(600ULL*1000000000ULL, NULL);
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2, "capitulos sem nome ainda sao lidos");
  ok(mkv_creditos_nomeados(caps, k) == 0.0, "sem nome, nao inventa creditos");

  // 5. Nome que CONTEM a palavra mas nao e o capitulo (ex.: "Credits Roll" e,
  //    mas "Discredited" tambem casaria com 'credit' — registra o limite).
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(120ULL*1000000000ULL, "Credits Roll");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 1 && mkv_creditos_nomeados(caps, k) > 119.0, "\"Credits Roll\" casa");

  // 5b. #115: "Opening Credits" no comeco E "End Credits" no fim. Vale o
  //     ULTIMO: o primeiro punha o painel de relacionados aos 90 s de filme.
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, "Prologue");
      capitulo(90ULL*1000000000ULL, "Opening Credits");
      capitulo(3000ULL*1000000000ULL, "Act II");
      capitulo(6900ULL*1000000000ULL, "End Credits");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 4 && mkv_creditos_nomeados(caps, k) > 6899.0,
     "abertura e final nomeados: vale o final");

  // 6. Lixo: nao pode travar nem devolver numero absurdo.
  comeca();
  { int i; for (i = 0; i < 64; i++) { unsigned char b = (unsigned char)(i * 7); put(&b, 1); } }
  k = lerCapitulos(buf, n, caps, MKV_MAX_CAPS);
  ok(k >= 0 && k <= MKV_MAX_CAPS, "lixo nao trava nem estoura");

  printf(falhas ? "\nFALHOU\n" : "\nmkv capitulos: PASS (nanossegundos, nome, acento, sem nome, lixo)\n");
  return falhas ? 1 : 0;
}
