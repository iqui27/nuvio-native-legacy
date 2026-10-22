#include "fonteauto.h"
#include "fonteregra.h"
#include <stddef.h>

static int naFila(const int *fila, int n, int i) {
  int j;
  for (j = 0; j < n; j++) if (fila[j] == i) return 1;
  return 0;
}

static int foraDoGrupo(const signed char *grupo, int i, int g) {
  return grupo ? grupo[i] != g : g != 0;
}

// Rank do addon da fonte i (0 = primeiro da ordem); sem ordem, todas iguais.
static int rankDe(const int *rank, int i) { return rank ? rank[i] : 0; }
// Faixa de qualidade da pontuacao: floor(p / FONTEAUTO_FAIXA). Os degraus de
// resolucao e as multas de cache/teto/origem sao multiplos disso, entao duas
// fontes da mesma faixa sao "a mesma qualidade" para o desempate.
static long faixaDe(long p) { return p >= 0 ? p / FONTEAUTO_FAIXA : -((-p + FONTEAUTO_FAIXA - 1) / FONTEAUTO_FAIXA); }

int fonteauto_fila_o(int modo, int total, int preferida, const long *pontos,
                     const unsigned char *acimaTeto, const unsigned char *excluida,
                     const signed char *grupo, const int *rank, int ordemUso,
                     int max, int *fila) {
  int n = 0, g;
  if (!fila || max < 1 || total < 1) return 0;
  if (!rank) ordemUso = 0;

  // A PREFERIDA ENTRA PRIMEIRO nos dois modos: e a fonte que a pessoa escolheu
  // a mao neste titulo (issues #56 e #57). Continua sendo UMA candidata — no
  // modo PRIMEIRA ela e a unica conferida, e nenhuma outra e tocada.
  if (preferida >= 0 && preferida < total && !(excluida && excluida[preferida]))
    fila[n++] = preferida;

  for (g = 0; g < FONTEAUTO_GRUPOS && n < max; g++) {
    // Ordem estrita: dentro do teto antes de fora dele (o teto e preferencia,
    // nao filtro, ver cabeNoTeto em streams.c), e so depois a ordem dos
    // add-ons. Nos outros usos o MELHOR deixa o teto para a pontuacao.
    int passos = (modo == FONTEAUTO_PRIMEIRA || ordemUso == FR_ORDEM_ESTRITA) ? 2 : 1, passo;
    for (passo = 0; passo < passos && n < max; passo++) {
      // O proximo candidato deste (grupo, passo): o melhor pela chave do modo.
      while (n < max) {
        int melhor = -1, i;
        for (i = 0; i < total; i++) {
          int acima = acimaTeto && acimaTeto[i];
          if (passos == 2 && (passo == 0) == acima) continue;
          if ((excluida && excluida[i]) || foraDoGrupo(grupo, i, g) || naFila(fila, n, i)) continue;
          if (melhor < 0) { melhor = i; continue; }
          if (modo == FONTEAUTO_PRIMEIRA) {
            // Ordem do addon; com a ordem ligada, o addon mais cedo na ordem
            // vem antes (dentro dele, a ordem em que ele mandou).
            if (ordemUso && rankDe(rank, i) < rankDe(rank, melhor)) melhor = i;
          } else {
            long pi = pontos ? pontos[i] : 0, pm = pontos ? pontos[melhor] : 0;
            if (ordemUso == FR_ORDEM_ESTRITA) {
              int ri = rankDe(rank, i), rm = rankDe(rank, melhor);
              if (ri < rm || (ri == rm && pi > pm)) melhor = i;
            } else if (ordemUso == FR_ORDEM_DESEMPATE) {
              long fi = faixaDe(pi), fm = faixaDe(pm);
              int ri = rankDe(rank, i), rm = rankDe(rank, melhor);
              if (fi > fm || (fi == fm && (ri < rm || (ri == rm && pi > pm)))) melhor = i;
            } else if (pi > pm) melhor = i;   // `>` e nao `>=`: empate fica o primeiro da lista
          }
        }
        if (melhor < 0) break;
        fila[n++] = melhor;
      }
    }
  }
  return n;
}

int fonteauto_fila_g(int modo, int total, int preferida, const long *pontos,
                     const unsigned char *acimaTeto, const unsigned char *excluida,
                     const signed char *grupo, int max, int *fila) {
  return fonteauto_fila_o(modo, total, preferida, pontos, acimaTeto, excluida, grupo,
                          NULL, FR_ORDEM_NAO, max, fila);
}

int fonteauto_fila(int modo, int total, int preferida, const long *pontos,
                   const unsigned char *acimaTeto, const unsigned char *excluida,
                   int max, int *fila) {
  return fonteauto_fila_g(modo, total, preferida, pontos, acimaTeto, excluida, NULL, max, fila);
}

int fonteauto_tentativas(int modo, int pedidas) {
  if (modo == FONTEAUTO_PRIMEIRA) return 1;
  return pedidas < 1 ? 1 : pedidas;
}

int fonteauto_primeira(const int *fila, int n, FonteVerificar verificar,
                       FonteFalhou falhou, void *u, int *tocadas) {
  int q, conta = 0, escolhida = -1;
  if (fila && verificar)
    for (q = 0; q < n; q++) {
      conta++;
      if (verificar(fila[q], u)) { escolhida = fila[q]; break; }
      if (falhou) falhou(fila[q], u);
    }
  if (tocadas) *tocadas = conta;
  return escolhida;
}

int fonteauto_pode_decidir(const FonteautoParcial *p) {
  int fila[1], nf, c, q, temCand = 0;
  if (!p || p->total < 1) return 0;
  for (q = 0; q < p->total; q++)
    if (!(p->excluida && p->excluida[q]) && !(p->grupo && p->grupo[q] < 0)) { temCand = 1; break; }
  if (!temCand) return 0;
  if (!p->algumPendente) return 1;
  if (p->preferida >= 0 && p->preferida < p->total &&
      !(p->excluida && p->excluida[p->preferida])) return 1;
  if (p->prefPendente) return 0;
  if (p->prazoPassou) return 1;
  nf = fonteauto_fila_o(p->modo, p->total, -1, p->pontos, p->acimaTeto,
                        p->excluida, p->grupo, p->rank, p->ordemUso, 1, fila);
  if (nf < 1) return 0;
  c = fila[0];
  // Um addon que ainda nao respondeu pode trazer um grupo melhor (o permitido):
  // espera por ele ate o prazo.
  if (p->grupo && p->pendenteGrupoMin < p->grupo[c]) return 0;
  if (p->instantaneo) return 1;
  // ORDEM ESTRITA: a escolha sai assim que o add-on mais cedo na ordem que ja
  // tem fonte valida respondeu, sem esperar os de depois. So falta esperar um
  // add-on que esta ANTES na ordem (ou, se a candidata nao esta na ordem, um
  // add-on da ordem). Fonte acima do teto espera o prazo, como em PRIMEIRA.
  if (p->ordemUso == FR_ORDEM_ESTRITA && p->rank) {
    int rc = p->rank[c];
    if (p->pendenteRankMin <= rc && p->pendenteRankMin < FR_ORDEM_SEM) return 0;
    if (rc < FR_ORDEM_SEM) return !(p->acimaTeto && p->acimaTeto[c]);
  }
  if (p->modo == FONTEAUTO_PRIMEIRA) {
    if (p->pendenteAntes && p->addon && p->pendenteAntes(p->addon[c], p->u)) return 0;
    return !(p->acimaTeto && p->acimaTeto[c]);
  }
  return p->boa && p->boa[c];
}
