// Ver tests/mkvcore_paridade.sh (205-rust-mkvcore).
// src/mkv.c compilado DUAS vezes: ref_* = caminho C de hoje, rs_* = nucleo Rust
// (-DNV_RUST_MKVCORE). Cada entrada vai para os dois e os CAMPOS sao comparados.
// Toda entrada e todo buffer de saida ficam colados numa pagina PROT_NONE: leitura
// ou escrita um byte alem (C ou Rust) vira SIGSEGV, nao um resultado "parecido".
#include "../src/mkv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define DECL(P) \
  const MkvDiag *P##mkv_diag(void); \
  int P##mkv_quadro_tem_rpu(const unsigned char *, long, int, int *, int *); \
  int P##mkv_contentor(const unsigned char *, long); \
  int P##mkv_faixas_do_trecho(const unsigned char *, long, MkvFaixa *, int, MkvCap *, int, int *);
DECL(ref_)
DECL(rs_)

static int falhas, casos;
static FILE *out;

// --- memoria com guarda -------------------------------------------------------
// Devolve p tal que [p, p+n) termina colado numa pagina PROT_NONE e ha outra
// antes do inicio da regiao.
static void *guardada(size_t n) {
  size_t pg = (size_t)sysconf(_SC_PAGESIZE);
  size_t pgs = (n + pg - 1) / pg + 1;            // paginas de dados
  unsigned char *r = mmap(NULL, (pgs + 2) * pg, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (r == MAP_FAILED) { perror("mmap"); exit(2); }
  mprotect(r, pg, PROT_NONE);
  mprotect(r + (pgs + 1) * pg, pg, PROT_NONE);
  return r + (pgs + 1) * pg - n;
}

// --- construtor de MKV --------------------------------------------------------
typedef struct { unsigned char *b; long n, cap; } Buf;
static void put(Buf *B, const void *d, long k) {
  if (B->n + k > B->cap) { B->cap = (B->n + k) * 2 + 64; B->b = realloc(B->b, (size_t)B->cap); }
  memcpy(B->b + B->n, d, (size_t)k); B->n += k;
}
static void byte(Buf *B, unsigned v) { unsigned char c = (unsigned char)v; put(B, &c, 1); }
static void idw(Buf *B, unsigned long id) {
  int w = id > 0xFFFFFF ? 4 : id > 0xFFFF ? 3 : id > 0xFF ? 2 : 1, i;
  for (i = w - 1; i >= 0; i--) byte(B, (unsigned)(id >> (8 * i)));
}
// tamanho de largura w (1..8), valor v; v < 0 = todos os bits em 1 (desconhecido)
static void vint(Buf *B, long long v, int w) {
  int i;
  unsigned long long u = v < 0 ? ~0ULL : (unsigned long long)v;
  for (i = w - 1; i >= 0; i--) {
    unsigned b = (unsigned)((u >> (8 * i)) & 0xFF);
    if (i == w - 1) b = (v < 0 ? 0xFF >> w : (b & (0xFF >> w))) | (0x80 >> (w - 1));
    byte(B, b);
  }
}
static void el(Buf *B, unsigned long id, const void *d, long k, int w) { idw(B, id); vint(B, k, w); put(B, d, k); }
static void eluint(Buf *B, unsigned long id, unsigned long long v) {
  unsigned char t[8]; int i;
  for (i = 0; i < 8; i++) t[i] = (unsigned char)(v >> (8 * (7 - i)));
  el(B, id, t, 8, 1);
}
static void elstr(Buf *B, unsigned long id, const char *s) { el(B, id, s, (long)strlen(s), 1); }
// contentor: abre com 8 bytes de tamanho, fecha pondo o tamanho real
static long abre(Buf *B, unsigned long id) { idw(B, id); vint(B, 0, 8); return B->n; }
static void fecha(Buf *B, long ini) {
  long long k = B->n - ini; int i;
  for (i = 0; i < 7; i++) B->b[ini - 1 - i] = (unsigned char)(k >> (8 * i));
  B->b[ini - 8] = 0x01;
}

// MKV valido: header EBML, Segment{SeekHead, Tracks{video hvcC+dvcC, audio, legenda},
// Chapters{Edition{2 atomos}}, Cluster{SimpleBlock(NAL 62), BlockGroup{Block}}}
static void montaHvcc(unsigned char *h, int nalTam, int nal62) {
  memset(h, 0, 32); h[0] = 1; h[21] = (unsigned char)(0xFC | (nalTam - 1)); h[22] = 1;
  h[23] = nal62 ? 0x3E : 0x20; h[24] = 0; h[25] = 1; h[26] = 0; h[27] = 2; h[28] = 0x40; h[29] = 0x01;
}
static Buf valido(void) {
  Buf B = { 0, 0, 0 };
  long e, s, t, a, c, ed, at, cl, bg;
  unsigned char hv[32], dv[24], frame[64], blk[80];
  int k;
  long posCap, posSeek;
  e = abre(&B, 0x1A45DFA3); elstr(&B, 0x4282, "matroska"); fecha(&B, e);
  s = abre(&B, 0x18538067);
  { long sh = abre(&B, 0x114D9B74), sk = abre(&B, 0x4DBB);
    el(&B, 0x53AB, "\x10\x43\xA7\x70", 4, 1);
    idw(&B, 0x53AC); vint(&B, 4, 1); posSeek = B.n; put(&B, "\0\0\0\0", 4);
    fecha(&B, sk); fecha(&B, sh); }
  t = abre(&B, 0x1654AE6B);
  a = abre(&B, 0xAE);
  eluint(&B, 0xD7, 1); eluint(&B, 0x83, 1); elstr(&B, 0x86, "V_MPEGH/ISO/HEVC");
  montaHvcc(hv, 4, 1); el(&B, 0x63A2, hv, 32, 1);
  { long m = abre(&B, 0x41E4);
    eluint(&B, 0x41E7, 0x64766343UL);
    memset(dv, 0, sizeof dv); dv[0] = 1; dv[2] = (unsigned char)((8 << 1) | 0); dv[3] = (unsigned char)((6 << 3) | 4 | 1);
    dv[4] = 0x10;
    el(&B, 0x41ED, dv, 24, 1); fecha(&B, m); }
  // DefaultDuration (23.976 fps) and Video > Colour > TransferCharacteristics (PQ).
  eluint(&B, 0x23E383, 41708333ULL);
  { long v = abre(&B, 0xE0), co = abre(&B, 0x55B0);
    eluint(&B, 0x55BA, 16); fecha(&B, co); fecha(&B, v); }
  fecha(&B, a);
  a = abre(&B, 0xAE);
  eluint(&B, 0xD7, 2); eluint(&B, 0x83, 2); elstr(&B, 0x86, "A_EAC3"); elstr(&B, 0x22B59C, "por");
  elstr(&B, 0x536E, "Surround 5.1");
  { long au = abre(&B, 0xE1); eluint(&B, 0x9F, 6); fecha(&B, au); }
  fecha(&B, a);
  a = abre(&B, 0xAE);
  eluint(&B, 0xD7, 3); eluint(&B, 0x83, 17); elstr(&B, 0x86, "S_TEXT/ASS"); elstr(&B, 0x22B59D, "pt-BR");
  eluint(&B, 0x55AA, 1); eluint(&B, 0x55AB, 0);
  fecha(&B, a);
  fecha(&B, t);
  posCap = B.n;
  c = abre(&B, 0x1043A770);
  ed = abre(&B, 0x45B9);
  for (k = 0; k < 2; k++) {
    at = abre(&B, 0xB6); eluint(&B, 0x91, 5000000000ULL * (unsigned)(k + 1));
    { long d = abre(&B, 0x80); elstr(&B, 0x85, k ? "End Credits" : "Intro"); fecha(&B, d); }
    fecha(&B, at);
  }
  fecha(&B, ed); fecha(&B, c);
  // quadro: [len4=5][NAL 40 01 aa bb cc] [len4=20][NAL 7C 01 19 08 00 08 00 aa...]
  { unsigned char *f = frame; long fn = 0;
    f[fn++] = 0; f[fn++] = 0; f[fn++] = 0; f[fn++] = 5;
    f[fn++] = 0x40; f[fn++] = 0x01; f[fn++] = 0xAA; f[fn++] = 0xBB; f[fn++] = 0xCC;
    f[fn++] = 0; f[fn++] = 0; f[fn++] = 0; f[fn++] = 10;
    f[fn++] = 0x7C; f[fn++] = 0x01; f[fn++] = 0x19; f[fn++] = 0x08; f[fn++] = 0x00; f[fn++] = 0x08;
    f[fn++] = 0x00; f[fn++] = 0xAA; f[fn++] = 0xAA; f[fn++] = 0x80;
    blk[0] = 0x81; blk[1] = 0; blk[2] = 0; blk[3] = 0x80; memcpy(blk + 4, f, (size_t)fn);
    cl = abre(&B, 0x1F43B675);
    eluint(&B, 0xE7, 0);
    el(&B, 0xA3, blk, 4 + fn, 1);
    bg = abre(&B, 0xA0); el(&B, 0xA1, blk, 4 + fn, 1); fecha(&B, bg);
    fecha(&B, cl);
  }
  fecha(&B, s);
  // SeekPosition: relativo ao inicio dos dados do Segment
  { long segIni = e + 0; long seg0; unsigned long long rel;
    // dados do Segment comecam logo apos seu cabecalho de 12 bytes (4 de id + 8)
    seg0 = s; (void)segIni;
    rel = (unsigned long long)(posCap - seg0);
    B.b[posSeek + 0] = (unsigned char)(rel >> 24); B.b[posSeek + 1] = (unsigned char)(rel >> 16);
    B.b[posSeek + 2] = (unsigned char)(rel >> 8);  B.b[posSeek + 3] = (unsigned char)rel;
  }
  return B;
}

// --- comparacao ---------------------------------------------------------------
static int mesmoFaixa(const MkvFaixa *a, const MkvFaixa *b) {
  return a->numero == b->numero && a->tipo == b->tipo && !memcmp(a->idioma, b->idioma, sizeof a->idioma) &&
         !memcmp(a->nome, b->nome, sizeof a->nome) && !memcmp(a->codec, b->codec, sizeof a->codec) &&
         a->forcado == b->forcado && a->sdh == b->sdh && a->canais == b->canais &&
         a->dvPerfil == b->dvPerfil && a->dvNivel == b->dvNivel && a->dvRpu == b->dvRpu &&
         a->dvEl == b->dvEl && a->dvBl == b->dvBl && a->dvCompat == b->dvCompat &&
         a->cpN == b->cpN && a->hvccNal62 == b->hvccNal62 && a->bamN == b->bamN &&
         a->nalTam == b->nalTam && a->entradaInteira == b->entradaInteira && a->bamTipo == b->bamTipo &&
         a->quadroNs == b->quadroNs && a->transferencia == b->transferencia;
}
static int mesmoCap(const MkvCap *a, const MkvCap *b) {
  return a->inicio == b->inicio && !memcmp(a->nome, b->nome, sizeof a->nome);
}
static int mesmoDiag(const MkvDiag *a, const MkvDiag *b) {
  return a->lidos == b->lidos && a->tracksAchado == b->tracksAchado && a->tracksInteiro == b->tracksInteiro &&
         a->tracksTam == b->tracksTam && a->rpu == b->rpu && a->quadros == b->quadros &&
         a->rpuTipo == b->rpuTipo && a->rpuPerfil == b->rpuPerfil &&
         a->blocoIni == b->blocoIni && a->blocoFim == b->blocoFim;
}

static void falha(const char *nome, const char *o) {
  falhas++;
  fprintf(out, "FALHOU %s: %s\n", nome, o);
}

// Um caso: o mesmo buffer, mesmos limites, nos dois caminhos.
// caps: 0 = NULL, 1 = com maxCaps, 2 = com maxCaps 0.
static void compara(const char *nome, const unsigned char *src, long n, int max, int modoCaps, int maxCaps) {
  unsigned char *in = n > 0 ? guardada((size_t)n) : NULL;
  MkvFaixa *fa = guardada(sizeof(MkvFaixa) * (size_t)max), *fb = guardada(sizeof(MkvFaixa) * (size_t)max);
  size_t nc = (size_t)(maxCaps > 0 ? maxCaps : 1);
  MkvCap *ca = guardada(sizeof(MkvCap) * nc), *cb = guardada(sizeof(MkvCap) * nc);
  int na = -9, nb = -9, ra, rb, i;
  MkvDiag da, db;
  if (n > 0) memcpy(in, src, (size_t)n);
  memset(fa, 0xA5, sizeof(MkvFaixa) * (size_t)max); memset(fb, 0xA5, sizeof(MkvFaixa) * (size_t)max);
  memset(ca, 0xA5, sizeof(MkvCap) * nc); memset(cb, 0xA5, sizeof(MkvCap) * nc);
  casos++;
  ra = ref_mkv_faixas_do_trecho(in, n, fa, max, modoCaps ? ca : NULL, modoCaps == 2 ? 0 : maxCaps, &na);
  rb = rs_mkv_faixas_do_trecho(in, n, fb, max, modoCaps ? cb : NULL, modoCaps == 2 ? 0 : maxCaps, &nb);
  da = *ref_mkv_diag(); db = *rs_mkv_diag();
  if (ra != rb) { char m[96]; snprintf(m, sizeof m, "faixas C=%d Rust=%d (n=%ld)", ra, rb, n); falha(nome, m); return; }
  if (na != nb) { char m[96]; snprintf(m, sizeof m, "nCaps C=%d Rust=%d (n=%ld)", na, nb, n); falha(nome, m); return; }
  for (i = 0; i < max; i++)
    if (!mesmoFaixa(&fa[i], &fb[i])) { char m[96]; snprintf(m, sizeof m, "faixa %d difere (n=%ld)", i, n); falha(nome, m); return; }
  for (i = 0; i < (int)nc; i++)
    if (!mesmoCap(&ca[i], &cb[i])) { char m[96]; snprintf(m, sizeof m, "cap %d difere (n=%ld)", i, n); falha(nome, m); return; }
  if (!mesmoDiag(&da, &db)) {
    char m[160];
    snprintf(m, sizeof m, "diag difere (n=%ld) C{rpu=%d q=%d ini=%lld fim=%lld tk=%d/%ld} Rust{rpu=%d q=%d ini=%lld fim=%lld tk=%d/%ld}",
             n, da.rpu, da.quadros, da.blocoIni, da.blocoFim, da.tracksAchado, da.tracksTam,
             db.rpu, db.quadros, db.blocoIni, db.blocoFim, db.tracksAchado, db.tracksTam);
    falha(nome, m); return;
  }
  if (ref_mkv_contentor(in, n) != rs_mkv_contentor(in, n)) { falha(nome, "contentor difere"); return; }
}

static void comparaRpu(const char *nome, const unsigned char *src, long n, int nalTam) {
  unsigned char *in = n > 0 ? guardada((size_t)n) : NULL;
  int ta = -7, pa = -7, tb = -7, pb = -7, ra, rb;
  if (n > 0) memcpy(in, src, (size_t)n);
  casos++;
  ra = ref_mkv_quadro_tem_rpu(in, n, nalTam, &ta, &pa);
  rb = rs_mkv_quadro_tem_rpu(in, n, nalTam, &tb, &pb);
  if (ra != rb || ta != tb || pa != pb) {
    char m[128];
    snprintf(m, sizeof m, "rpu C=%d(%d,%d) Rust=%d(%d,%d) n=%ld nalTam=%d", ra, ta, pa, rb, tb, pb, n, nalTam);
    falha(nome, m);
  }
  ra = ref_mkv_quadro_tem_rpu(in, n, nalTam, NULL, NULL);
  rb = rs_mkv_quadro_tem_rpu(in, n, nalTam, NULL, NULL);
  if (ra != rb) falha(nome, "rpu sem ponteiros de saida");
}

// --- fuzz: gerador fixo (LCG), sem dependencia de tempo ------------------------
static uint64_t seed = 0x205C0DE;
static unsigned rnd(void) { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; return (unsigned)(seed >> 33); }

int main(int argc, char **argv) {
  int iter = argc > 1 ? atoi(argv[1]) : 4000, i, j;
  Buf V = valido();
  static const struct { const char *nome; long long tam; int w; } grandes[] = {
    { "tam 2GiB (4 bytes)", 0x80000000LL, 5 }, { "tam 4GiB-1", 0xFFFFFFFFLL, 5 },
    { "tam 2^40", 1LL << 40, 6 }, { "tam 2^56-2", (1LL << 56) - 2, 8 }, { "tam desconhecido", -1, 8 },
    { "tam desconhecido 1 byte", -1, 1 }, { "tam 0", 0, 8 }, { "tam 1", 1, 1 },
  };
  out = stderr;
  freopen("/dev/null", "w", stdout);          // os [mkv] ... do modulo
  // 1) valido, todos os limites e modos de caps
  { int mx[] = { 1, 2, 3, 4, 64 }, mc[] = { 1, 2, 64 };
    for (i = 0; i < 5; i++) for (j = 0; j < 3; j++) {
      compara("valido sem caps", V.b, V.n, mx[i], 0, mc[j]);
      compara("valido com caps", V.b, V.n, mx[i], 1, mc[j]);
      compara("valido cap=0", V.b, V.n, mx[i], 2, mc[j]);
    }
    compara("valido maxCaps 0 com ponteiro", V.b, V.n, 8, 1, 0);
  }
  // 2) toda janela (truncamento) do valido
  for (i = 0; i <= V.n; i++) { compara("truncado", V.b, i, 8, 1, 8); compara("truncado sem caps", V.b, i, 8, 0, 0); }
  // 3) tamanhos enormes no lugar do tamanho de cada elemento do valido: varre o
  // buffer procurando "id + tamanho de 8 bytes" gerado por abre() (0x01 00..)
  for (i = 0; i + 8 < V.n; i++) {
    if (V.b[i] != 0x01 || V.b[i + 1] || V.b[i + 2] || V.b[i + 3] || V.b[i + 4]) continue;
    for (j = 0; j < (int)(sizeof grandes / sizeof grandes[0]); j++) {
      Buf M = { malloc((size_t)V.n + 16), 0, V.n + 16 };
      long long v = grandes[j].tam; int k;
      memcpy(M.b, V.b, (size_t)V.n); M.n = V.n;
      // reescreve os 8 bytes do tamanho: 0x01 + 7 bytes de valor (ou todos 0xFF)
      if (v < 0) { M.b[i] = 0x01; for (k = 1; k < 8; k++) M.b[i + (long)k] = 0xFF; }
      else { M.b[i] = 0x01; for (k = 0; k < 7; k++) M.b[i + 7 - k] = (unsigned char)(v >> (8 * k)); }
      compara(grandes[j].nome, M.b, M.n, 8, 1, 8);
      compara(grandes[j].nome, M.b, M.n, 8, 0, 0);
      free(M.b);
    }
  }
  // 4) tamanhos com largura menor (1..7 bytes) em Segment: o resto continua valido
  for (j = 1; j <= 8; j++) {
    Buf M = { 0, 0, 0 };
    long e = abre(&M, 0x1A45DFA3); elstr(&M, 0x4282, "matroska"); fecha(&M, e);
    idw(&M, 0x18538067); vint(&M, j == 8 ? -1 : (1LL << (7 * j)) - 2, j);
    { long t = abre(&M, 0x1654AE6B), a = abre(&M, 0xAE);
      eluint(&M, 0xD7, 1); eluint(&M, 0x83, 1); elstr(&M, 0x86, "V_MPEG4/ISO/AVC");
      fecha(&M, a); fecha(&M, t); }
    while (M.n < 200) byte(&M, 0);
    compara("segment largura variavel", M.b, M.n, 8, 1, 8);
    free(M.b);
  }
  // 5) quadro HEVC: NAL invalido, comprimentos 0/1/enormes, nalTam 0..5
  { unsigned char q[64]; static const unsigned char nal62[] = { 0x7C, 0x01, 0x19, 0x08, 0x00, 0x08, 0x00, 0xAA, 0x80 };
    int nt, k;
    for (nt = -1; nt <= 5; nt++) {
      compara("rpu: vazio", q, 0, 1, 0, 0);
      comparaRpu("rpu: vazio", q, 0, nt);
      memset(q, 0, sizeof q);
      comparaRpu("rpu: so zeros", q, 64, nt);
      for (k = 0; k < nt && k < 4; k++) q[k] = 0xFF;
      comparaRpu("rpu: tamanho enorme", q, 16, nt);
      memset(q, 0, sizeof q);
      if (nt >= 1 && nt <= 4) {
        q[nt - 1] = 1; memcpy(q + nt, nal62, 2); comparaRpu("rpu: len 1", q, nt + 8, nt);
        q[nt - 1] = 0; comparaRpu("rpu: len 0", q, nt + 8, nt);
        q[nt - 1] = (unsigned char)sizeof nal62; memcpy(q + nt, nal62, sizeof nal62);
        comparaRpu("rpu: NAL 62 inteiro", q, nt + (long)sizeof nal62, nt);
        comparaRpu("rpu: NAL 62 cortado", q, nt + (long)sizeof nal62 - 3, nt);
        comparaRpu("rpu: sobra 1 byte", q, nt + (long)sizeof nal62 + 1, nt);
        // 00 00 03 (prevencao de emulacao) dentro do RPU
        q[nt + 2] = 0x00; q[nt + 3] = 0x00; q[nt + 4] = 0x03; q[nt + 5] = 0x08;
        comparaRpu("rpu: emulation prevention", q, nt + (long)sizeof nal62, nt);
      }
    }
  }
  // 6) fuzz: (a) bytes aleatorios com a assinatura EBML; (b) o valido com 1..8
  // bytes trocados; (c) o mesmo, cortado num ponto qualquer
  for (i = 0; i < iter; i++) {
    long n = 64 + (long)(rnd() % 600);
    unsigned char *r = malloc((size_t)n);
    long k;
    for (k = 0; k < n; k++) r[k] = (unsigned char)rnd();
    memcpy(r, "\x1A\x45\xDF\xA3", 4);
    if (i & 1) { // entrada com cara de Segment/Tracks para o acaso ir mais fundo
      r[4] = 0x80; r[5] = 0x18; r[6] = 0x53; r[7] = 0x80; r[8] = 0x67; r[9] = 0x01;
    }
    compara("fuzz bytes", r, n, 4, 1, 4);
    comparaRpu("fuzz rpu", r + 4, n - 4, 1 + (int)(rnd() % 4));
    free(r);
  }
  for (i = 0; i < iter * 4; i++) {
    Buf M = { malloc((size_t)V.n), V.n, V.n };
    int m = 1 + (int)(rnd() % 8), k;
    long n;
    memcpy(M.b, V.b, (size_t)V.n);
    for (k = 0; k < m; k++) M.b[rnd() % (unsigned)V.n] = (unsigned char)rnd();
    n = (i % 3 == 0) ? (long)(rnd() % (unsigned)V.n) : M.n;
    compara("fuzz mutacao", M.b, n, 1 + (int)(rnd() % 8), (int)(rnd() % 3), 1 + (int)(rnd() % 8));
    free(M.b);
  }
  fprintf(out, falhas ? "mkvcore_paridade: %d FALHA(S) em %d casos\n" : "mkvcore_paridade: %d casos, tudo igual\n",
          falhas ? falhas : casos, casos);
  free(V.b);
  return falhas != 0;
}
