// PR 600 (etapa A): tamanhos EBML e profundidade hostis na sonda do MKV.
//   * lerTam em 64 bits saturando em LONG_MAX: em ILP32 (webOS ARM32, Tizen
//     armv7, wasm) o `long` de 32 bits truncava os 8 bytes do tamanho — um
//     elemento declarando >4 GiB virava outro numero (0, negativo, lixo).
//     No host LP64 esse ramo tem o mesmo comportamento; o que difere aqui e
//     a profundidade e o "parar limpo" nos tamanhos absurdos.
//   * comparacoes `tam > n - o` em vez de `o + tam > n` (estouro com tam
//     perto de LONG_MAX).
//   * rpuNoContentor com profundidade <= 8 (paridade com o nucleo Rust,
//     PROFUNDIDADE_MAX): BlockGroup aninhado em cadeia estourava a pilha.
//   * arquivo valido com Segment declarado >4 GiB continua abrindo igual,
//     e BlockGroup aninhado ate a profundidade 8 continua varrido.
//
// O BlockGroup x9 e a cadeia de 250 mil diferem da base: la dentro a sonda
// varria o quadro (quadros=1 aqui seria 0) e a cadeia estourava a pilha —
// o teste falha, ou crasha sob ASan, em c18c21756.
//
// Montador EBML baseado em tests/mkv_dvrpu.c.
#include "../src/mkv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void ok(int c, const char *o) {
  printf("  %-64s %s\n", o, c ? "ok" : "FALHOU");
  if (!c) falhas++;
}

// --- montador de EBML: tamanho sempre em 8 bytes (0x01 + 7) -------------------
static unsigned char *B;
static long N;
static void bytes(const void *p, long n) { memcpy(B + N, p, n); N += n; }
static void id(unsigned long v) {
  unsigned char b[4]; int k = v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1, i;
  for (i = 0; i < k; i++) b[i] = (unsigned char)(v >> (8 * (k - 1 - i)));
  bytes(b, k);
}
static long abre(unsigned long v) { id(v); B[N] = 0x01; N += 8; return N; }   // devolve o inicio dos dados
static void fecha(long ini) {
  long t = N - ini, i;
  for (i = 0; i < 7; i++) B[ini - 7 + i] = (unsigned char)(t >> (8 * (6 - i)));
}
static void el(unsigned long v, const void *p, long n) { long a = abre(v); bytes(p, n); fecha(a); }
static void u8el(unsigned long v, unsigned char x) { el(v, &x, 1); }
// Elemento com tamanho de 8 bytes JA escrito (para declarar um numero que nao
// e o tamanho real, ex.: >4 GiB ou 2^55).
static void elTam(const void *p, long n, unsigned long v, unsigned long long decl) {
  int i;
  id(v); B[N] = 0x01; N += 8;
  for (i = 0; i < 7; i++) B[N - 7 + i] = (unsigned char)(decl >> (8 * (6 - i)));
  bytes(p, n);
}

static unsigned char hvcc[64];
static long hvccN(void) {
  memset(hvcc, 0, sizeof hvcc);
  hvcc[0] = 1; hvcc[21] = 0xF0 | 3; hvcc[22] = 0;   // lengthSizeMinusOne 3: nalTam 4, sem NAL 62
  return 23;
}

// EBML + Segment + Tracks (video HEVC com hvcC -> nalTam 4; audio AC3 "pol").
// segDecl >= 0: Segment declara esse tamanho; < 0: tamanho desconhecido.
static void cabecalho(long long segDecl) {
  long t, e;
  static const unsigned char segDesc[] = { 0x18, 0x53, 0x80, 0x67, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  N = 0;
  { long a = abre(0x1A45DFA3UL); fecha(a); }
  if (segDecl < 0) bytes(segDesc, sizeof segDesc);
  else {
    int i;
    id(0x18538067UL); B[N] = 0x01; N += 8;
    for (i = 0; i < 7; i++) B[N - 7 + i] = (unsigned char)((unsigned long long)segDecl >> (8 * (6 - i)));
  }
  t = abre(0x1654AE6BUL);
  e = abre(0xAEUL);
  u8el(0xD7UL, 1); u8el(0x83UL, 1);
  el(0x86UL, "V_MPEGH/ISO/HEVC", 16);
  el(0x63A2UL, hvcc, hvccN());
  fecha(e);
  e = abre(0xAEUL);
  u8el(0xD7UL, 2); u8el(0x83UL, 2); el(0x86UL, "A_AC3", 5); el(0x22B59CUL, "pol", 3);
  fecha(e);
  fecha(t);
}

// Um SimpleBlock da faixa 1 com UM quadro HEVC "sem RPU": prefixo de tamanho
// de 4 bytes + NAL tipo 19 (0x26 0x01 ...). mkv_quadro_tem_rpu devolve 0
// (quadro inteiro varrido, sem NAL 62): a sonda DECIDE (quadros++, rpu=0).
static long quadro(void) {
  long a = abre(0xA3UL), q = N;
  unsigned char h[4] = { 0x81, 0, 0, 0x80 };   // faixa 1, tempo 0, chave
  unsigned char nal[8] = { 0x26, 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A };
  unsigned char len[4] = { 0, 0, 0, sizeof nal };
  bytes(h, 4); bytes(len, 4); bytes(nal, sizeof nal);
  fecha(a);
  return N - q;   // so para contabilizar; o elemento inteiro ja esta fechado
}

// Cluster com `niv` BlockGroups aninhados em cadeia terminando no SimpleBlock.
// niv == 0: o SimpleBlock direto no Cluster.
static void clusterNinho(int niv) {
  long c = abre(0x1F43B675UL), *stk = malloc((size_t)(niv > 0 ? niv : 1) * sizeof(long));
  int i;
  u8el(0xE7UL, 0);
  for (i = 0; i < niv; i++) stk[i] = abre(0xA0UL);
  quadro();
  for (i = niv - 1; i >= 0; i--) fecha(stk[i]);
  free(stk);
  fecha(c);
}

int main(void) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  const MkvDiag *d;
  int k;
  B = calloc(1, 8L << 20);
  if (!B) return 2;

  printf("arquivo valido, Segment declarado >4 GiB\n");
  cabecalho(5LL * 1024 * 1024 * 1024);
  clusterNinho(0);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && fx[0].nalTam == 4 && fx[0].entradaInteira, "5 GiB declarados: 2 faixas, TrackEntry inteira");
  ok(d->rpu == 0 && d->quadros == 1, "5 GiB declarados: quadro varrido, sem RPU");

  printf("BlockGroup aninhado ate a profundidade 8 (limite do nucleo Rust)\n");
  cabecalho(-1);
  clusterNinho(8);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && d->quadros == 1 && d->rpu == 0, "BlockGroup x8: quadro ainda varrido");

  printf("BlockGroup x9: alem do limite, nao desce\n");
  cabecalho(-1);
  clusterNinho(9);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && d->quadros == 0 && d->rpu == -1, "BlockGroup x9: sem varredura (na base: quadros=1)");

  printf("elemento declarando 4 GiB (8 bytes) entre Tracks e Cluster\n");
  cabecalho(-1);
  { unsigned char z[16] = { 0 };
    elTam(z, sizeof z, 0xECUL, 4ULL * 1024 * 1024 * 1024); }   // Void de 4 GiB
  clusterNinho(0);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && d->quadros == 0, "Void hostil de 4 GiB: faixas lidas, varredura para limpa");

  printf("cadeia de 250 mil BlockGroups (a base estourava a pilha)\n");
  cabecalho(-1);
  clusterNinho(250000);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && d->quadros == 0, "250 mil BlockGroups: termina sem decidir");

  printf(falhas ? "mkv_hostil: %d FALHA(S)\n" : "mkv_hostil: tudo ok\n", falhas);
  return falhas ? 1 : 0;
}

// --- transporte: a sonda nao sai do trecho ------------------------------------
char *rede_baixar_trecho(const char *u, int s, long i, long f, long *t) {
  (void)u; (void)s; (void)i; (void)f; *t = 0; return NULL;
}
char *rede_baixar_trecho_st(const char *u, int s, long long i, long long f, long *t,
                            int *st, int *er, char *fin, unsigned tf) {
  (void)u; (void)s; (void)i; (void)f; (void)fin; (void)tf;
  *t = 0; if (st) *st = 0; if (er) *er = 0; return NULL;
}
