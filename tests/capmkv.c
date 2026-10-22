// 203-capitulos: capitulos do MKV no Android e no .tpk. Arquivo VIRTUAL servido
// por um Range falso (sem socket): cabecalho no inicio e Chapters a 5 GiB, para
// provar a leitura pelo SeekHead com posicao de 64 bits (o ARM de 32 bits do
// .tpk) e o elemento maior que a primeira janela de 64 KB.
#include "../src/capmkv.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFMAX (1024 * 1024)
typedef struct { unsigned char *b; long n; } Buf;
static Buf nova(void) { Buf b; b.b = calloc(1, BUFMAX); b.n = 0; assert(b.b); return b; }
static void bytes(Buf *b, const void *p, long n) { assert(b->n + n <= BUFMAX); memcpy(b->b + b->n, p, (size_t)n); b->n += n; }
static void id(Buf *b, unsigned long v) {
  unsigned char t[4]; int n = v > 0xFFFFFF ? 4 : v > 0xFFFF ? 3 : v > 0xFF ? 2 : 1, i;
  for (i = 0; i < n; i++) t[i] = (unsigned char)(v >> (8 * (n - 1 - i)));
  bytes(b, t, n);
}
static void tam8(Buf *b, long long v) {
  unsigned char t[8]; int i; t[0] = 0x01;
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
  unsigned char z = (unsigned char)(0x80 | strlen(s)); id(b, eid); bytes(b, &z, 1); bytes(b, s, (long)strlen(s));
}
static void mestre(Buf *b, unsigned long eid, const Buf *c) { id(b, eid); tam8(b, c->n); bytes(b, c->b, c->n); }
static void vazio(Buf *b, long n) { static unsigned char z[BUFMAX]; id(b, 0xEC); tam8(b, n); bytes(b, z, n); }

typedef struct { double seg; const char *nome; } Cap;
static Buf capitulos(const Cap *c, int n, long folga) {
  Buf ed = nova(), ch = nova();
  int i;
  for (i = 0; i < n; i++) {
    Buf a = nova(), d = nova();
    uintEl(&a, 0x91, (unsigned long long)(c[i].seg * 1e9), 8);
    strEl(&d, 0x85, c[i].nome);
    mestre(&a, 0x80, &d);
    mestre(&ed, 0xB6, &a);
    free(a.b); free(d.b);
  }
  if (folga) vazio(&ch, folga);
  mestre(&ch, 0x45B9, &ed);
  { Buf r = nova(); mestre(&r, 0x1043A770UL, &ch); free(ed.b); free(ch.b); return r; }
}

// Arquivo virtual: [0,headN) = cabecalho; [capPos, capPos+capN) = Chapters.
static Buf head, chap; static long long capPos;
static int pedidos, falhar503;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

// cabecalho: EBML + Segment(tam desconhecido-ish 8B) + SeekHead + Info + Tracks + Void (+ Chapters opcional)
static void montar(int chapNaJanela, long long pos, const Cap *c, int nc, long folga, int seekDiz) {
  Buf eb = nova(), sh = nova(), tr = nova(), info = nova(), te = nova(), v = nova();
  free(head.b); free(chap.b);
  head = nova(); chap = capitulos(c, nc, folga); capPos = pos;
  uintEl(&eb, 0x4286, 1, 1); strEl(&eb, 0x4282, "matroska");
  mestre(&head, 0x1A45DFA3UL, &eb);
  id(&head, 0x18538067UL); tam8(&head, 0x00FFFFFFFFFFFFLL);    // Segment (tamanho irrelevante)
  { long segIni = head.n;
    if (seekDiz) {
      Buf s = nova(), z = nova();
      id(&s, 0x53AB); { unsigned char l = 0x84; bytes(&s, &l, 1); unsigned char k[4] = { 0x10, 0x43, 0xA7, 0x70 }; bytes(&s, k, 4); }
      uintEl(&s, 0x53AC, (unsigned long long)(chapNaJanela ? (segIni + 400 + 0) : pos - segIni), 8);
      mestre(&sh, 0x4DBB, &s); free(s.b); free(z.b);
      mestre(&head, 0x114D9B74UL, &sh);
    } }
  uintEl(&info, 0x2AD7B1, 1000000, 4); mestre(&head, 0x1549A966UL, &info);
  uintEl(&v, 0xD7, 1, 1); uintEl(&v, 0x83, 1, 1); strEl(&v, 0x86, "V_MPEG4/ISO/AVC");
  mestre(&te, 0xAE, &v); mestre(&head, 0x1654AE6BUL, &te);
  if (chapNaJanela) bytes(&head, chap.b, chap.n);
  else vazio(&head, 330 * 1024);                                // empurra tudo para alem de 320 KB
  free(eb.b); free(sh.b); free(tr.b); free(info.b); free(te.b); free(v.b);
}

char *rede_baixar_trecho_st(const char *url, int s, long long ini, long long fim, long *tam,
                            int *status, int *erro, char *final, unsigned tamFinal) {
  long n; char *p; (void)s; (void)url; (void)final; (void)tamFinal;
  *tam = 0; if (status) *status = 0; if (erro) *erro = 0;
  pthread_mutex_lock(&trava); pedidos++;
  if (ini > 0 && falhar503 > 0) { falhar503--; pthread_mutex_unlock(&trava); if (status) *status = 503; return NULL; }
  pthread_mutex_unlock(&trava);
  if (ini < head.n) {
    if (fim >= head.n) fim = head.n - 1;
    n = (long)(fim - ini + 1); p = calloc(1, (size_t)n + 1); memcpy(p, head.b + ini, (size_t)n);
  } else if (ini >= capPos && ini < capPos + chap.n) {
    if (fim >= capPos + chap.n) fim = capPos + chap.n - 1;
    n = (long)(fim - ini + 1); p = calloc(1, (size_t)n + 1); memcpy(p, chap.b + (ini - capPos), (size_t)n);
  } else { if (status) *status = 416; return NULL; }
  *tam = n; if (status) *status = 206; return p;
}
char *rede_baixar_trecho(const char *u, int s, long i, long f, long *t) { (void)u; (void)s; (void)i; (void)f; *t = 0; return NULL; }
static unsigned char *prebusca; static long prebuscaN;
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url;
  if (!prebusca) return 0;
  if (buf) { *buf = malloc((size_t)prebuscaN); memcpy(*buf, prebusca, (size_t)prebuscaN); }
  if (n) *n = prebuscaN;
  return 1;
}
static IntroTrecho gravado[8]; static _Atomic int nGravado, chamadas;   // escritos pelo fio lateral
void intro_definir_capitulos(const IntroTrecho *v, int n) {
  chamadas++; nGravado = n;
  if (n > 0) memcpy(gravado, v, (size_t)n * sizeof *v);
}

static int falhas;

// Barreira do fio lateral: para depois da leitura, antes da publicacao.
static pthread_mutex_t bt = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t bc = PTHREAD_COND_INITIALIZER;
static int chegou, solta;
static void pausa(void) {
  pthread_mutex_lock(&bt); chegou = 1; pthread_cond_broadcast(&bc);
  while (!solta) pthread_cond_wait(&bc, &bt);
  pthread_mutex_unlock(&bt);
}
static void ok(int c, const char *o) { printf("  %-62s %s\n", o, c ? "ok" : "FALHOU"); if (!c) falhas++; }
static int perto(double a, double b) { return a > b - 0.01 && a < b + 0.01; }

int main(void) {
  static const Cap anime[] = { {0, "Prologue"}, {90, "Opening"}, {180, "Part A"}, {1260, "Ending"}, {1350, "Next Episode Preview"} };
  static const Cap filme[] = { {0, "Chapter 1"}, {3000, "Chapter 2"}, {7000, "End Credits"} };
  static const Cap semNome[] = { {0, "Cap 1"}, {600, "Cap 2"}, {1300, "Cap 3"} };
  MkvCap caps[MKV_MAX_CAPS]; int n, antes;
  IntroTrecho t[4];
  capmkv_espera_inicial_ms = 0;

  puts("capmkv: Chapters na janela do cabecalho");
  montar(1, 0, anime, 5, 0, 1);
  n = capmkv_ler_agora("https://x.test/a.mkv", caps, MKV_MAX_CAPS, 0);
  ok(n == 5, "5 capitulos lidos da janela");
  ok(pedidos == 1, "um Range so (a janela)");
  ok(perto(caps[3].inicio, 1260) && !strcmp(caps[3].nome, "Ending"), "ChapterTimeStart em ns -> 1260 s");

  puts("capmkv: Chapters a 5 GiB, achados pelo SeekHead (elemento > 64 KB)");
  pedidos = 0;
  montar(0, 5LL * 1024 * 1024 * 1024, anime, 5, 100 * 1024, 1);
  n = capmkv_ler_agora("https://x.test/b.mkv", caps, MKV_MAX_CAPS, 0);
  ok(n == 5, "5 capitulos lidos alem da janela");
  ok(pedidos == 3, "janela + Range de 64 KB + Range do tamanho exato");
  ok(perto(caps[4].inicio, 1350), "posicao de 64 bits sem dar a volta");

  puts("capmkv: pre-busca do mkvass serve o cabecalho sem rede");
  pedidos = 0; prebusca = head.b; prebuscaN = head.n;
  n = capmkv_ler_agora("https://x.test/b.mkv", caps, MKV_MAX_CAPS, 0);
  ok(n == 5 && pedidos == 2, "sem a janela: so os Range do Chapters");
  prebusca = NULL;

  puts("capmkv: CDN recusa o Range extra (503), recua e acerta");
  pedidos = 0; falhar503 = 1;
  n = capmkv_ler_agora("https://x.test/b.mkv", caps, MKV_MAX_CAPS, 0);
  ok(n == 5, "segunda tentativa leu os capitulos");

  puts("capmkv: SeekHead nao aponta Chapters e eles estao fora da janela");
  pedidos = 0;
  montar(0, 5LL * 1024 * 1024 * 1024, anime, 5, 0, 0);
  n = capmkv_ler_agora("https://x.test/c.mkv", caps, MKV_MAX_CAPS, 0);
  ok(n == 0, "0 capitulos, sem chutar posicao");
  ok(pedidos == 1, "nenhum Range extra");

  puts("capmkv: regras de nome -> trechos (as mesmas da LG)");
  montar(1, 0, anime, 5, 0, 1);
  capmkv_ler_agora("https://x.test/a.mkv", caps, MKV_MAX_CAPS, 0);
  n = capmkv_trechos(caps, 5, t, 4);
  ok(n == 3, "abertura + creditos + previa");
  ok(t[0].tipo == INTRO_ABERTURA && perto(t[0].inicio, 90) && perto(t[0].fim, 180), "Opening 90-180 s");
  ok(t[1].tipo == INTRO_CREDITOS && perto(t[1].inicio, 1260) && perto(t[1].fim, 1350), "Ending 1260 s ate a previa");
  ok(t[2].tipo == INTRO_PREVIA && perto(t[2].inicio, 1350), "Preview nao e credito: so marca o fim");
  capmkv_aplicar(caps, 5);
  ok(chamadas == 1 && nGravado == 3, "alimenta o modulo de intro");
  ok(perto(capmkv_creditos(1400), 1260), "creditos = Ending (nome vale sem duracao)");

  { MkvCap fm[3] = { {0, "Chapter 1"}, {3000, "Chapter 2"}, {7000, "End Credits"} };
    n = capmkv_trechos(fm, 3, t, 4);
    ok(n == 1 && t[0].tipo == INTRO_CREDITOS && t[0].fim == 0.0, "filme: End Credits ate o fim, sem previa"); }
  { MkvCap sn[3] = { {0, "Cap 1"}, {600, "Cap 2"}, {1300, "Cap 3"} };
    n = capmkv_trechos(sn, 3, t, 4);
    ok(n == 0, "sem nome util: nenhum trecho (nada de chute)");
    capmkv_aplicar(sn, 3);
    ok(capmkv_creditos(0) == 0.0, "ultimo capitulo sem duracao: nao vale");
    ok(capmkv_creditos(1400) > 1299 && capmkv_creditos(1400) < 1301, "ultimo capitulo no ultimo quarto vale");
    ok(capmkv_creditos(2400) == 0.0, "ultimo capitulo antes dos 75% nao vale"); }
  (void)filme; (void)semNome;

  puts("capmkv: fio lateral e cache por URL");
  montar(0, 5LL * 1024 * 1024 * 1024, anime, 5, 0, 1);
  pedidos = 0; chamadas = 0;
  capmkv_iniciar("https://x.test/d.mkv");
  for (int i = 0; i < 300 && !nGravado; i++) usleep(10000);
  usleep(50000);
  ok(nGravado == 3, "fio entregou os trechos ao modulo de intro");
  antes = pedidos;
  capmkv_iniciar("https://x.test/d.mkv");
  usleep(100000);
  ok(pedidos == antes, "mesma URL de novo: cache, nenhum pedido");
  ok(perto(capmkv_creditos(1400), 1260), "e os creditos continuam la");
  pedidos = 0;
  capmkv_iniciar("https://x.test/filme.mp4");
  usleep(100000);
  ok(pedidos == 0, "MP4 nao tem capitulo Matroska: nem pede");

  // Revisao 2.0.3 (achado 8): o fio de A passa da checagem de geracao, o
  // usuario troca para B (sem capitulos) e A publica em cima de B.
  puts("capmkv: fio de A atrasado nao publica capitulos em B");
  montar(0, 5LL * 1024 * 1024 * 1024, anime, 5, 0, 1);
  capmkv_teste_antes_de_publicar = pausa;
  capmkv_iniciar("https://x.test/a-atrasado.mkv");
  pthread_mutex_lock(&bt);
  while (!chegou) pthread_cond_wait(&bc, &bt);
  pthread_mutex_unlock(&bt);
  capmkv_iniciar("https://x.test/b-sem-capitulos.mp4");   // B: zera, sem fio
  chamadas = 0; nGravado = 0;
  pthread_mutex_lock(&bt); solta = 1; pthread_cond_broadcast(&bc); pthread_mutex_unlock(&bt);
  usleep(300000);
  capmkv_teste_antes_de_publicar = NULL;
  ok(capmkv_creditos(1400) == 0.0, "B sem creditos vindos de A");
  ok(nGravado == 0, "B sem trechos de intro vindos de A");

  printf("%s\n", falhas ? "FALHOU" : "capmkv: ok");
  return falhas ? 1 : 0;
}
