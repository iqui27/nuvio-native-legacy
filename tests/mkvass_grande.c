// #269: legenda de TEXTO embutida num MKV maior que 2 GiB (remux 4K).
//
// No registro do S90C (.tpk, ARMv7) a pre-busca leu o cabecalho, a sonda achou
// 11 faixas no MESMO trecho, e o mkvass disse "nao e MKV (HTTP 0, curl 0)":
// lerTam recusava o tamanho do Segment por passar de LONG_MAX, que no ARM de
// 32 bits e 2 GiB. As posicoes do SeekHead e do Cues alem de 2 GiB tambem
// davam a volta. Este teste monta um arquivo VIRTUAL de 6 GiB (so tres
// trechos existem: cabecalho, um Cluster depois de 3 GiB e o Cues depois de
// 5 GiB) e serve Range por um transporte local, sem socket.
//
// Roda no Mac (64 bits) por tests/mkvass_grande.sh e, para provar o caso da
// TV, no ARMv7 de 32 bits do nuvio-tpk-sdk (MKVASS_GRANDE_ARM=1).
#include "mkvass.h"
#include "legenda.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GIB (1024LL * 1024LL * 1024LL)
#define TOTAL (6 * GIB)
#define CLUSTER_ABS (3 * GIB + 4096)
#define CUES_ABS (5 * GIB + 100)
#define FALA "Hello from past 3 GiB"

typedef struct { unsigned char b[4096]; long n; } Buf;
typedef struct { long long at; Buf *buf; } Trecho;

static Buf cab, clu, cues;
static Trecho trechos[3];
static long long segIni, maiorIni;
static int pedidos;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static void bytes(Buf *b, const void *p, long n) {
  assert(b->n + n <= (long)sizeof b->b);
  memcpy(b->b + b->n, p, (size_t)n); b->n += n;
}
static void id(Buf *b, unsigned long v) {
  unsigned char t[4]; int n = v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1, i;
  for (i = 0; i < n; i++) t[i] = (unsigned char)(v >> (8 * (n - 1 - i)));
  bytes(b, t, n);
}
// Tamanho sempre em 8 bytes (0x01 + 7): valido em EBML e cabe 6 GiB.
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
// Elemento mestre: id + tamanho de 8 bytes + o corpo ja montado.
static void mestre(Buf *b, unsigned long eid, const Buf *corpo) {
  id(b, eid); tam8(b, corpo->n); bytes(b, corpo->b, corpo->n);
}

static long relBloco;   // CueRelativePosition: do inicio dos DADOS do Cluster

static void montar(void) {
  Buf ebml = {0}, seek = {0}, seekHead = {0}, info = {0}, v = {0}, s = {0}, tracks = {0};
  Buf corpo = {0}, block = {0}, grupo = {0}, pos = {0}, ponto = {0}, lista = {0};
  unsigned char dur[8]; double d = 7200000.0; unsigned long long u; int i;

  uintEl(&ebml, 0x4286, 1, 1);
  strEl(&ebml, 0x4282, "matroska");
  mestre(&cab, 0x1A45DFA3UL, &ebml);
  id(&cab, 0x18538067UL);
  // Segment: dos dados ate o fim do arquivo virtual — passa de 2 GiB.
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
  mestre(&tracks, 0xAE, &v);
  mestre(&tracks, 0xAE, &s);
  mestre(&cab, 0x1654AE6BUL, &tracks);

  // Cluster depois de 3 GiB: Timestamp 60 s e um BlockGroup da faixa 2.
  uintEl(&corpo, 0xE7, 60000, 4);
  relBloco = corpo.n;
  { unsigned char h[4] = { 0x82, 0, 0, 0 }; bytes(&block, h, 4); bytes(&block, FALA, (long)strlen(FALA)); }
  mestre(&grupo, 0xA1, &block);
  uintEl(&grupo, 0x9B, 2000, 2);
  mestre(&corpo, 0xA0, &grupo);
  mestre(&clu, 0x1F43B675UL, &corpo);

  // Cues depois de 5 GiB, apontando o Cluster pela posicao relativa ao Segment.
  uintEl(&pos, 0xF7, 2, 1);
  uintEl(&pos, 0xF1, (unsigned long long)(CLUSTER_ABS - segIni), 8);
  uintEl(&pos, 0xF0, (unsigned long long)relBloco, 2);
  uintEl(&ponto, 0xB3, 60000, 4);
  mestre(&ponto, 0xB7, &pos);
  mestre(&lista, 0xBB, &ponto);
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

// Servidor de Range sobre o arquivo virtual: zeros fora dos tres trechos.
char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *status, int *erro, char *final, unsigned tamFinal) {
  long n; char *p; int k;
  (void)s;
  *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  pthread_mutex_lock(&trava);
  pedidos++;
  if (ini > maiorIni) maiorIni = ini;
  pthread_mutex_unlock(&trava);
  if (ini < 0 || fim < ini || ini >= TOTAL) { if (status) *status = 416; return NULL; }
  if (fim >= TOTAL) fim = TOTAL - 1;
  n = (long)(fim - ini + 1);
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
  for (int i = 0; i < 2000; i++) {
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
  dados_iniciar(".");
  mkvass_aceitar_texto(1);
  mkvass_iniciar_ordinal("https://example.test/filme-4k.mkv", 0);
  e = esperar();
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  printf("long=%zu bytes, estado %d, %d pedidos, maior Range em %lld, %d/%d blocos\n",
         sizeof(long), e, pedidos, maiorIni, colhidos, total);
  assert(e == MKVASS_COMPLETO);          // antes: 10 (nao e MKV) no ARM de 32 bits
  assert(maiorIni >= CUES_ABS);          // o Cues de verdade, nao uma posicao que deu a volta
  assert(total == 1 && colhidos == 1);
  assert(legenda_cues(60.5, 0, cue, 4) == 1);
  assert(!strcmp(cue[0].texto, FALA));
  mkvass_parar();
  puts("mkvass_grande: ok");
  return 0;
}
