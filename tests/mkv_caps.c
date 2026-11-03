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

static const MkvSeg *achaSeg(const MkvSeg *s, int n, int tipo) {
  int i;
  for (i = 0; i < n; i++) if (s[i].tipo == tipo) return &s[i];
  return NULL;
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
// Varios ChapterDisplay (um por idioma), FlagHidden/FlagEnabled (feat/mkv-chapters-all-platforms).
static void display(const char *nome, const char *lang, const char *bcp) {
  long d; id1(0x80); d = tam4(); id1(0x85); str(nome);
  if (lang) { id2(0x437C); str(lang); }
  if (bcp) { id2(0x437D); str(bcp); }
  fecha(d);
}
static void atomo(unsigned long long seg, int hidden, int enabled, const char *a, const char *la,
                  const char *b, const char *lb) {
  long t; id1(0xB6); t = tam4(); id1(0x91); uint8b(seg * 1000000000ULL);
  if (hidden >= 0) { unsigned char h[3] = {0x98, 0x81, (unsigned char)hidden}; put(h, 3); }
  if (enabled >= 0) { unsigned char e[4] = {0x45, 0x98, 0x81, (unsigned char)enabled}; put(e, 4); }
  if (a) display(a, la, NULL);
  if (b) display(b, lb, NULL);
  fecha(t);
}

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

  // 5c. 2.0.3, ANIME: "Opening", "Part A", "Part B", "Ending", "Preview". O ED
  //     e o inicio dos creditos; a previa (ultimo capitulo) nao e.
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, "Opening");
      capitulo(90ULL*1000000000ULL, "Part A");
      capitulo(700ULL*1000000000ULL, "Part B");
      capitulo(1320ULL*1000000000ULL, "Ending");
      capitulo(1410ULL*1000000000ULL, "Preview");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 5 && mkv_creditos_nomeados(caps, k) > 1319.0 && mkv_creditos_nomeados(caps, k) < 1321.0,
     "anime: \"Ending\" e os creditos");
  ok(k == 5 && mkv_creditos_ultimo(caps, k) > 1319.0 && mkv_creditos_ultimo(caps, k) < 1321.0,
     "anime: o ultimo capitulo util pula a previa");
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, "OP");
      capitulo(600ULL*1000000000ULL, "Bed Scene");
      capitulo(1330ULL*1000000000ULL, "ED");
      capitulo(1420ULL*1000000000ULL, "Next Episode");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 4 && mkv_creditos_nomeados(caps, k) > 1329.0 && mkv_creditos_nomeados(caps, k) < 1331.0,
     "anime: \"ED\" casa como palavra (\"Bed\" nao)");
  // So "Opening Credits": e abertura, nao o fim.
  comeca();
  { long c = abreChapters(); long e;
    id2(0x45B9UL); e = tam4();
      capitulo(0ULL, "Cold Open");
      capitulo(90ULL*1000000000ULL, "Opening Credits");
      capitulo(3000ULL*1000000000ULL, "Act II");
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3 && mkv_creditos_nomeados(caps, k) == 0.0, "\"Opening Credits\" sozinho nao e o fim");

  // 6. Lixo: nao pode travar nem devolver numero absurdo.
  comeca();
  { int i; for (i = 0; i < 64; i++) { unsigned char b = (unsigned char)(i * 7); put(&b, 1); } }
  k = lerCapitulos(buf, n, caps, MKV_MAX_CAPS);
  ok(k >= 0 && k <= MKV_MAX_CAPS, "lixo nao trava nem estoura");

  // 7. SEGMENTOS (feat/mkv-chapter-segments): regras do NuvioTV + nomes extra.
  printf("segmentos dos capitulos (NuvioTV + extra)\n");
  { MkvSeg s[6]; int ns;
#define CAPS(...) MkvCap c[] = { __VA_ARGS__ }; int nc = (int)(sizeof c / sizeof c[0])
#define ACHA(tp) achaSeg(s, ns, tp)
    // Classificacao de nomes soltos.
    int f;
    ok(mkv_classificar_nome("OP", &f) == MKV_SEG_ABERTURA && f == 3, "\"OP\" estrito");
    ok(mkv_classificar_nome("OP 2", &f) == MKV_SEG_ABERTURA && f == 3, "\"OP 2\" estrito");
    ok(mkv_classificar_nome("Opening Credits", &f) == MKV_SEG_ABERTURA && f == 3, "\"Opening Credits\" e abertura");
    ok(mkv_classificar_nome("Ending Theme", &f) == MKV_SEG_CREDITOS && f == 3, "\"Ending Theme\" estrito");
    ok(mkv_classificar_nome("End Credits", &f) == MKV_SEG_CREDITOS && f == 3, "\"End Credits\" estrito");
    ok(mkv_classificar_nome("Previously on Silo", &f) == MKV_SEG_RESUMO && f == 3, "\"Previously on ...\" e resumo");
    ok(mkv_classificar_nome("Recap", &f) == MKV_SEG_RESUMO, "\"Recap\" e resumo");
    ok(mkv_classificar_nome("Post-Credits", &f) == MKV_SEG_POSCRED, "\"Post-Credits\" e pos-creditos");
    ok(mkv_classificar_nome("Mid Credits Scene", &f) == MKV_SEG_POSCRED, "\"Mid Credits Scene\" e pos-creditos");
    ok(mkv_classificar_nome("The Opening Scene", &f) == MKV_SEG_ABERTURA && f == 1,
       "\"The Opening Scene\" nao e estrito (so extra)");
    ok(mkv_classificar_nome("Alternate Ending", &f) == MKV_SEG_CREDITOS && f == 1,
       "\"Alternate Ending\" nao e estrito (so extra)");
    ok(mkv_classificar_nome("Cr\xc3\xa9" "ditos", &f) == MKV_SEG_CREDITOS && f == 1, "\"Creditos\" (extra)");
    ok(mkv_classificar_nome("Yokoku", &f) == MKV_SEG_PREVIA, "\"Yokoku\" e previa");
    ok(mkv_classificar_nome("Part A", &f) == MKV_SEG_NENHUM, "\"Part A\" nada");

    { CAPS({0, "Prologue"}, {90, "OP"}, {180, "Part A"}, {1260, "ED"}, {1350, "Epilogue"});
      ns = mkv_segmentos(c, nc, 1440, s, 6);
      ok(ns == 2 && ACHA(MKV_SEG_CREDITOS) && ACHA(MKV_SEG_CREDITOS)->fim == 1350,
         "anime: ED acaba no capitulo seguinte (cena depois nao some)"); }
    { CAPS({0, "Prologue"}, {90, "OP"}, {180, "Part A"}, {1260, "ED"}, {1350, "Post-Credits"});
      ns = mkv_segmentos(c, nc, 1440, s, 6);
      ok(ACHA(MKV_SEG_POSCRED) && ACHA(MKV_SEG_POSCRED)->inicio == 1350 &&
         ACHA(MKV_SEG_CREDITOS)->fim == 1350, "pos-creditos marcada depois do ED"); }
    { CAPS({0, "Recap"}, {60, "Intro"}, {150, "Part A"}, {1300, "Credits"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(ACHA(MKV_SEG_RESUMO) && ACHA(MKV_SEG_ABERTURA) && ACHA(MKV_SEG_ABERTURA)->inicio == 60,
         "\"Intro\" vira abertura sem OP; resumo lido"); }
    { CAPS({0, "Intro"}, {60, "OP"}, {150, "Part A"}, {1300, "Credits"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(ACHA(MKV_SEG_ABERTURA) && ACHA(MKV_SEG_ABERTURA)->inicio == 60, "com OP, \"Intro\" nao conta"); }
    { CAPS({0, "The Opening Scene"}, {200, "Opening"}, {290, "Part A"}, {1300, "ED"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(ACHA(MKV_SEG_ABERTURA) && ACHA(MKV_SEG_ABERTURA)->inicio == 200, "estrito vence o extra do mesmo tipo"); }
    { CAPS({0, "Prologo"}, {60, "Abertura"}, {150, "Parte A"}, {1300, "Encerramento"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(ACHA(MKV_SEG_ABERTURA) && ACHA(MKV_SEG_CREDITOS) && ACHA(MKV_SEG_CREDITOS)->inicio == 1300,
         "nomes em portugues/espanhol continuam valendo"); }
    { CAPS({0, "Part A"}, {3, "OP"}, {6, "Part B"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(!ACHA(MKV_SEG_ABERTURA), "abertura de 3 s descartada (< 5 s)"); }
    { CAPS({0, "Part A"}, {60, "OP"}, {600, "Part B"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(!ACHA(MKV_SEG_ABERTURA), "abertura de 9 min descartada (> 240 s)"); }
    { CAPS({0, "Movie"}, {6000, "End Credits"}, {6600, "Post-Credits Scene"});
      ns = mkv_segmentos(c, nc, 6700, s, 6);
      ok(ACHA(MKV_SEG_CREDITOS) && ACHA(MKV_SEG_CREDITOS)->fim == 6600 && ACHA(MKV_SEG_POSCRED),
         "filme: creditos de 10 min aceitos e param na cena"); }
    { CAPS({0, "Chapter 1"}, {600, "Chapter 2"}, {1200, "Chapter 3"});
      ns = mkv_segmentos(c, nc, 1400, s, 6);
      ok(ns == 0, "capitulos sem nome util: nenhum segmento"); }
#undef CAPS
#undef ACHA
  }


  // --- feat/mkv-chapters-all-platforms: como o NuvioTV le os capitulos ---------
  printf("capitulos: idioma do nome, escondidos, edicao padrao, ordem\n");
  // Ingles + russo no mesmo capitulo (o Fight Club da C9): antes ficava o
  // ULTIMO display, o russo.
#define RU_OP "\xd0\x92\xd1\x81\xd1\x82\xd1\x83\xd0\xbf\xd0\xbb\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5"
  comeca();
  { long c = abreChapters(), e; id2(0x45B9UL); e = tam4();
    atomo(0, -1, -1, "Opening", "eng", RU_OP, "rus");
    atomo(90, -1, -1, RU_OP, "rus", "Main", "eng");
    atomo(600, -1, -1, "Huvuddel", "swe", "Main", "eng");
    fecha(e); fecha(c); }
  mkv_capitulos_idioma("en");
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3 && !strcmp(caps[0].nome, "Opening") && !strcmp(caps[1].nome, "Main") &&
     !strcmp(caps[2].nome, "Main"), "app em ingles: nome ingles (era o ultimo)");
  mkv_capitulos_idioma("sv");
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3 && !strcmp(caps[0].nome, "Opening") && !strcmp(caps[2].nome, "Huvuddel"),
     "app em sueco: sueco quando ha, senao ingles");
  mkv_capitulos_idioma("ru");
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3 && !strcmp(caps[0].nome, RU_OP) && !strcmp(caps[2].nome, "Main"),
     "app em russo: russo quando ha");
  mkv_capitulos_idioma("en");
  // So russo: fica o russo (nao ha outro).
  comeca();
  { long c = abreChapters(), e; id2(0x45B9UL); e = tam4();
    atomo(0, -1, -1, RU_OP, "rus", NULL, NULL); atomo(90, -1, -1, "Part", NULL, NULL, NULL);
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2 && !strcmp(caps[0].nome, RU_OP) && !strcmp(caps[1].nome, "Part"), "so um idioma: ele mesmo");
#undef RU_OP
  // Escondido (FlagHidden=1) e desligado (FlagEnabled=0) ficam de fora.
  comeca();
  { long c = abreChapters(), e; id2(0x45B9UL); e = tam4();
    atomo(0, -1, -1, "Part A", NULL, NULL, NULL); atomo(60, 1, -1, "Hidden", NULL, NULL, NULL);
    atomo(120, -1, 0, "Disabled", NULL, NULL, NULL); atomo(180, 0, 1, "Part B", NULL, NULL, NULL);
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2 && !strcmp(caps[0].nome, "Part A") && !strcmp(caps[1].nome, "Part B"),
     "escondido e desligado ficam de fora");
  // Duas edicoes: vale a marcada como padrao; edicao escondida nunca.
  comeca();
  { long c = abreChapters(), e;
    id2(0x45B9UL); e = tam4(); atomo(0, -1, -1, "Ed1 A", NULL, NULL, NULL); atomo(60, -1, -1, "Ed1 B", NULL, NULL, NULL); fecha(e);
    id2(0x45B9UL); e = tam4(); { unsigned char d[4] = {0x45, 0xDB, 0x81, 1}; put(d, 4); }
      atomo(0, -1, -1, "Ed2 A", NULL, NULL, NULL); atomo(60, -1, -1, "Ed2 B", NULL, NULL, NULL); fecha(e);
    fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2 && !strcmp(caps[0].nome, "Ed2 A"), "edicao padrao vence a primeira");
  comeca();
  { long c = abreChapters(), e;
    id2(0x45B9UL); e = tam4(); { unsigned char h[4] = {0x45, 0xBD, 0x81, 1}; put(h, 4); }
      atomo(0, -1, -1, "Oculta A", NULL, NULL, NULL); atomo(60, -1, -1, "Oculta B", NULL, NULL, NULL); fecha(e);
    id2(0x45B9UL); e = tam4(); atomo(0, -1, -1, "Vista A", NULL, NULL, NULL); atomo(60, -1, -1, "Vista B", NULL, NULL, NULL); fecha(e);
    fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 2 && !strcmp(caps[0].nome, "Vista A"), "edicao escondida e pulada");
  // Fora de ordem e inicio repetido: ordenado, um por inicio.
  comeca();
  { long c = abreChapters(), e; id2(0x45B9UL); e = tam4();
    atomo(300, -1, -1, "C", NULL, NULL, NULL); atomo(0, -1, -1, "A", NULL, NULL, NULL);
    atomo(120, -1, -1, "B", NULL, NULL, NULL); atomo(120, -1, -1, "B2", NULL, NULL, NULL);
    fecha(e); fecha(c); }
  k = lerCapitulos(buf + 8, n - 8, caps, MKV_MAX_CAPS);
  ok(k == 3 && caps[0].inicio == 0 && caps[1].inicio == 120 && caps[2].inicio == 300 &&
     !strcmp(caps[1].nome, "B"), "ordenado por inicio, sem inicio repetido");

  // --- PV, "Next time" e palpite por duracao (teste na C9) ---------------------
  { MkvCap c[8]; MkvSeg s2[6]; int ns2;
    memset(c, 0, sizeof c);
    c[0].inicio = 0;    snprintf(c[0].nome, sizeof c[0].nome, "Part A");
    c[1].inicio = 1300; snprintf(c[1].nome, sizeof c[1].nome, "ED");
    c[2].inicio = 1390; snprintf(c[2].nome, sizeof c[2].nome, "PV");
    ns2 = mkv_segmentos(c, 3, 1420, s2, 6);
    ok(ns2 == 2 && s2[1].tipo == MKV_SEG_PREVIA && s2[1].inicio == 1390, "\"PV\" no fim e previa");
    snprintf(c[2].nome, sizeof c[2].nome, "Next Time");
    ns2 = mkv_segmentos(c, 3, 1420, s2, 6);
    ok(ns2 == 2 && s2[1].tipo == MKV_SEG_PREVIA, "\"Next Time\" no fim e previa");
    ok(mkv_classificar_nome("UPVOTE", NULL) == MKV_SEG_NENHUM, "\"PV\" so como palavra");
    // Attack on Titan da C9: "Chapter 1".."Chapter 5", sem nome util.
    for (k = 0; k < 5; k++) snprintf(c[k].nome, sizeof c[k].nome, "Chapter %d", k + 1);
    c[0].inicio = 0; c[1].inicio = 10; c[2].inicio = 102; c[3].inicio = 1320; c[4].inicio = 1410;
    ns2 = mkv_segmentos_palpite(c, 5, s2, 6);
    ok(ns2 == 2 && s2[0].tipo == MKV_SEG_ABERTURA && s2[0].inicio == 10 && s2[0].fim == 102 &&
       s2[1].tipo == MKV_SEG_CREDITOS && s2[1].inicio == 1320 && s2[1].fim == 1410,
       "palpite: OP 10-102 s e ED 1320-1410 s pela duracao");
    snprintf(c[3].nome, sizeof c[3].nome, "Ending");
    ok(mkv_segmentos_palpite(c, 5, s2, 6) == 0, "palpite so sem nenhum nome util");
    c[0].inicio = 0; c[1].inicio = 600; c[2].inicio = 1200;
    for (k = 0; k < 3; k++) snprintf(c[k].nome, sizeof c[k].nome, "Chapter %d", k + 1);
    ok(mkv_segmentos_palpite(c, 3, s2, 6) == 0, "palpite: capitulos de 10 min nao viram nada"); }

  printf(falhas ? "\nFALHOU\n" : "\nmkv capitulos: PASS (nanossegundos, nome, acento, sem nome, lixo)\n");
  return falhas ? 1 : 0;
}
