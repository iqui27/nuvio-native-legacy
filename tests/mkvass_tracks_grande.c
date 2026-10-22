// #308: faixa ASS embutida cujo Tracks passa de 64 KB (CodecPrivate de fansub
// com estilos e typesetting grandes: o Tracks do remux TTGA tinha 96393
// bytes). O Tracks nao cabe na janela do cabecalho; o mkvass buscava o elemento
// por posicao do SeekHead com um Range de 64 KB fixos, via o elemento cortado
// e devolvia NOGO_FAIXA ("faixa nao e ASS"): a legenda ia para a TV como texto
// simples. Este teste monta um MKV pequeno com esse Tracks e exige COMPLETO.
//
// (base: teste de MKV >2 GiB, #269)
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


#define FALA "Hello from a big ASS header"
#define BUFMAX (256 * 1024)

typedef struct { unsigned char *b; long n; } Buf;
static unsigned char *arq; static long arqN;
static int pedidos;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Buf nova(void) { Buf b; b.b = calloc(1, BUFMAX); b.n = 0; assert(b.b); return b; }
static void bytes(Buf *b, const void *p, long n) {
  assert(b->n + n <= BUFMAX);
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
static void binEl(Buf *b, unsigned long eid, const void *p, long n) {
  id(b, eid); tam8(b, n); bytes(b, p, n);
}
static void seekEl(Buf *sh, const unsigned char alvo[4], long long pos) {
  Buf s = nova();
  id(&s, 0x53AB); { unsigned char z = 0x84; bytes(&s, &z, 1); bytes(&s, alvo, 4); }
  uintEl(&s, 0x53AC, (unsigned long long)pos, 8);
  mestre(sh, 0x4DBB, &s); free(s.b);
}

static long idLen(unsigned long v) { return v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1; }

static void montar(void) {
  Buf f = nova(), ebml = nova(), info = nova(), seekHead = nova(), tracks = nova(), v = nova(), s = nova();
  Buf corpo = nova(), block = nova(), grupo = nova(), pos = nova(), ponto = nova(), lista = nova(), cues = nova(), clu = nova();
  unsigned char dur[8]; double d = 120000.0; unsigned long long u; int i;
  static const unsigned char TR[4] = { 0x16, 0x54, 0xAE, 0x6B }, CU[4] = { 0x1C, 0x53, 0xBB, 0x6B };
  long segIniL, lenPos, trAbs, cuAbs, cluAbs, relBloco, privN = 96 * 1024;
  char *priv = malloc((size_t)privN + 4096); long pn = 0;

  uintEl(&ebml, 0x4286, 1, 1);
  strEl(&ebml, 0x4282, "matroska");
  mestre(&f, 0x1A45DFA3UL, &ebml);
  id(&f, 0x18538067UL);
  lenPos = f.n; tam8(&f, 0);
  segIniL = f.n;

  uintEl(&info, 0x2AD7B1, 1000000, 4);
  memcpy(&u, &d, 8);
  for (i = 0; i < 8; i++) dur[i] = (unsigned char)(u >> (8 * (7 - i)));
  id(&info, 0x4489); { unsigned char z = 0x88; bytes(&info, &z, 1); bytes(&info, dur, 8); }

  // SeekHead de tamanho fixo (posicoes em 8 bytes): mede com zeros.
  seekEl(&seekHead, TR, 0); seekEl(&seekHead, CU, 0);
  trAbs = f.n + idLen(0x114D9B74UL) + 8 + seekHead.n + idLen(0x1549A966UL) + 8 + info.n;

  pn += sprintf(priv + pn, "[Script Info]\r\nTitle: typeset grande\r\nScriptType: v4.00+\r\nPlayResX: 1920\r\nPlayResY: 1080\r\n\r\n"
                "[V4+ Styles]\r\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\r\n");
  for (i = 0; pn < privN; i++)
    pn += sprintf(priv + pn, "Style: S%05d,Arial,48,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,0,2,10,10,10,1\r\n", i);
  pn += sprintf(priv + pn, "\r\n[Events]\r\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\r\n");

  uintEl(&v, 0xD7, 1, 1); uintEl(&v, 0x83, 1, 1); strEl(&v, 0x86, "V_MPEG4/ISO/AVC");
  uintEl(&s, 0xD7, 2, 1); uintEl(&s, 0x83, 17, 1); strEl(&s, 0x86, "S_TEXT/ASS");
  strEl(&s, 0x22B59C, "eng");
  binEl(&s, 0x63A2, priv, pn);
  mestre(&tracks, 0xAE, &v);
  mestre(&tracks, 0xAE, &s);
  cluAbs = trAbs + idLen(0x1654AE6BUL) + 8 + tracks.n;

  uintEl(&corpo, 0xE7, 60000, 4);
  relBloco = corpo.n;
  { unsigned char h[4] = { 0x82, 0, 0, 0 };
    const char *p = "0,0,S00000,,0,0,0,," FALA;
    bytes(&block, h, 4); bytes(&block, p, (long)strlen(p)); }
  mestre(&grupo, 0xA1, &block);
  uintEl(&grupo, 0x9B, 2000, 2);
  mestre(&corpo, 0xA0, &grupo);
  mestre(&clu, 0x1F43B675UL, &corpo);
  cuAbs = cluAbs + clu.n;

  uintEl(&pos, 0xF7, 2, 1);
  uintEl(&pos, 0xF1, (unsigned long long)(cluAbs - segIniL), 8);
  uintEl(&pos, 0xF0, (unsigned long long)relBloco, 2);
  uintEl(&ponto, 0xB3, 60000, 4);
  mestre(&ponto, 0xB7, &pos);
  mestre(&lista, 0xBB, &ponto);
  mestre(&cues, 0x1C53BB6BUL, &lista);

  seekHead.n = 0;
  seekEl(&seekHead, TR, trAbs - segIniL); seekEl(&seekHead, CU, cuAbs - segIniL);
  mestre(&f, 0x114D9B74UL, &seekHead);
  mestre(&f, 0x1549A966UL, &info);
  assert(f.n == trAbs);
  mestre(&f, 0x1654AE6BUL, &tracks);
  assert(f.n == cluAbs);
  bytes(&f, clu.b, clu.n);
  assert(f.n == cuAbs);
  bytes(&f, cues.b, cues.n);
  { Buf vz = nova(); static unsigned char z[16384]; binEl(&vz, 0xEC, z, sizeof z); bytes(&f, vz.b, vz.n); free(vz.b); }   // folga: Range nao termina curto
  { Buf t = nova(); tam8(&t, f.n - segIniL); memcpy(f.b + lenPos, t.b, 8); free(t.b); }
  arq = f.b; arqN = f.n;
  assert(cluAbs > 64 * 1024);   // o Tracks passa da janela de 64 KB
  free(priv);
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
  long n; char *p;
  (void)s;
  *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 0;
  if (final && tamFinal) snprintf(final, tamFinal, "%s", url);
  pthread_mutex_lock(&trava);
  pedidos++;
  pthread_mutex_unlock(&trava);
  if (ini < 0 || fim < ini || ini >= arqN) { if (status) *status = 416; return NULL; }
  if (fim >= arqN) fim = arqN - 1;
  n = (long)(fim - ini + 1);
  p = calloc(1, (size_t)n + 1); assert(p);
  memcpy(p, arq + ini, (size_t)n);
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
  mkvass_iniciar_ordinal("https://example.test/remux.mkv", 0);
  e = esperar();
  mkvass_estatisticas(NULL, NULL, &colhidos, &total);
  printf("estado %d, %d pedidos, %d/%d blocos\n", e, pedidos, colhidos, total);
  assert(e == MKVASS_COMPLETO);          // antes: NOGO_FAIXA ("faixa nao e ASS")
  assert(total == 1 && colhidos == 1);
  assert(legenda_cues(60.5, 0, cue, 4) == 1);
  assert(strstr(cue[0].texto, FALA));
  mkvass_parar();
  puts("mkvass_tracks_grande: ok");
  return 0;
}
