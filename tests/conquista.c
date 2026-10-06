// MOTOR DAS CONQUISTAS (conquista.h): degraus, "Em ordem", desbloqueio que
// nunca perde, orcamento de um aviso por sessao e privacidade. Sem rede, sem
// SDL. Cada caso e uma regra que o dono aprovou no plano (secao 8.1).
#include "conquista.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ConqItem it(long ordem, int visto, long long quando) {
  ConqItem c; c.ordem = ordem; c.visto = visto; c.quando = quando; return c;
}

static void degraus(void) {
  // Nada visto: nada. Nem "Primeiro passo" por abrir o app (regra 3).
  assert(conq_degraus(0, 11, 0) == 0);
  assert(conq_degraus(0, 0, 0) == 0);
  // 2 de 11 (a captura do dono, Gore Verbinski): so o primeiro passo.
  assert(conq_degraus(2, 11, 0) == CONQ_PRIMEIRO);
  // Metade e arredondada para cima do lado de quem viu: 6 de 11 e metade.
  assert(conq_degraus(5, 11, 0) == CONQ_PRIMEIRO);
  assert(conq_degraus(6, 11, 0) == (CONQ_PRIMEIRO | CONQ_METADE));
  // Quase la em alvo grande: 80% (9 de 11 ja e; 8 de 11 nao).
  assert(!(conq_degraus(8, 11, 0) & CONQ_QUASE));
  assert(conq_degraus(9, 11, 0) & CONQ_QUASE);
  // Alvo pequeno (<= 10): so "falta 1". 8 de 10 nao e quase; 9 de 10 e.
  assert(!(conq_degraus(8, 10, 0) & CONQ_QUASE));
  assert(conq_degraus(9, 10, 0) & CONQ_QUASE);
  // Completa leva os de baixo; Em ordem so com a prova.
  assert(conq_degraus(10, 10, 0) == (CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE | CONQ_COMPLETA));
  assert(conq_degraus(10, 10, 1) & CONQ_ORDEM);
  // "Em ordem" sem estar completa nao existe.
  assert(!(conq_degraus(9, 10, 1) & CONQ_ORDEM));
  // Alvo de um filme: ver o filme e tudo menos em ordem sem prova.
  assert(conq_degraus(1, 1, 0) == (CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE | CONQ_COMPLETA));
  // Visto a mais (filme que saiu da filmografia) nao passa de 100%.
  assert(conq_degraus(12, 10, 0) & CONQ_COMPLETA);
  assert(conq_maior(CONQ_PRIMEIRO | CONQ_METADE) == CONQ_METADE);
  assert(conq_maior(0) == 0);
  assert(conq_indice(CONQ_ORDEM) == 4 && conq_indice(CONQ_PRIMEIRO) == 0);
  assert(conq_indice(3) == -1);
  assert(!strcmp(conq_rotulo(CONQ_QUASE), "Quase lá"));
  printf("ok degraus\n");
}

static void emOrdem(void) {
  ConqItem v[4];
  // Lancamento 1998, 2000, 2010, 2016; visto em datas crescentes: em ordem.
  v[0] = it(19980101, 1, 100); v[1] = it(20000101, 1, 200);
  v[2] = it(20100101, 1, 300); v[3] = it(20160101, 1, 400);
  assert(conq_em_ordem(v, 4));
  // Mesma data de fim (maratona num dia so, carimbo igual): vale.
  v[2].quando = 200;
  assert(conq_em_ordem(v, 4));
  // Um fora de ordem: nao.
  v[2].quando = 50;
  assert(!conq_em_ordem(v, 4));
  // Rever nao conta: quem chama passa a PRIMEIRA vez. Com a primeira vez em
  // ordem, uma revisao depois nao estraga (ela nem entra aqui).
  v[2].quando = 300;
  assert(conq_em_ordem(v, 4));
  // Sem data em um item: nao ha prova, nao sai.
  v[1].quando = 0;
  assert(!conq_em_ordem(v, 4));
  v[1].quando = 200;
  // Um nao visto: nao.
  v[3].visto = 0;
  assert(!conq_em_ordem(v, 4));
  v[3].visto = 1;
  // Itens com a MESMA ordem (dois filmes no mesmo dia) em qualquer ordem.
  v[1].ordem = v[2].ordem = 20050505; v[1].quando = 350; v[2].quando = 250;
  assert(conq_em_ordem(v, 4));
  // Ordem da historia (trilha): ordem = posicao, nao data.
  { ConqItem s[3];
    s[0] = it(1, 1, 30); s[1] = it(2, 1, 20); s[2] = it(3, 1, 40);
    assert(!conq_em_ordem(s, 3));
    s[1].quando = 35;
    assert(conq_em_ordem(s, 3)); }
  assert(!conq_em_ordem(NULL, 0));
  printf("ok em ordem\n");
}

static void calcular(void) {
  ConqItem v[11];
  ConqProgresso p;
  int i;
  for (i = 0; i < 11; i++) v[i] = it(20000101 + i, i < 2, i < 2 ? 100 + i : 0);
  p = conq_calcular(v, 11);
  assert(p.vistos == 2 && p.total == 11 && conq_faltam(&p) == 9);
  assert(p.degraus == CONQ_PRIMEIRO);
  for (i = 0; i < 11; i++) { v[i].visto = 1; v[i].quando = 100 + i; }
  p = conq_calcular(v, 11);
  assert(p.degraus & CONQ_ORDEM);
  // Completa fora de ordem: Completa sim, Em ordem nao.
  v[0].quando = 999;
  p = conq_calcular(v, 11);
  assert((p.degraus & CONQ_COMPLETA) && !(p.degraus & CONQ_ORDEM));
  p = conq_calcular(NULL, 0);
  assert(p.total == 0 && p.degraus == 0);
  printf("ok calcular\n");
}

static void desbloqueio(void) {
  int g = 0, atual;
  atual = CONQ_PRIMEIRO;
  assert(conq_novos(g, atual) == CONQ_PRIMEIRO);
  g = conq_guardar(g, atual);
  // Mesma coisa de novo: nada novo, nada de aviso repetido.
  assert(conq_novos(g, atual) == 0);
  atual = CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE | CONQ_COMPLETA;
  assert(conq_novos(g, atual) == (CONQ_METADE | CONQ_QUASE | CONQ_COMPLETA));
  g = conq_guardar(g, atual);
  // O diretor lancou um filme novo: o progresso cai para "quase", mas a
  // Completa ganha fica (nada se perde) e nao ha "novo" a anunciar.
  atual = CONQ_PRIMEIRO | CONQ_METADE | CONQ_QUASE;
  assert(conq_novos(g, atual) == 0);
  assert(conq_guardar(g, atual) & CONQ_COMPLETA);
  printf("ok desbloqueio\n");
}

static void aviso(void) {
  ConqAviso a;
  int id = -9, extras = -9;
  conq_aviso_zerar(&a);
  // Nada pendente: nada sai.
  assert(!conq_aviso_tirar(&a, 1, &id, &extras));
  conq_aviso_somar(&a, 3);
  conq_aviso_somar(&a, 7);
  conq_aviso_somar(&a, 8);
  // Durante o filme (momento ruim): espera.
  assert(!conq_aviso_tirar(&a, 0, &id, &extras));
  // Depois dos creditos: UM aviso, o primeiro, com "+2".
  assert(conq_aviso_tirar(&a, 1, &id, &extras));
  assert(id == 3 && extras == 2);
  // Um segundo desbloqueio na mesma sessao nao vira segundo aviso.
  conq_aviso_somar(&a, 9);
  assert(!conq_aviso_tirar(&a, 1, &id, &extras));
  printf("ok aviso\n");
}

static void privacidade(void) {
  // Padrao (P6): alcance >= 1 e "Mostrar aos amigos" marcado.
  assert(conq_visivel_amigos(1, 1, 0, 0));
  assert(conq_visivel_amigos(2, 1, 0, 0));
  // Alcance 0 (so eu): nunca, mesmo marcado.
  assert(!conq_visivel_amigos(0, 1, 0, 0));
  // Desmarcado no modal: nao.
  assert(!conq_visivel_amigos(2, 0, 0, 0));
  // Engracada: privada sempre.
  assert(!conq_visivel_amigos(2, 1, 1, 0));
  // Perfil infantil: nunca para amigos.
  assert(!conq_visivel_amigos(2, 1, 0, 1));
  printf("ok privacidade\n");
}

int main(void) {
  degraus();
  emOrdem();
  calcular();
  desbloqueio();
  aviso();
  privacidade();
  printf("conquista: tudo ok\n");
  return 0;
}
