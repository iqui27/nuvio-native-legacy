#include "imgsub.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_QUADROS 512
#define MAX_BYTES (48u << 20)   // PGS 1080p cheio e ~8 MB; o comum e dezenas de KB

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static ImgsubQuadro *q[MAX_QUADROS];    // ordenado por ini
static int nq;
static size_t bytes;
static unsigned proximoId;
static double ultimo;                   // ultimo `t` pedido: o que fica longe sai primeiro

static size_t pesoDe(const ImgsubQuadro *f) {
  size_t b = sizeof *f;
  int i;
  for (i = 0; i < f->n; i++) b += (size_t)f->obj[i].w * f->obj[i].h * 4;
  return b;
}

static void solta(ImgsubQuadro *f) {
  int i;
  if (--f->refs > 0) return;
  for (i = 0; i < PGS_MAX_OBJ; i++) free(f->obj[i].rgba);
  free(f);
}

static void tira(int i) {
  bytes -= pesoDe(q[i]);
  solta(q[i]);
  memmove(q + i, q + i + 1, (size_t)(nq - i - 1) * sizeof *q);
  nq--;
}

static double dist(const ImgsubQuadro *f) {
  double d = f->ini - ultimo;
  return d < 0 ? -d : d;
}

static unsigned donoGeracao;

// `dono` e a geracao da legenda que decodificou o quadro. O mais novo manda: um
// quadro de dono mais velho (coletor que saiu no meio da decodificacao) e
// descartado, e um dono mais novo expulsa os quadros antigos.
void imgsub_por(unsigned dono, double ini, double fim, PgsQuadro *src) {
  ImgsubQuadro *f = src ? calloc(1, sizeof *f) : NULL;
  int i, pos, cabe;
  if (!f) return;
  f->ini = ini; f->fim = fim; f->refs = 1;
  f->tela_w = src->tela_w; f->tela_h = src->tela_h; f->n = src->n;
  memcpy(f->obj, src->obj, sizeof f->obj);   // transfere o rgba; sem copia
  memset(src->obj, 0, sizeof src->obj);
  src->n = 0;
  pthread_mutex_lock(&trava);
  if (dono < donoGeracao) {
    pthread_mutex_unlock(&trava);
    for (i = 0; i < PGS_MAX_OBJ; i++) free(f->obj[i].rgba);
    free(f);
    return;
  }
  if (dono > donoGeracao) { while (nq) tira(nq - 1); donoGeracao = dono; }
  f->id = ++proximoId;
  for (pos = 0; pos < nq && q[pos]->ini < ini; pos++) {}
  if (pos < nq && q[pos]->ini == ini) tira(pos);   // mesma fala colhida duas vezes
  // ponytail: varre ate 512 sob a trava. So ponteiro, o bitmap ja e nosso.
  while (nq >= MAX_QUADROS || (nq && bytes + pesoDe(f) > MAX_BYTES)) {
    int longe = 0;
    for (i = 1; i < nq; i++) if (dist(q[i]) > dist(q[longe])) longe = i;
    if (dist(q[longe]) < dist(f)) break;
    tira(longe);
  }
  cabe = nq < MAX_QUADROS && (nq == 0 || bytes + pesoDe(f) <= MAX_BYTES);
  if (cabe) {
    for (pos = 0; pos < nq && q[pos]->ini < ini; pos++) {}
    memmove(q + pos + 1, q + pos, (size_t)(nq - pos) * sizeof *q);
    q[pos] = f; nq++;
    bytes += pesoDe(f);
  }
  pthread_mutex_unlock(&trava);
  if (!cabe) { int k; for (k = 0; k < PGS_MAX_OBJ; k++) free(f->obj[k].rgba); free(f); }
}

unsigned imgsub_dono_atual(void) {
  unsigned g;
  pthread_mutex_lock(&trava); g = donoGeracao; pthread_mutex_unlock(&trava);
  return g;
}

void imgsub_limpar(void) {
  pthread_mutex_lock(&trava);
  donoGeracao = 0;
  while (nq) tira(nq - 1);
  pthread_mutex_unlock(&trava);
}

ImgsubQuadro *imgsub_pegar(double t) {
  ImgsubQuadro *f = NULL;
  int i;
  pthread_mutex_lock(&trava);
  ultimo = t;
  for (i = nq - 1; i >= 0 && q[i]->ini > t; i--) {}
  if (i >= 0) {
    ImgsubQuadro *c = q[i];
    double fim = c->fim;
    // n==0 e a marca de limpar: vale ja, mesmo com o quadro anterior ainda
    // na janela. Sem fim proprio, dura ate o proximo quadro (ou o padrao).
    if (fim < 0) fim = (c->n == 0 && i + 1 < nq) ? q[i + 1]->ini : c->ini + IMGSUB_DUR_PADRAO;
    if (t < fim) { f = c; f->refs++; }
  }
  pthread_mutex_unlock(&trava);
  return f;
}

void imgsub_soltar(ImgsubQuadro *f) {
  if (!f) return;
  pthread_mutex_lock(&trava);
  solta(f);
  pthread_mutex_unlock(&trava);
}
