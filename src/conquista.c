// Motor puro das conquistas. Ver conquista.h.
#include "conquista.h"
#include <string.h>

int conq_degraus(int vistos, int total, int emOrdem) {
  int m = 0, faltam;
  if (total <= 0 || vistos <= 0) return 0;
  if (vistos > total) vistos = total;
  faltam = total - vistos;
  m |= CONQ_PRIMEIRO;
  if (vistos * 2 >= total) m |= CONQ_METADE;
  // "Quase la": falta um, ou 80% quando o alvo e grande (13 filmes de Kubrick:
  // 11 ja e quase). Num alvo de 2, um visto ja e metade E quase: e verdade.
  if (faltam <= 1 || (total > 10 && vistos * 5 >= total * 4)) m |= CONQ_QUASE;
  if (faltam == 0) {
    m |= CONQ_COMPLETA;
    if (emOrdem) m |= CONQ_ORDEM;
  }
  return m;
}

int conq_maior(int degraus) {
  int b;
  for (b = CONQ_N_DEGRAUS - 1; b >= 0; b--) if (degraus & (1 << b)) return 1 << b;
  return 0;
}

int conq_indice(int degrau) {
  int b;
  for (b = 0; b < CONQ_N_DEGRAUS; b++) if (degrau == (1 << b)) return b;
  return -1;
}

int conq_em_ordem(const ConqItem *v, int n) {
  int i, j;
  if (!v || n <= 0) return 0;
  for (i = 0; i < n; i++) if (!v[i].visto || v[i].quando <= 0) return 0;
  // O(n^2) de proposito: n e uma filmografia (< 100) e a regra fica literal.
  for (i = 0; i < n; i++)
    for (j = 0; j < n; j++)
      if (v[i].ordem < v[j].ordem && v[i].quando > v[j].quando) return 0;
  return 1;
}

ConqProgresso conq_calcular(const ConqItem *v, int n) {
  ConqProgresso p;
  int i, ordem;
  memset(&p, 0, sizeof p);
  if (!v || n <= 0) return p;
  p.total = n;
  for (i = 0; i < n; i++) if (v[i].visto) p.vistos++;
  ordem = p.vistos == n ? conq_em_ordem(v, n) : 0;
  p.degraus = conq_degraus(p.vistos, p.total, ordem);
  return p;
}

int conq_faltam(const ConqProgresso *p) {
  int f = p ? p->total - p->vistos : 0;
  return f < 0 ? 0 : f;
}

int conq_novos(int guardado, int atual) { return atual & ~guardado; }
int conq_guardar(int guardado, int atual) { return guardado | atual; }

void conq_aviso_zerar(ConqAviso *a) {
  if (!a) return;
  a->mostrados = 0; a->pendentes = 0; a->primeiro = -1;
}

void conq_aviso_somar(ConqAviso *a, int id) {
  if (!a) return;
  if (a->pendentes == 0) a->primeiro = id;
  a->pendentes++;
}

int conq_aviso_tirar(ConqAviso *a, int momentoBom, int *id, int *extras) {
  if (!a || !momentoBom || a->pendentes <= 0 || a->mostrados >= 1) return 0;
  if (id) *id = a->primeiro;
  if (extras) *extras = a->pendentes - 1;
  a->mostrados = 1;
  a->pendentes = 0;
  a->primeiro = -1;
  return 1;
}

int conq_visivel_amigos(int alcance, int mostrar, int engracada, int infantil) {
  return alcance >= 1 && mostrar && !engracada && !infantil;
}

const char *conq_rotulo(int degrau) {
  switch (degrau) {
    case CONQ_PRIMEIRO: return "Primeiro passo";
    case CONQ_METADE:   return "Metade";
    case CONQ_QUASE:    return "Quase lá";
    case CONQ_COMPLETA: return "Completa";
    case CONQ_ORDEM:    return "Em ordem";
  }
  return "";
}
