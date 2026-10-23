// Ver tests/mkv_dvrpu.sh (203-dvrpu).
#include "../src/mkv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JANELA (320L * 1024)   // o MKV_TRECHO de src/mkv.c

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

static const unsigned char DOVI81[] = { 0x01, 0x00, 0x10, 0x35, 0x10 };   // perfil 8 nivel 6 compat 1
static unsigned char hvcc[64];
static long hvccN(int comNal62) {
  memset(hvcc, 0, sizeof hvcc);
  hvcc[0] = 1; hvcc[21] = 0xF0 | 3; hvcc[22] = comNal62 ? 1 : 0;
  if (!comNal62) return 23;
  hvcc[23] = 62; hvcc[24] = 0; hvcc[25] = 1;        // array de NAL 62 com 1 NAL
  hvcc[26] = 0; hvcc[27] = 4; hvcc[28] = 0x7C; hvcc[29] = 0x01; hvcc[30] = 0x19; hvcc[31] = 0x08;
  return 32;
}
// EBML + Segment(tamanho desconhecido) + [Void de `antes` bytes] + Tracks.
static void cabecalho(long antes, int dvcc, int nal62hvcc) {
  long t, e, m;
  static const unsigned char segDesc[] = { 0x18, 0x53, 0x80, 0x67, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  N = 0;
  { long a = abre(0x1A45DFA3UL); fecha(a); }
  bytes(segDesc, sizeof segDesc);
  if (antes) { long a = abre(0xECUL); memset(B + N, 0, antes); N += antes; fecha(a); }
  t = abre(0x1654AE6BUL);
  e = abre(0xAEUL);
  u8el(0xD7UL, 1); u8el(0x83UL, 1);
  el(0x86UL, "V_MPEGH/ISO/HEVC", 16);
  el(0x63A2UL, hvcc, hvccN(nal62hvcc));
  if (dvcc) { m = abre(0x41E4UL); el(0x41E7UL, "dvcC", 4); el(0x41EDUL, DOVI81, sizeof DOVI81); fecha(m); }
  fecha(e);
  e = abre(0xAEUL);
  u8el(0xD7UL, 2); u8el(0x83UL, 2); el(0x86UL, "A_AC3", 5); el(0x22B59CUL, "pol", 3);
  fecha(e);
  fecha(t);
}

static unsigned char *lerArq(const char *nome, long *n) {
  FILE *f = fopen(nome, "rb");
  unsigned char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc(*n);
  if (fread(b, 1, *n, f) != (size_t)*n) { free(b); b = NULL; }
  fclose(f);
  return b;
}

int main(int argc, char **argv) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  const MkvDiag *d;
  int k;
  if (argc < 3) return 2;
  B = calloc(1, 4L << 20);

  printf("(d) dvcC no lugar normal\n");
  cabecalho(0, 1, 0);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 2 && fx[0].dvPerfil == 8 && fx[0].dvNivel == 6 && fx[0].dvCompat == 1, "perfil 8.1 nivel 6 lido");
  ok(fx[0].bamN == 1 && fx[0].bamTipo == 0x64766343UL && fx[0].nalTam == 4 && fx[0].cpN == 23 &&
     fx[0].entradaInteira && !fx[0].hvccNal62, "diag: bam=1(dvcC) hvcC 23 B nalTam 4 entrada inteira");
  ok(d->tracksAchado && d->tracksInteiro && d->lidos == N, "diag: Tracks inteiro, bytes varridos");
  ok(d->rpu == -1 && d->blocoIni < 0, "sem Cluster: rpu indeciso(sem-quadro)");

  printf("(a) dvcC alem dos 320 KB (Tracks depois de 400 KB de anexos)\n");
  cabecalho(400L * 1024, 1, 0);
  k = mkv_faixas_do_trecho(B, JANELA, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
  ok(k == 0 && !d->tracksAchado && d->lidos == JANELA, "janela de 320 KB: Tracks ausente, diag diz");
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL);
  ok(k == 2 && fx[0].dvPerfil == 8, "arquivo inteiro: o mesmo dvcC aparece");
  { long corte = N - 20;   // Tracks comeca na janela e termina fora dela
    k = mkv_faixas_do_trecho(B, corte, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
    ok(k == 0 && d->tracksAchado && !d->tracksInteiro, "Tracks cortado: diag diz cortado"); }

  printf("hvcC com array de NAL 62 (lugar nao padrao)\n");
  cabecalho(0, 0, 1);
  k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL);
  ok(k == 2 && fx[0].hvccNal62 == 1 && fx[0].bamN == 0 && fx[0].dvPerfil == 0, "nal62 no hvcC marcado, sem dvcC");

  printf("quadro de video maior que a janela\n");
  { long c, sb, dados, fimQ, i;
    static const unsigned char rpu[] = { 0, 0, 0, 10, 0x7C, 0x01, 0x19, 0x08, 0x00, 0x08, 0x00, 0xAA, 0xAA, 0x80 };
    long slice = 500L * 1024;
    cabecalho(0, 0, 0);
    c = abre(0x1F43B675UL);
    u8el(0xE7UL, 0);
    sb = abre(0xA3UL);
    { unsigned char h[4] = { 0x81, 0, 0, 0x80 }; bytes(h, 4); }   // faixa 1, tempo 0, chave
    dados = N;
    B[N++] = (unsigned char)(slice >> 24); B[N++] = (unsigned char)(slice >> 16);
    B[N++] = (unsigned char)(slice >> 8); B[N++] = (unsigned char)slice;
    B[N] = 0x26; B[N + 1] = 0x01; for (i = 2; i < slice; i++) B[N + i] = 0x5A; N += slice;   // IDR
    bytes(rpu, sizeof rpu);
    fimQ = N;
    fecha(sb); fecha(c);
    k = mkv_faixas_do_trecho(B, JANELA, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
    ok(k == 2 && d->rpu == -1 && d->blocoIni == dados && d->blocoFim == fimQ,
       "janela corta o quadro: indeciso, [ini,fim) do quadro para o Range");
    ok(mkv_quadro_tem_rpu(B + dados, fimQ - dados, 4, NULL, NULL) == 1, "o quadro inteiro tem NAL 62 no fim");
    k = mkv_faixas_do_trecho(B, N, fx, MKV_MAX_FAIXAS, NULL, 0, NULL); d = mkv_diag();
    ok(d->rpu == 1 && d->rpuTipo == 2 && d->rpuPerfil == 1, "arquivo inteiro: rpu=1 tipo 2 vdr_rpu_profile 1"); }

  printf("(b) MKV real, RPU so em banda, sem dvcC\n");
  { long n = 0; unsigned char *f = lerArq(argv[1], &n);
    long w = n < JANELA ? n : JANELA;
    k = f ? mkv_faixas_do_trecho(f, w, fx, MKV_MAX_FAIXAS, NULL, 0, NULL) : 0; d = mkv_diag();
    ok(k == 2 && fx[0].tipo == 1 && fx[0].dvPerfil == 0 && fx[0].bamN == 0 && fx[0].nalTam == 4, "2 faixas, hvcC, nenhum BlockAdditionMapping");
    ok(d->rpu == 1 && d->quadros >= 1 && d->rpuTipo == 2 && d->rpuPerfil == 1, "rpu_banda=sim, rpu_tipo 2, vdr_rpu_profile 1");
    free(f); }

  printf("(c) MKV real, HDR10 puro\n");
  { long n = 0; unsigned char *f = lerArq(argv[2], &n);
    long w = n < JANELA ? n : JANELA;
    k = f ? mkv_faixas_do_trecho(f, w, fx, MKV_MAX_FAIXAS, NULL, 0, NULL) : 0; d = mkv_diag();
    ok(k == 2 && fx[0].dvPerfil == 0 && fx[0].bamN == 0 && fx[0].nalTam == 4, "2 faixas, hvcC, sem dvcC");
    ok(d->rpu == 0 && d->quadros >= 1, "rpu_banda=nao (quadro inteiro varrido)");
    free(f); }

  printf(falhas ? "mkv_dvrpu: %d FALHA(S)\n" : "mkv_dvrpu: tudo ok\n", falhas);
  return falhas != 0;
}
