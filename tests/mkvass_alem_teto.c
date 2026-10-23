// #384: legenda ASS embutida que SOME depois da abertura do anime.
//
// No registro do LG C5 (Nuvio 2.0.2) a faixa tinha "8000 blocos indexados", o
// mkvass fechou "completo: 8000/8000" e o libass recebeu um documento de 8000
// eventos com cobertura de 7 s a 217 s: o karaoke e os letreiros da abertura
// gastaram o teto de 8000 pontos do indice, e dali ate o fim do episodio nao
// havia fala nenhuma. O sidecar ainda guardou isso como "completo".
//
// Este teste monta um MKV com MAIS de 8000 blocos de legenda — abertura densa
// (8400 em 210 s), dialogo (um a cada 2 s) e encerramento denso — e serve
// Range por um transporte local, sem socket. O assrender e um dublê: guarda o
// documento que o libass receberia, e e nele que as provas olham.
//
//   ./test padrao    tetos de producao: o indice cobre a faixa inteira, mas
//                    SO a janela do playhead e colhida (o #384 nao pode virar
//                    o #308: nada de milhares de Ranges em segundo plano); ha
//                    fala depois do bloco 8000 quando o playhead chega la;
//                    nunca "completo"; sidecar parcial; e um sidecar
//                    "completo" truncado da 2.0.2 nao e aceito na reabertura.
//   ./test janela    corpo maior que o teto (compilado com um teto pequeno):
//                    o corpo fica so com a janela do playhead, sem perder nem
//                    repetir fala, indo e voltando por seek.
//   ./test ritmo     o teto de Ranges de producao (3 por segundo): nunca mais
//                    que isso em 1 s, nunca dois pedidos ao mesmo tempo, nem
//                    depois de seek; e o freio do CDN (curl 28) ainda pausa a
//                    leitura em vez de insistir.
#include "mkvass.h"
#include "legenda.h"
#include "assrender.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef MKVASS_CORPO_MAX
#define MKVASS_CORPO_MAX (16L * 1024 * 1024)
#endif

#define URL "https://example.test/anime-typeset.mkv"

// --- o arquivo ---------------------------------------------------------------

typedef struct { unsigned char *b; long n, cap; } Buf;
static void bytes(Buf *b, const void *p, long n) {
  if (b->n + n > b->cap) {
    long nc = b->cap ? b->cap : 256;
    while (nc < b->n + n) nc *= 2;
    b->b = realloc(b->b, (size_t)nc); assert(b->b); b->cap = nc;
  }
  memcpy(b->b + b->n, p, (size_t)n); b->n += n;
}
static void id(Buf *b, unsigned long v) {
  unsigned char t[4]; int n = v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1, i;
  for (i = 0; i < n; i++) t[i] = (unsigned char)(v >> (8 * (n - 1 - i)));
  bytes(b, t, n);
}
static long idLen(unsigned long v) { return v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1; }
static void tam8(Buf *b, long long v) {
  unsigned char t[8]; int i;
  t[0] = 0x01;
  for (i = 1; i < 8; i++) t[i] = (unsigned char)(v >> (8 * (7 - i)));
  bytes(b, t, 8);
}
static void uintEl(Buf *b, unsigned long eid, unsigned long long v, int n) {
  unsigned char t[8]; int i;
  id(b, eid); { unsigned char s = (unsigned char)(0x80 | n); bytes(b, &s, 1); }
  for (i = 0; i < n; i++) t[i] = (unsigned char)(v >> (8 * (n - 1 - i)));
  bytes(b, t, n);
}
static void strEl(Buf *b, unsigned long eid, const char *s) {
  unsigned char z = (unsigned char)(0x80 | strlen(s));
  id(b, eid); bytes(b, &z, 1); bytes(b, s, (long)strlen(s));
}
static void mestre(Buf *b, unsigned long eid, const Buf *corpo) {
  id(b, eid); tam8(b, corpo->n); bytes(b, corpo->b, corpo->n);
}
static void binEl(Buf *b, unsigned long eid, const void *p, long n) {
  id(b, eid); tam8(b, n); bytes(b, p, n);
}
static void seekEl(Buf *sh, const unsigned char alvo[4], long long pos) {
  Buf s = {0};
  id(&s, 0x53AB); { unsigned char z = 0x84; bytes(&s, &z, 1); bytes(&s, alvo, 4); }
  uintEl(&s, 0x53AC, (unsigned long long)pos, 8);
  mestre(sh, 0x4DBB, &s); free(s.b);
}

// Abertura 0-210 s: 40 eventos por segundo (10 no mesmo instante a cada
// 250 ms, como letreiro quadro a quadro) = 8400, mais que o teto antigo.
// Um segundo em cada 16 tem os 40 eventos a 10 ms um do outro: sem folga
// entre eles, e onde o corte da janela nao pode cair.
// Dialogo 210-1300 s: uma fala a cada 2 s. Encerramento 1300-1390 s: denso.
#define ABERTURA_FIM   210
#define DIALOGO_FIM    1300
#define FIM            1390
#define MAX_EV         16000
static long evMs[MAX_EV];          // inicio de cada evento, em ms
static int  nEv, evAposTeto = -1;  // primeiro evento do dialogo (depois do 8000o)
static unsigned char *arq; static long arqN;
static int pad;                    // bytes de enchimento por evento

static void montar(void) {
  Buf f = {0}, ebml = {0}, info = {0}, seekHead = {0}, tracks = {0}, v = {0}, s = {0};
  Buf cues = {0}, lista = {0}, clusters = {0};
  unsigned char dur[8]; double d = FIM * 1000.0; unsigned long long u; int i, c;
  static const unsigned char TR[4] = { 0x16, 0x54, 0xAE, 0x6B }, CU[4] = { 0x1C, 0x53, 0xBB, 0x6B };
  long segIniL, lenPos, trAbs, cuAbs, cluAbs;
  char *enche = malloc((size_t)pad + 1);
  const char *hdr = "[Script Info]\r\nScriptType: v4.00+\r\nPlayResX: 1920\r\nPlayResY: 1080\r\n\r\n[V4+ Styles]\r\n"
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\r\n"
    "Style: Default,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1\r\n\r\n"
    "[Events]\r\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n";
  assert(enche);
  memset(enche, 'x', (size_t)pad); enche[pad] = 0;

  uintEl(&ebml, 0x4286, 1, 1); strEl(&ebml, 0x4282, "matroska");
  mestre(&f, 0x1A45DFA3UL, &ebml);
  id(&f, 0x18538067UL); lenPos = f.n; tam8(&f, 0); segIniL = f.n;
  uintEl(&info, 0x2AD7B1, 1000000, 4);
  memcpy(&u, &d, 8);
  for (i = 0; i < 8; i++) dur[i] = (unsigned char)(u >> (8 * (7 - i)));
  id(&info, 0x4489); { unsigned char z = 0x88; bytes(&info, &z, 1); bytes(&info, dur, 8); }
  seekEl(&seekHead, TR, 0); seekEl(&seekHead, CU, 0);
  trAbs = f.n + idLen(0x114D9B74UL) + 8 + seekHead.n + idLen(0x1549A966UL) + 8 + info.n;
  uintEl(&v, 0xD7, 1, 1); uintEl(&v, 0x83, 1, 1); strEl(&v, 0x86, "V_MPEG4/ISO/AVC");
  uintEl(&s, 0xD7, 2, 1); uintEl(&s, 0x83, 17, 1); strEl(&s, 0x86, "S_TEXT/ASS"); strEl(&s, 0x22B59C, "eng");
  binEl(&s, 0x63A2, hdr, (long)strlen(hdr));
  mestre(&tracks, 0xAE, &v); mestre(&tracks, 0xAE, &s);
  cluAbs = trAbs + idLen(0x1654AE6BUL) + 8 + tracks.n;

  // Um Cluster por segundo de midia.
  for (c = 0; c < FIM; c++) {
    Buf corpo = {0}; int k, quantos, denso = c < ABERTURA_FIM || c >= DIALOGO_FIM;
    long cluPos = cluAbs + clusters.n;
    if (!denso && (c % 2)) continue;
    quantos = denso ? 40 : 1;
    uintEl(&corpo, 0xE7, (unsigned long long)c * 1000, 4);
    for (k = 0; k < quantos; k++) {
      Buf blk = {0}, grp = {0}, pos = {0}, ponto = {0}; char *txt = malloc((size_t)pad + 96);
      long rel = !denso ? 0 : (c % 16 == 5) ? k * 10 : (k / 10) * 250, relBloco = corpo.n;
      unsigned char hs[4] = { 0x82, (unsigned char)(rel >> 8), (unsigned char)(rel & 255), 0 };
      assert(txt && nEv < MAX_EV);
      if (!denso && evAposTeto < 0) evAposTeto = nEv;
      evMs[nEv] = (long)c * 1000 + rel;
      snprintf(txt, (size_t)pad + 96, "%d,0,Default,,0,0,0,,IDX%06d %s", nEv, nEv, enche);
      bytes(&blk, hs, 4); bytes(&blk, txt, (long)strlen(txt));
      mestre(&grp, 0xA1, &blk); uintEl(&grp, 0x9B, denso ? 250 : 1900, 2);
      mestre(&corpo, 0xA0, &grp);
      uintEl(&pos, 0xF7, 2, 1);
      uintEl(&pos, 0xF1, (unsigned long long)(cluPos - segIniL), 8);
      uintEl(&pos, 0xF0, (unsigned long long)relBloco, 4);
      uintEl(&ponto, 0xB3, (unsigned long long)evMs[nEv], 4);
      mestre(&ponto, 0xB7, &pos);
      mestre(&lista, 0xBB, &ponto);
      nEv++;
      free(blk.b); free(grp.b); free(pos.b); free(ponto.b); free(txt);
    }
    mestre(&clusters, 0x1F43B675UL, &corpo); free(corpo.b);
  }
  cuAbs = cluAbs + clusters.n;
  mestre(&cues, 0x1C53BB6BUL, &lista);
  seekHead.n = 0;
  seekEl(&seekHead, TR, trAbs - segIniL); seekEl(&seekHead, CU, cuAbs - segIniL);
  mestre(&f, 0x114D9B74UL, &seekHead); mestre(&f, 0x1549A966UL, &info);
  mestre(&f, 0x1654AE6BUL, &tracks);
  assert(f.n == cluAbs);
  bytes(&f, clusters.b, clusters.n);
  assert(f.n == cuAbs);
  bytes(&f, cues.b, cues.n);
  { Buf t = {0}; tam8(&t, f.n - segIniL); memcpy(f.b + lenPos, t.b, 8); free(t.b); }
  arq = f.b; arqN = f.n;
  free(enche);
}

// --- rede local --------------------------------------------------------------

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int pedidos;
// Ritmo: instante de cada pedido (ms), quantos estao no ar e o maior numero
// simultaneo; `estourar` = os proximos N pedidos estouram o prazo (curl 28).
#define MAX_PED 40000
static long quandoMs[MAX_PED];
static int noAr, maiorNoAr, estourar, estourados;

static long relogioMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

char *rede_baixar(const char *url, int s) { (void)url; (void)s; return NULL; }
char *rede_baixar_bin(const char *url, int s, long *n) { (void)url; (void)s; (void)n; return NULL; }
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  (void)url; snprintf(dst, tam, "local"); return dst;
}
long rede_corte_host(const char *url) { (void)url; return 0; }
int rede_resto_recusado(void) { return 0; }

char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *status, int *erro, char *final, unsigned tamFinal) {
  long n; char *p; int falha;
  (void)s;
  *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  pthread_mutex_lock(&trava);
  if (pedidos < MAX_PED) quandoMs[pedidos] = relogioMs();
  pedidos++;
  if (++noAr > maiorNoAr) maiorNoAr = noAr;
  falha = estourar > 0;
  if (falha) { estourar--; estourados++; }
  pthread_mutex_unlock(&trava);
  // Um pedido de verdade dura: sem isto dois pedidos nunca se encontrariam no
  // ar e a prova de "um por vez" nao provaria nada.
  usleep(3000);
  pthread_mutex_lock(&trava); noAr--; pthread_mutex_unlock(&trava);
  if (falha) { if (erro) *erro = 28; return NULL; }
  if (ini < 0 || fim < ini || ini >= arqN) { if (status) *status = 416; return NULL; }
  if (fim >= arqN) fim = arqN - 1;
  n = (long)(fim - ini + 1);
  p = calloc(1, (size_t)n + 1); assert(p);
  memcpy(p, arq + ini, (size_t)n);
  *tam = n;
  if (status) *status = 206;
  return p;
}

static int nPedidos(void) {
  int n; pthread_mutex_lock(&trava); n = pedidos; pthread_mutex_unlock(&trava); return n;
}

// --- duble do assrender: o documento que o libass receberia ---------------------

static char  *doc; static size_t docN, docMax; static long docEntregas;

static int guardar(const char *corpo, size_t tamanho) {
  char *c = malloc(tamanho + 1);
  assert(c);
  memcpy(c, corpo, tamanho); c[tamanho] = 0;
  pthread_mutex_lock(&trava);
  free(doc); doc = c; docN = tamanho; docEntregas++;
  if (tamanho > docMax) docMax = tamanho;
  pthread_mutex_unlock(&trava);
  return 1;
}
int  assrender_carregar(const char *corpo, size_t tamanho, unsigned g) { (void)g; return guardar(corpo, tamanho); }
int  assrender_atualizar(const char *corpo, size_t tamanho, unsigned g) { (void)g; return guardar(corpo, tamanho); }
int  assrender_carregar_texto(const char *corpo, size_t tamanho, unsigned g) { (void)corpo; (void)tamanho; (void)g; return 0; }
void assrender_limpar(void) {}
void assrender_limpar_fontes(void) {}
int  assrender_adicionar_fonte(const char *nome, const void *dados, size_t tamanho) { (void)nome; (void)dados; (void)tamanho; return 1; }
void assrender_geracao(unsigned g) { (void)g; }
void assrender_preaquecer(void) {}
const char *assrender_diagnostico(void) { return "duble"; }

// Quantas vezes cada evento aparece no documento atual.
static unsigned char vezes[MAX_EV];
static size_t contar(void) {
  const char *p; size_t n;
  memset(vezes, 0, sizeof vezes);
  pthread_mutex_lock(&trava);
  for (p = doc; p && (p = strstr(p, ",,IDX")) != NULL; p += 5) {
    int i = atoi(p + 5);
    if (i >= 0 && i < MAX_EV && vezes[i] < 255) vezes[i]++;
  }
  n = docN;
  pthread_mutex_unlock(&trava);
  return n;
}

static int falhas;
static void conferir(int ok, const char *caso) {
  printf("%s: %s\n", ok ? "ok" : "FALHA", caso);
  fflush(stdout);
  if (!ok) falhas++;
}

// Todo evento que comeca em [a, b] s esta no documento, uma vez so; e nenhum
// evento do documento inteiro aparece repetido.
static int janelaInteira(double a, double b, const char *onde) {
  int i, faltam = 0, repetidos = 0, esperados = 0;
  contar();
  for (i = 0; i < nEv; i++) {
    double t = evMs[i] / 1000.0;
    if (vezes[i] > 1) repetidos++;
    if (t < a || t > b) continue;
    esperados++;
    if (!vezes[i]) faltam++;
  }
  printf("  %s: %d eventos em %.0f-%.0f s, faltam %d, repetidos no documento %d\n",
         onde, esperados, a, b, faltam, repetidos);
  return esperados > 0 && !faltam && !repetidos;
}

// Espera a colheita SOSSEGAR com o playhead em `pos`: nenhum Range novo e
// nenhuma entrega nova por 800 ms (o laco ocioso acorda a cada 250 ms).
static int sossegar(double pos) {
  int ult = -1, parado = 0, i; long ultE = -1;
  for (i = 0; i < 6000; i++) {
    int e = mkvass_estado(), n; long en;
    mkvass_passo(pos);
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return e;
    n = nPedidos();
    pthread_mutex_lock(&trava); en = docEntregas; pthread_mutex_unlock(&trava);
    if (n == ult && en == ultE) { if (++parado >= 80) return e; } else { parado = 0; ult = n; ultE = en; }
    usleep(10000);
  }
  return -1;
}

static int esperarFim(double pos) {
  int i;
  for (i = 0; i < 12000; i++) {
    int e = mkvass_estado();
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return e;
    mkvass_passo(pos);
    usleep(10000);
  }
  return mkvass_estado();
}

// Para e ESPERA o fio sair: e ele quem grava o sidecar parcial na saida.
static void pararDeVez(void) {
  int i;
  mkvass_parar();
  for (i = 0; i < 2000 && mkvass_ocupado(); i++) usleep(5000);
}

// O mesmo nome que mkvass.c da ao sidecar (FNV-1a da url + faixa).
static void nomeSidecar(char *dst, size_t tam) {
  unsigned long long h = 1469598103934665603ULL;
  const unsigned char *p = (const unsigned char *)URL;
  while (*p) { h ^= *p++; h *= 1099511628211ULL; }
  snprintf(dst, tam, "mkvass-%016llx-%d.ass", h, -1);
}

// --- ./test padrao --------------------------------------------------------------

// Clusters com legenda cujo inicio cai em [a, b] s: um Range cada, no maximo.
static int clustersEm(double a, double b) {
  int c, n = 0;
  for (c = 0; c < FIM; c++) {
    int denso = c < ABERTURA_FIM || c >= DIALOGO_FIM;
    if (!denso && (c % 2)) continue;
    if (c >= a && c <= b) n++;
  }
  return n;
}

static void sidecarV3(const char *nome, const char *corpo, int eventos) {
  char *corte = malloc(strlen(corpo) + 64); const char *fim = corpo; int k = 0;
  assert(corte);
  strcpy(corte, "; mkvass-estado: completo-v3\n");
  while (k < eventos && (fim = strstr(fim, "\nDialogue: ")) != NULL) { fim++; k++; }
  assert(k == eventos && fim);
  fim = strchr(fim, '\n'); assert(fim);
  strncat(corte, corpo, (size_t)(fim + 1 - corpo));
  assert(dados_gravar_leve(nome, corte));
  free(corte);
}

static int provaPadrao(void) {
  int e, colhidos = 0, total = 0, antes, i, r0, r600;
  char nome[64], *sc; const char *cab;
  Buf v3 = {0};
  nomeSidecar(nome, sizeof nome);

  mkvass_iniciar_ordinal(URL, 0);
  e = sossegar(0.0);
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  r0 = nPedidos();
  printf("playhead 0 s: estado %d, %d/%d blocos (arquivo com %d), documento %zu bytes, %d Ranges "
         "(a janela tem %d Clusters; a base fazia 203 para os 8000 que indexava)\n",
         e, colhidos, total, nEv, contar(), r0, clustersEm(0, 90));
  conferir(total == nEv && total > 8000, "o indice cobre a faixa inteira, alem de 8000 blocos");
  conferir(e == MKVASS_COLHENDO, "faixa acima de 8000 blocos: a colheita NAO se declara completa");
  conferir(janelaInteira(0.0, 85.0, "abertura"), "playhead 0 s: a janela esta inteira no documento");
  // O #308: nada de faixa inteira em segundo plano. Cabecalho + Cues (3) e um
  // Range por Cluster da janela (com o do palpite, quando falha, e folga).
  conferir(r0 <= clustersEm(0, 90) + 8, "so a janela foi pedida: nenhum Range para o resto da faixa");
  conferir(!vezes[evAposTeto], "o que esta longe do playhead nao foi baixado");
  antes = nPedidos();
  for (i = 0; i < 150; i++) { mkvass_passo(0.0); usleep(10000); }
  conferir(nPedidos() == antes, "playhead parado: nenhum Range a mais");

  // Seek para DEPOIS do bloco 8000 (era aqui que nao havia fala).
  e = sossegar(600.0);
  r600 = nPedidos() - r0;
  printf("seek para 600 s: %d Ranges (a janela tem %d Clusters)\n", r600, clustersEm(592, 690));
  conferir(e == MKVASS_COLHENDO && janelaInteira(594.0, 685.0, "dialogo"),
           "seek para 600 s: ha fala depois do bloco 8000");
  conferir(vezes[evAposTeto + 195] == 1 && r600 <= clustersEm(592, 690) + 8,
           "o seek pediu so a janela nova");
  pararDeVez();
  sc = dados_ler(nome);
  conferir(sc && strstr(sc, "; mkvass-estado: parcial-") == sc,
           "o sidecar de uma faixa em janela e PARCIAL, nunca completo");
  free(sc);

  // O sidecar que a 2.0.2 deixou: "completo-v3" com os 8000 primeiros eventos
  // (montado aqui com o mesmo formato de linha que o mkvass escreve).
  cab = "[Script Info]\nScriptType: v4.00+\n\n[Events]\nFormat: Layer, Start, End, Style, Name, "
        "MarginL, MarginR, MarginV, Effect, Text\n";
  bytes(&v3, cab, (long)strlen(cab));
  for (i = 0; i < 8000; i++) {
    char l[160]; long ms = evMs[i];
    snprintf(l, sizeof l, "Dialogue: 0,%ld:%02ld:%02ld.%02ld,%ld:%02ld:%02ld.%02ld,Default,,0,0,0,,IDX%06d xxxxxxxx\n",
             ms / 3600000, ms / 60000 % 60, ms / 1000 % 60, ms / 10 % 100,
             (ms + 250) / 3600000, (ms + 250) / 60000 % 60, (ms + 250) / 1000 % 60, (ms + 250) / 10 % 100, i);
    bytes(&v3, l, (long)strlen(l));
  }
  bytes(&v3, "", 1);
  sidecarV3(nome, (const char *)v3.b, 8000);
  antes = nPedidos();
  mkvass_iniciar_ordinal(URL, 0);
  e = sossegar(600.0);
  contar();
  printf("reabertura com sidecar v3 truncado: estado %d, %d Ranges novos\n", e, nPedidos() - antes);
  conferir(e == MKVASS_COLHENDO && nPedidos() > antes && vezes[evAposTeto + 195] == 1,
           "sidecar 'completo' truncado da 2.0.2 nao e aceito: ha fala em 600 s na reabertura");
  pararDeVez();

  // Um sidecar v3 de faixa pequena continua valendo: sem rede nenhuma.
  // (O completo exige o cache de fontes ao lado; este arquivo nao tem anexos.)
  { char nf[80]; snprintf(nf, sizeof nf, "%s.fonts", nome); assert(dados_gravar_leve(nf, "NVASS-FONTES-1\n0\n")); }
  sidecarV3(nome, (const char *)v3.b, 100);
  antes = nPedidos();
  mkvass_iniciar_ordinal(URL, 0);
  for (i = 0; i < 500 && mkvass_estado() != MKVASS_COMPLETO; i++) usleep(10000);
  contar();
  conferir(mkvass_estado() == MKVASS_COMPLETO && nPedidos() == antes && vezes[99] == 1 && !vezes[100],
           "sidecar completo v3 de faixa abaixo do teto antigo continua aceito, sem rede");
  pararDeVez();
  free(v3.b);
  return falhas ? 1 : 0;
}

// --- ./test ritmo ----------------------------------------------------------------

// Maior numero de pedidos comecados em qualquer intervalo de 1 s.
static int maiorPorSegundo(void) {
  int i, j = 0, maior = 0, n;
  pthread_mutex_lock(&trava);
  n = pedidos < MAX_PED ? pedidos : MAX_PED;
  for (i = 0; i < n; i++) {
    while (quandoMs[i] - quandoMs[j] >= 1000) j++;
    if (i - j + 1 > maior) maior = i - j + 1;
  }
  pthread_mutex_unlock(&trava);
  return maior;
}

static void tocar(double de, int segundos) {
  int t, k;
  for (t = 0; t < segundos; t++) for (k = 0; k < 100; k++) { mkvass_passo(de + t + k / 100.0); usleep(10000); }
}

static int provaRitmo(void) {
  int e, n0, n1, i, simult; long p0, t0;
  // Abertura densa, depois seek para o encerramento denso e de volta: o
  // leitor tem trabalho de sobra o tempo todo — e a hora em que estouraria.
  mkvass_iniciar_ordinal(URL, 0);
  t0 = relogioMs();
  tocar(0.0, 4);
  tocar(1320.0, 4);
  tocar(100.0, 4);
  n0 = nPedidos();
  pthread_mutex_lock(&trava); simult = maiorNoAr; pthread_mutex_unlock(&trava);
  printf("12 s tocando com dois seeks: %d Ranges em %ld ms (%.2f por segundo), no maximo %d em 1 s, "
         "no maximo %d ao mesmo tempo\n", n0, relogioMs() - t0, n0 * 1000.0 / (double)(relogioMs() - t0),
         maiorPorSegundo(), simult);
  conferir(n0 > 20, "o leitor trabalhou o tempo todo (ha o que medir)");
  conferir(maiorPorSegundo() <= 3, "nunca mais que 3 Ranges em 1 s, nem logo depois de um seek");
  conferir(simult == 1, "um pedido por vez: nenhuma conexao a mais ao lado do video");

  // O CDN pede calma: curl 28 em sete Ranges seguidos, no meio de um seek.
  p0 = mkvass_pausas_cdn();
  pthread_mutex_lock(&trava); estourar = 7; estourados = 0; pthread_mutex_unlock(&trava);
  n1 = nPedidos();
  for (i = 0; i < 3000 && mkvass_pausas_cdn() == p0; i++) { mkvass_passo(900.0); usleep(10000); }
  e = mkvass_estado();
  printf("freio do CDN: %ld pausa(s), estado %d, %d Ranges ate pausar\n",
         mkvass_pausas_cdn() - p0, e, nPedidos() - n1);
  conferir(mkvass_pausas_cdn() > p0 && e == MKVASS_COLHENDO,
           "curl 28 repetido PAUSA a leitura: nao desiste nem insiste");
  conferir(nPedidos() - n1 <= 6, "ate pausar foram so as tentativas com recuo, nao uma rajada");
  e = sossegar(900.0);
  conferir(e == MKVASS_COLHENDO && janelaInteira(894.0, 985.0, "dialogo depois do freio"),
           "depois da pausa a leitura volta e a janela fica inteira");
  conferir(maiorPorSegundo() <= 3, "o teto de 3 por segundo valeu a prova inteira");
  pararDeVez();
  return falhas ? 1 : 0;
}

// --- ./test janela ---------------------------------------------------------------

static int provaJanela(void) {
  int e, colhidos = 0, total = 0, t;
  char nome[64], *sc;
  nomeSidecar(nome, sizeof nome);

  mkvass_iniciar_ordinal(URL, 0);
  e = sossegar(5.0);
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  printf("playhead 5 s: estado %d, %d blocos no indice, documento %zu bytes (maior %zu), %d Ranges\n",
         e, total, contar(), docMax, nPedidos());
  conferir(total == nEv && total > 8000, "o indice cobre a faixa inteira, alem de 8000 blocos");
  conferir(e == MKVASS_COLHENDO, "corpo em janela: a colheita NAO se declara completa");
  conferir(janelaInteira(0.0, 20.0, "abertura"), "playhead 5 s: a janela esta inteira no documento");

  // Seek para longe, DEPOIS do bloco 8000 (era aqui que nao havia fala).
  e = sossegar(900.0);
  conferir(e == MKVASS_COLHENDO && janelaInteira(894.0, 980.0, "dialogo"),
           "seek para 900 s: ha fala depois do bloco 8000");
  e = sossegar(1320.0);
  conferir(e == MKVASS_COLHENDO && janelaInteira(1314.0, 1338.0, "encerramento"),
           "seek para o encerramento: a janela esta inteira");
  // De volta a abertura, num trecho que ja foi despejado.
  e = sossegar(100.0);
  conferir(e == MKVASS_COLHENDO && janelaInteira(94.0, 118.0, "abertura de novo"),
           "seek de volta: o trecho despejado e colhido de novo, sem repetir");
  // Tocando: o playhead anda 60 s pela abertura densa, por cima do segundo
  // sem folga entre eventos (101 s, 117 s, 133 s...).
  for (t = 100; t <= 160; t += 2) { int k; for (k = 0; k < 15; k++) { mkvass_passo((double)t); usleep(10000); } }
  e = sossegar(160.0);
  conferir(e == MKVASS_COLHENDO && janelaInteira(154.0, 178.0, "tocando"),
           "tocando pela abertura: a janela acompanha o playhead, sem perder nem repetir");
  printf("maior documento entregue: %zu bytes (teto %ld), %d Ranges\n", docMax, (long)MKVASS_CORPO_MAX, nPedidos());
  conferir(docMax > 0 && docMax <= (size_t)MKVASS_CORPO_MAX, "o corpo entregue nunca passa do teto");
  pararDeVez();
  sc = dados_ler(nome);
  conferir(!sc || strstr(sc, "; mkvass-estado: completo-") != sc,
           "colheita em janela nao grava sidecar completo");
  free(sc);
  return falhas ? 1 : 0;
}

int main(int argc, char **argv) {
  int janela = argc > 1 && !strcmp(argv[1], "janela"), ritmo = argc > 1 && !strcmp(argv[1], "ritmo"), r;
  if (argc < 2 || (!janela && !ritmo && strcmp(argv[1], "padrao"))) {
    fprintf(stderr, "uso: %s padrao|janela|ritmo\n", argv[0]); return 2;
  }
  // Fala curta nas provas com os tetos de producao; ~300 bytes na da janela (a abertura passa
  // de 2,5 MB, varias vezes o teto com que ela e compilada).
  pad = janela ? 260 : 8;
  montar();
  printf("arquivo: %ld bytes, %d eventos (o %d e o primeiro depois da abertura)\n", arqN, nEv, evAposTeto);
  dados_iniciar(".");
  r = janela ? provaJanela() : ritmo ? provaRitmo() : provaPadrao();
  puts(r ? "mkvass_alem_teto: FALHOU" : "mkvass_alem_teto: ok");
  return r;
}
