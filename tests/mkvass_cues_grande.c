// 17/11 (dono): Fight Club 4K remux, 145 faixas, 137 legendas indexadas: o
// Cues tinha 9,35 MB. Com o teto antigo de 4 MB o mkvass nem lia o indice e
// a legenda de texto embutida ia para a VARREDURA do arquivo (Clusters de
// video baixados de novo ao lado do video). Este teste monta um arquivo
// VIRTUAL com um Cues de ~9 MB, cujo unico ponto da faixa de legenda fica
// DEPOIS dos primeiros 4 MB (cruza a costura dos pedacos), e exige:
//   - COMPLETO pelo indice (nao varredura);
//   - nenhum Range maior que um pedaco (o Cues nunca inteiro na memoria);
//   - o Cues lido uma vez so (os bytes pedidos dele = o tamanho dele).
#include "mkvass.h"
#include "legenda.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MIB (1024LL * 1024LL)
#define TOTAL (2048LL * MIB)
#define CLUSTER_ABS (1024LL * MIB + 4096)
#define CUES_ABS (1536LL * MIB + 100)
#define FALA "Hello from a 9 MB index"
#define PONTOS_ENCHE 230000   // ~41 bytes cada: ~9,4 MB de Cues

typedef struct { unsigned char *b; long n, cap; } Buf;
typedef struct { long long at; Buf *buf; } Trecho;

static Buf cab, clu, cues;
static Trecho trechos[3];
static long long segIni, maiorPedido, bytesNoCues;
static int pedidos;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static void bytes(Buf *b, const void *p, long n) {
  if (b->n + n > b->cap) {
    b->cap = (b->n + n) * 2 + 4096;
    b->b = realloc(b->b, (size_t)b->cap); assert(b->b);
  }
  memcpy(b->b + b->n, p, (size_t)n); b->n += n;
}
static void id(Buf *b, unsigned long v) {
  unsigned char t[4]; int n = v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1, i;
  for (i = 0; i < n; i++) t[i] = (unsigned char)(v >> (8 * (n - 1 - i)));
  bytes(b, t, n);
}
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
static void limpar(Buf *b) { b->n = 0; }

// Um CuePoint (tempo, faixa, Cluster, posicao relativa) no fim de `lista`.
static void ponto(Buf *lista, unsigned long tempo, int faixa, long long cluster, long rel) {
  static Buf pos, pt;
  limpar(&pos); limpar(&pt);
  uintEl(&pos, 0xF7, (unsigned long long)faixa, 1);
  uintEl(&pos, 0xF1, (unsigned long long)cluster, 8);
  uintEl(&pos, 0xF0, (unsigned long long)rel, 2);
  uintEl(&pt, 0xB3, tempo, 4);
  mestre(&pt, 0xB7, &pos);
  mestre(lista, 0xBB, &pt);
}

static void montar(void) {
  Buf ebml = {0}, seek = {0}, seekHead = {0}, info = {0}, v = {0}, s = {0}, o = {0}, tracks = {0};
  Buf corpo = {0}, block = {0}, grupo = {0}, lista = {0};
  unsigned char dur[8]; double d = 7200000.0; unsigned long long u; long relBloco; int i;

  uintEl(&ebml, 0x4286, 1, 1);
  strEl(&ebml, 0x4282, "matroska");
  mestre(&cab, 0x1A45DFA3UL, &ebml);
  id(&cab, 0x18538067UL);
  segIni = cab.n + 8;
  tam8(&cab, TOTAL - segIni);

  id(&seek, 0x53AB); { unsigned char z = 0x84, c[4] = { 0x1C, 0x53, 0xBB, 0x6B };
    bytes(&seek, &z, 1); bytes(&seek, c, 4); }
  uintEl(&seek, 0x53AC, (unsigned long long)(CUES_ABS - segIni), 8);
  mestre(&seekHead, 0x4DBB, &seek);
  mestre(&cab, 0x114D9B74UL, &seekHead);

  uintEl(&info, 0x2AD7B1, 1000000, 4);
  memcpy(&u, &d, 8);
  for (i = 0; i < 8; i++) dur[i] = (unsigned char)(u >> (8 * (7 - i)));
  id(&info, 0x4489); { unsigned char z = 0x88; bytes(&info, &z, 1); bytes(&info, dur, 8); }
  mestre(&cab, 0x1549A966UL, &info);

  uintEl(&v, 0xD7, 1, 1); uintEl(&v, 0x83, 1, 1); strEl(&v, 0x86, "V_MPEGH/ISO/HEVC");
  uintEl(&s, 0xD7, 2, 1); uintEl(&s, 0x83, 17, 1); strEl(&s, 0x86, "S_TEXT/UTF8");
  strEl(&s, 0x22B59C, "eng");
  uintEl(&o, 0xD7, 3, 1); uintEl(&o, 0x83, 17, 1); strEl(&o, 0x86, "S_HDMV/PGS");
  mestre(&tracks, 0xAE, &v);
  mestre(&tracks, 0xAE, &s);
  mestre(&tracks, 0xAE, &o);
  mestre(&cab, 0x1654AE6BUL, &tracks);

  uintEl(&corpo, 0xE7, 60000, 4);
  relBloco = corpo.n;
  { unsigned char h[4] = { 0x82, 0, 0, 0 }; bytes(&block, h, 4); bytes(&block, FALA, (long)strlen(FALA)); }
  mestre(&grupo, 0xA1, &block);
  uintEl(&grupo, 0x9B, 2000, 2);
  mestre(&corpo, 0xA0, &grupo);
  mestre(&clu, 0x1F43B675UL, &corpo);

  // O enchimento: video e PGS, Clusters que nunca sao lidos. O ponto da
  // legenda vai depois de ~5,5 MB (passa do primeiro pedaco de 4 MB).
  for (i = 0; i < PONTOS_ENCHE; i++) {
    if (i == PONTOS_ENCHE * 6 / 10) ponto(&lista, 60000, 2, CLUSTER_ABS - segIni, relBloco);
    ponto(&lista, (unsigned long)i * 30, i % 2 ? 1 : 3, (CLUSTER_ABS - segIni) + 1000000LL * (i / 50), 0);
  }
  mestre(&cues, 0x1C53BB6BUL, &lista);

  trechos[0].at = 0;           trechos[0].buf = &cab;
  trechos[1].at = CLUSTER_ABS; trechos[1].buf = &clu;
  trechos[2].at = CUES_ABS;    trechos[2].buf = &cues;
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
  long n; char *p; int k;
  (void)s;
  *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  if (ini < 0 || fim < ini || ini >= TOTAL) { if (status) *status = 416; return NULL; }
  if (fim >= TOTAL) fim = TOTAL - 1;
  n = (long)(fim - ini + 1);
  pthread_mutex_lock(&trava);
  pedidos++;
  if (n > maiorPedido) maiorPedido = n;
  { long long a = CUES_ABS, b = CUES_ABS + cues.n, de = ini > a ? ini : a, ate = fim + 1 < b ? fim + 1 : b;
    if (de < ate) bytesNoCues += ate - de; }
  pthread_mutex_unlock(&trava);
  p = calloc(1, (size_t)n + 1); assert(p);
  for (k = 0; k < 3; k++) {
    long long a = trechos[k].at, b = a + trechos[k].buf->n;
    long long de = ini > a ? ini : a, ate = fim + 1 < b ? fim + 1 : b;
    if (de < ate) memcpy(p + (de - ini), trechos[k].buf->b + (de - a), (size_t)(ate - de));
  }
  *tam = n;
  if (status) *status = 206;
  return p;
}

static int esperar(void) {
  for (int i = 0; i < 3000; i++) {
    int e = mkvass_estado();
    if (e == MKVASS_COMPLETO || e >= MKVASS_NOGO) return e;
    mkvass_passo(60.0);
    usleep(10000);
  }
  return mkvass_estado();
}

int main(void) {
  int e, colhidos = 0, total = 0;
  LegendaCue cue[4];
  montar();
  printf("Cues de %ld bytes (%.2f MB)\n", cues.n, cues.n / (double)MIB);
  assert(cues.n > 8 * MIB);
  dados_iniciar(".");
  mkvass_aceitar_texto(1);
  mkvass_iniciar_ordinal("https://example.test/remux-145-faixas.mkv", 0);
  e = esperar();
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  printf("estado %d, varredura %d, %d pedidos, maior Range %lld bytes, %lld bytes do Cues pedidos, %d/%d blocos\n",
         e, mkvass_varredura(), pedidos, maiorPedido, bytesNoCues, colhidos, total);
  assert(e == MKVASS_COMPLETO);
  assert(!mkvass_varredura());                       // pelo indice, nao varrendo o arquivo
  assert(total == 1 && colhidos == 1);
  assert(maiorPedido <= 4 * MIB + 64 * 1024);        // nunca o Cues inteiro num pedido
  assert(bytesNoCues <= cues.n + 256 * 1024);         // o Cues lido uma vez so
  assert(legenda_cues(60.5, 0, cue, 4) == 1);
  assert(!strcmp(cue[0].texto, FALA));
  mkvass_parar();
  puts("mkvass_cues_grande: ok");
  return 0;
}
