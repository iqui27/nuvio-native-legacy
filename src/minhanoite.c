#include "minhanoite.h"

#include <string.h>

void mn_fonte_limpar(MnFonte *f) { if (f) f->n = 0; }

static int durDe(const MnEntrada *e) {
  int d = e->durSeg > 0 ? e->durSeg
        : !strcmp(e->tipo, "series") ? MN_DUR_EPISODIO_S : MN_DUR_FILME_S;
  if (d < MN_DUR_MIN_S) d = MN_DUR_MIN_S;
  if (d > MN_DUR_MAX_S) d = MN_DUR_MAX_S;
  // Blocos em multiplos de 5 min: a grade le como grade de TV.
  return ((d + 299) / 300) * 300;
}

int mn_fonte_juntar(MnFonte *f, const MnEntrada *e) {
  int i;
  if (!f || !e || !e->id[0]) return 0;
  if (strcmp(e->tipo, "movie") && strcmp(e->tipo, "series")) return 0;
  if (!strcmp(e->tipo, "series") && e->episodio <= 0) return 0;
  for (i = 0; i < f->n; i++)
    if (!strcmp(f->e[i].id, e->id)) {
      if (e->origem < f->e[i].origem) f->e[i] = *e;
      return 1;
    }
  if (f->n >= MN_MAX) return 0;
  f->e[f->n++] = *e;
  return 1;
}

long mn_dia(time_t t, long fusoSeg) {
  long long s = (long long)t + fusoSeg;
  long long d = s / 86400;
  if (s < 0 && s % 86400) d--;
  return (long)d;
}

// FNV-1a do id misturado com o dia: a ordem muda de um dia para o outro e e a
// mesma em qualquer aparelho, sem depender da ordem em que o catalogo chegou.
static unsigned long chave(const char *id, long dia) {
  unsigned long h = 2166136261UL ^ (unsigned long)dia * 2654435761UL;
  for (; *id; id++) { h ^= (unsigned char)*id; h *= 16777619UL; h &= 0xffffffffUL; }
  h ^= h >> 15; h *= 2246822519UL; h &= 0xffffffffUL; h ^= h >> 13;
  return h & 0xffffffffUL;
}

// A ordem do dia: origem primeiro (o que esta pela metade abre a noite), e
// dentro da origem a chave do dia; o id desempata.
static int ordemDoDia(const MnFonte *f, long dia, int *ord) {
  int i, j;
  for (i = 0; i < f->n; i++) {
    int x = i;
    unsigned long kx = chave(f->e[x].id, dia);
    for (j = i; j > 0; j--) {
      const MnEntrada *a = &f->e[ord[j - 1]], *b = &f->e[x];
      unsigned long ka = chave(a->id, dia);
      int depois = a->origem != b->origem ? a->origem > b->origem
                 : ka != kx ? ka > kx : strcmp(a->id, b->id) > 0;
      if (!depois) break;
      ord[j] = ord[j - 1];
    }
    ord[j] = x;
  }
  return f->n;
}

// O bloco que cobre o segundo `seg` do dia `dia`. O dia e uma volta continua
// pela ordem dele; a meia-noite corta o bloco e o dia seguinte abre com a
// propria ordem.
static int bloco(const MnFonte *f, long dia, long fusoSeg, long seg, MnPrograma *p) {
  int ord[MN_MAX], n, i, d;
  long ciclo = 0, pos, ini, fim;
  double fr;
  time_t base = (time_t)((long long)dia * 86400 - fusoSeg);
  if (!f || f->n < 1 || f->n > MN_MAX || !p) return 0;
  n = ordemDoDia(f, dia, ord);
  for (i = 0; i < n; i++) ciclo += durDe(&f->e[ord[i]]);
  pos = seg % ciclo;
  ini = seg - pos;
  for (i = 0; i < n - 1; i++) {
    d = durDe(&f->e[ord[i]]);
    if (pos < d) break;
    pos -= d; ini += d;
  }
  d = durDe(&f->e[ord[i]]);
  fim = ini + d;
  if (fim > 86400) fim = 86400;
  fr = (double)(seg - ini) / (double)d;
  p->indice = ord[i];
  p->ini = base + ini;
  p->fim = base + fim;
  p->fracao = fr < 0.0 ? 0.0 : fr > MN_ENTRADA_MAX ? MN_ENTRADA_MAX : fr;
  return 1;
}

int mn_programa(const MnFonte *f, time_t t, long fusoSeg, MnPrograma *p) {
  long dia = mn_dia(t, fusoSeg);
  long seg = (long)((long long)t + fusoSeg - (long long)dia * 86400);
  return bloco(f, dia, fusoSeg, seg, p);
}

int mn_proximo(const MnFonte *f, time_t t, long fusoSeg, int k, MnPrograma *p) {
  long dia = mn_dia(t, fusoSeg);
  long seg = (long)((long long)t + fusoSeg - (long long)dia * 86400);
  MnPrograma a;
  if (k < 0 || !bloco(f, dia, fusoSeg, seg, &a)) return 0;
  // Anda bloco a bloco pelo instante de inicio: atravessa a meia-noite sem
  // precisar conhecer a ordem do dia seguinte aqui.
  for (;;) {
    time_t prox = a.fim;
    dia = mn_dia(prox, fusoSeg);
    seg = (long)((long long)prox + fusoSeg - (long long)dia * 86400);
    if (!bloco(f, dia, fusoSeg, seg, &a)) return 0;
    a.fracao = 0.0;
    if (k-- == 0) { *p = a; return 1; }
  }
}

int mn_zap(int naMinha, int temMinha, int daVolta) {
  if (naMinha) return MN_ZAP_PONTA;
  return temMinha && daVolta ? MN_ZAP_MINHA : MN_ZAP_IPTV;
}

int mn_conta(int passiva, int gesto) {
  if (!passiva) return 1;
  return gesto == MN_GESTO_DO_INICIO || gesto == MN_GESTO_DAQUI;
}
