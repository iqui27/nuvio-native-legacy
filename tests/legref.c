// Coletor independente da referencia do AutoSync (src/legref.c) contra MKV
// REAIS feitos por ffmpeg e mkvmerge (tests/legref_fixtures.sh).
//   bash tests/legref.sh            SANITIZE=1 / SANITIZE=thread
#include "legref.h"
#include "nvtmp.h"
#include "rede.h"
#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// legenda.c pede estes; o coletor nao usa rede de verdade aqui.
char *rede_baixar_bin(const char *url, int segundos, long *n) { (void)url; (void)segundos; (void)n; return NULL; }
unsigned rede_pedido_capacidades(void) { return REDE_CAP_JOB; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; memset(r, 0, sizeof *r); r->erro = REDE_INDISPONIVEL; return 0; }
void rede_resposta_limpar(RedeResposta *r) { free(r->corpo); free(r->cabecalhos); r->corpo = r->cabecalhos = NULL; }

static const char *DIR;
typedef struct {
  _Atomic int semRange, atrasoUs, travarApos;   // travarApos: segura o leitor depois de N pedidos
  _Atomic int liberar;
  pthread_mutex_t m; int pedidos; long long bytes; long long maiorPedido;
} Leitor;

static unsigned char *lerArquivo(void *u, const char *url, long long ini, long n, long *tam,
                                 int *status, int (*parar)(void *), void *pu) {
  Leitor *l = u; char caminho[700]; FILE *f; unsigned char *b; long long total;
  const char *nome = strrchr(url, '/');
  snprintf(caminho, sizeof caminho, "%s/%s", DIR, nome ? nome + 1 : url);
  pthread_mutex_lock(&l->m); l->pedidos++; if (n > l->maiorPedido) l->maiorPedido = n;
  int k = l->pedidos; pthread_mutex_unlock(&l->m);
  if (l->travarApos && k > l->travarApos) while (l->travarApos && !l->liberar && !parar(pu)) usleep(1000);
  if (l->atrasoUs) usleep((useconds_t)l->atrasoUs);
  if (parar(pu)) { *status = 0; return NULL; }
  f = fopen(caminho, "rb"); if (!f) { *status = 404; return NULL; }
  fseek(f, 0, SEEK_END); total = ftell(f);
  if (l->semRange) { ini = 0; n = (long)total; *status = 200; } else *status = 206;
  if (ini >= total) { fclose(f); *status = 416; return NULL; }
  if (ini + n > total) n = (long)(total - ini);
  b = malloc((size_t)n + 1); fseek(f, (long)ini, SEEK_SET);
  *tam = (long)fread(b, 1, (size_t)n, f); fclose(f);
  pthread_mutex_lock(&l->m); l->bytes += *tam; pthread_mutex_unlock(&l->m);
  return b;
}

static LegRefOrcamento orcamento(void) { return (LegRefOrcamento){ 12LL << 20, 6000, 0 }; }

static LegRefStatus esperar(LegRef *r, uint64_t id) {
  LegRefStatus s;
  for (int i = 0; i < 20000; i++) {
    s = legref_status(r);
    if (s.pedido == id && s.fase != LEGREF_LENDO) return s;
    usleep(1000);
  }
  assert(!"timeout");
  return s;
}

static LegendaDocumento *srt(const char *nome, uint64_t sessao) {
  char c[700]; FILE *f; long n; char *b; LegendaDocumento *d;
  LegendaDocumentoInfo i = { .sessao = sessao, .flags = LEGENDA_DOC_COMPLETO };
  snprintf(c, sizeof c, "%s/%s", DIR, nome); f = fopen(c, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n); assert(fread(b, 1, (size_t)n, f) == (size_t)n); fclose(f);
  snprintf(i.identidade, sizeof i.identidade, "%s", nome);
  d = legenda_documento_bytes(b, n, &i); free(b); return d;
}

// O documento colhido tem os MESMOS tempos (e textos, quando lidos) da
// legenda que entrou no arquivo. ass: centesimos. tempos: so o indice foi
// lido (SRT com CueDuration, MP4), o texto e um rotulo.
static void conferirIgual(LegendaDocumento *got, LegendaDocumento *esperado, int ass, int tempos) {
  int na, nb; const LegendaCue *a = legenda_documento_dados(got, &na), *b = legenda_documento_dados(esperado, &nb);
  assert(na == nb && na == 113);
  for (int i = 0; i < na; i++) {
    assert(fabs(a[i].inicio - b[i].inicio) < (ass ? 0.011 : 0.0011));
    assert(fabs(a[i].fim - b[i].fim) < (ass ? 0.011 : 0.0011));
    if (!tempos) assert(!strcmp(a[i].texto, b[i].texto));
  }
}

static LegendaDocumento *colher(LegRef *r, const char *arq, const char *idioma, const int *ex, int nex,
                                LegRefMotivo esperado, LegRefStatus *out) {
  char url[256]; uint64_t id; LegRefStatus s; LegRefOrcamento o = orcamento();
  snprintf(url, sizeof url, "https://cdn.example/x/%s", arq);
  id = legref_pedir(r, url, 7, idioma, ex, nex, &o); assert(id);
  s = esperar(r, id);
  if (s.motivo != esperado) fprintf(stderr, "%s: motivo %s, esperado %s\n", arq, legref_motivo(s.motivo), legref_motivo(esperado));
  assert(s.motivo == esperado);
  if (out) *out = s;
  return legref_tomar(r, id);
}

int main(int argc, char **argv) {
  Leitor l; memset(&l, 0, sizeof l); pthread_mutex_init(&l.m, NULL);
  DIR = argc > 1 ? argv[1] : nvt("/nv-legref");
  unsigned gerAntes = legenda_geracao();
  LegRef *r = legref_criar(lerArquivo, &l); assert(r);
  LegendaDocumento *emb = srt("emb.srt", 7), *d; LegRefStatus s;
  int casos = 0;

  // 1. ffmpeg, SRT: completo, identico, identidade opaca sem URL.
  d = colher(r, "ff.mkv", "en", NULL, 0, LEGREF_OK, &s); assert(d);
  { const LegendaDocumentoInfo *i = legenda_documento_info(d);
    assert(i->flags & LEGENDA_DOC_COMPLETO); assert(!(i->flags & LEGENDA_DOC_FORCED));
    assert(i->sessao == 7); assert(!strcmp(i->identidade, "mkv-track:2"));
    assert(!strstr(i->identidade, "http") && !strstr(i->origem, "http")); }
  conferirIgual(d, emb, 0, 1); legenda_documento_liberar(d); casos++;
  printf("ffmpeg srt: %d pedidos, %lld bytes (arquivo de ~22 MB)\n", s.pedidos, s.bytes);
  // SRT com CueDuration: so cabeca + Cues, nenhum bloco (antes: 116 Ranges).
  assert(s.feitos == 113 && s.total == 113 && s.pedidos <= 4);

  // 2. mkvmerge: o letreiro (forced, "Signs") e pulado; ingles ASS vem antes do portugues.
  d = colher(r, "mm.mkv", "en", NULL, 0, LEGREF_OK, &s); assert(d && s.faixa == 3 && !strcmp(s.codec, "S_TEXT/ASS"));
  conferirIgual(d, emb, 1, 0);
  legenda_documento_liberar(d); casos++;
  // Pedindo portugues, vem a faixa 4 (a mesma lingua primeiro).
  d = colher(r, "mm.mkv", "pt", NULL, 0, LEGREF_OK, &s); assert(d && s.faixa == 4); legenda_documento_liberar(d); casos++;
  // "Outra referencia": excluindo 3 e 4 sobra so o letreiro -> sem faixa.
  { int ex[2] = { 3, 4 }; d = colher(r, "mm.mkv", "en", ex, 2, LEGREF_SEM_FAIXA, NULL); assert(!d); casos++; }
  { int ex[1] = { 3 }; d = colher(r, "mm.mkv", "en", ex, 1, LEGREF_OK, &s); assert(d && s.faixa == 4); legenda_documento_liberar(d); casos++; }

  // 3. Recusas honestas.
  assert(!colher(r, "semcues.mkv", "en", NULL, 0, LEGREF_SEM_INDICE, NULL)); casos++;
  assert(!colher(r, "soforced.mkv", "en", NULL, 0, LEGREF_SEM_FAIXA, NULL)); casos++;
  assert(!colher(r, "video.mp4", "en", NULL, 0, LEGREF_SEM_FAIXA, NULL)); casos++;   // MP4 sem faixa de texto

  // 3b. MP4 (tx3g): so o moov, com o mdat de ~22 MB pulado pelo tamanho.
  //     moov no fim e no comeco; idioma ISO 639-2 ("por") casa com "pt".
  for (int fs = 0; fs < 2; fs++) {
    const char *arq = fs ? "txt_fs.mp4" : "txt.mp4";
    d = colher(r, arq, "en", NULL, 0, LEGREF_OK, &s); assert(d && !strcmp(s.codec, "tx3g") && !strcmp(s.idioma, "eng"));
    assert(!strncmp(legenda_documento_info(d)->identidade, "mp4-track:", 10));
    conferirIgual(d, emb, 0, 1); legenda_documento_liberar(d);
    printf("mp4 %s: %d pedidos, %lld bytes\n", arq, s.pedidos, s.bytes);
    assert(s.pedidos <= 6 && s.bytes < 2 * 1024 * 1024);
    d = colher(r, arq, "pt", NULL, 0, LEGREF_OK, &s); assert(d && !strcmp(s.idioma, "por"));
    { int n; const LegendaCue *v = legenda_documento_dados(d, &n);
      LegendaDocumento *e = srt("ext_mais2500.srt", 7); int ne; const LegendaCue *w = legenda_documento_dados(e, &ne);
      assert(n == ne && fabs(v[0].inicio - w[0].inicio) < 0.0011); legenda_documento_liberar(e); }
    legenda_documento_liberar(d);
    { int ex[2] = { 2, 3 }; assert(!colher(r, arq, "en", ex, 2, LEGREF_SEM_FAIXA, NULL)); }
    casos += 3;
  }
  assert(!colher(r, "frag.mp4", "en", NULL, 0, LEGREF_SEM_INDICE, NULL)); casos++;   // fragmentado: fora

  // 3c. Validacao cruzada (zero Range a mais: os tempos de todas estao no Cues).
  //     A preferida +1,5 s fora cai quando duas outras concordam entre si; com
  //     uma so outra nao ha como saber qual esta fora e fica a preferida.
  d = colher(r, "cruz.mkv", "en", NULL, 0, LEGREF_OK, &s); assert(d && s.faixa == 3);
  conferirIgual(d, emb, 0, 1); legenda_documento_liberar(d); casos++;
  d = colher(r, "cruz2.mkv", "en", NULL, 0, LEGREF_OK, &s); assert(d && s.faixa == 2); legenda_documento_liberar(d); casos++;
  assert(!colher(r, "video.mkv", "en", NULL, 0, LEGREF_SEM_FAIXA, NULL)); casos++;
  l.semRange = 1; assert(!colher(r, "ff.mkv", "en", NULL, 0, LEGREF_SEM_RANGE, NULL)); l.semRange = 0; casos++;

  // 4. Orcamento: bytes e pedidos (ASS: le os blocos, um Range por fala).
  { LegRefOrcamento o = { 200 * 1024, 6000, 0 }; uint64_t id = legref_pedir(r, "h://x/mm.mkv", 7, "en", NULL, 0, &o);
    s = esperar(r, id); assert(s.motivo == LEGREF_ORCAMENTO && !legref_tomar(r, id)); casos++; }
  { LegRefOrcamento o = { 12 << 20, 20, 0 }; uint64_t id = legref_pedir(r, "h://x/mm.mkv", 7, "en", NULL, 0, &o);
    s = esperar(r, id); assert(s.motivo == LEGREF_ORCAMENTO && s.pedidos <= 20 && !legref_tomar(r, id)); casos++; }

  // 5. Pausa (seek): nenhum pedido novo enquanto pausado.
  { LegRefOrcamento o = orcamento(); uint64_t id; int p0;
    l.atrasoUs = 2000;
    id = legref_pedir(r, "h://x/mm.mkv", 7, "en", NULL, 0, &o);
    usleep(30000); legref_pausar(r, 1); usleep(20000);
    pthread_mutex_lock(&l.m); p0 = l.pedidos; pthread_mutex_unlock(&l.m);
    usleep(150000);
    pthread_mutex_lock(&l.m); assert(l.pedidos <= p0 + 1); pthread_mutex_unlock(&l.m);
    assert(legref_status(r).fase == LEGREF_LENDO);
    legref_pausar(r, 0); s = esperar(r, id); assert(s.motivo == LEGREF_OK);
    d = legref_tomar(r, id); assert(d); assert(!legref_tomar(r, id)); legenda_documento_liberar(d);
    l.atrasoUs = 0; casos++; }

  // 6. Cancelamento e geracao: o pedido A preso, B substitui; A nunca entrega.
  { LegRefOrcamento o = orcamento(); uint64_t a, b;
    l.travarApos = l.pedidos + 3;   // ASS: le os blocos, fica preso no 4o Range
    a = legref_pedir(r, "h://x/mm.mkv", 7, "en", NULL, 0, &o); usleep(20000);
    b = legref_pedir(r, "h://x/mm.mkv", 8, "en", NULL, 0, &o);
    l.travarApos = 0;
    s = esperar(r, b); assert(s.motivo == LEGREF_OK && s.faixa == 3);
    assert(!legref_tomar(r, a));
    d = legref_tomar(r, b); assert(d && legenda_documento_info(d)->sessao == 8); legenda_documento_liberar(d);
    l.travarApos = l.pedidos + 2; a = legref_pedir(r, "h://x/ff.mkv", 7, "en", NULL, 0, &o); usleep(20000);
    legref_cancelar(r); l.travarApos = 0; usleep(20000);
    s = legref_status(r); assert(s.fase == LEGREF_CANCELADO && !legref_tomar(r, a)); casos++; }

  // 7. Destruir com leitura presa: join sem vazamento/corrida.
  { LegRefOrcamento o = orcamento(); l.travarApos = l.pedidos + 2;
    legref_pedir(r, "h://x/ff.mkv", 7, "en", NULL, 0, &o); usleep(20000);
    legref_destruir(r); l.travarApos = 0; casos++; }

  // 8. Nada do overlay principal mudou: mesma geracao de legenda.
  assert(legenda_geracao() == gerAntes); casos++;

  { int n, semRel; LegRefPonto *p; unsigned char lixo[8] = { 0xBB, 0x84, 0xB3, 0x81, 0x01, 0xB7, 0x80, 0 };
    n = legref_cues(lixo, 7, 2, 1e-3, &p, &semRel); assert(n == 0); free(p); casos++; }
  legenda_documento_liberar(emb);
  printf("legref: %d casos ok\n", casos);
  return 0;
}
