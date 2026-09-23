#include "relogio.h"

#include <string.h>

// Mais do que isto de diferenca entre a amostra e a previsao e salto (seek,
// troca de fonte, retomada): o historico nao vale mais.
#define REL_SALTO     0.75
// A previsao nunca passa disto alem da ultima amostra: se o pipeline parou de
// mandar posicao (buffering), a legenda para logo depois dele.
#define REL_TETO      0.35
// Recuo tolerado sem acompanhar: as amostras oscilam dezenas de ms; andar
// para tras por isso faria o letreiro tremer. Mais que isto e salto de fato.
#define REL_RECUO     0.25

void relogio_zerar(Relogio *r) { memset(r, 0, sizeof *r); }

static double maxOff(const Relogio *r) {
  int i; double m = r->off[0];
  for (i = 1; i < r->n; i++) if (r->off[i] > m) m = r->off[i];
  return m;
}

void relogio_amostra(Relogio *r, double posSeg, double agora, int tocando) {
  int nova = !r->temAmostra || posSeg != r->ultPos;
  if (!tocando) {
    // Pausado: a posicao e a do pipeline, sem previsao, e o historico cai —
    // ao voltar, o deslocamento e outro.
    r->tocando = 0; r->n = r->prox = 0;
    r->ultPos = posSeg; r->ultAgora = agora; r->temAmostra = 1;
    r->saida = posSeg;
    return;
  }
  if (!r->tocando) { r->tocando = 1; r->n = r->prox = 0; nova = 1; }
  if (!nova) return;
  { double off = posSeg - agora;
    if (r->n && (off - maxOff(r) > REL_SALTO || maxOff(r) - off > REL_SALTO)) {
      r->n = r->prox = 0;
      r->saida = posSeg;
    } else if (r->temAmostra && posSeg < r->ultPos - REL_RECUO) {
      r->n = r->prox = 0;
      r->saida = posSeg;
    }
    r->off[r->prox] = off;
    r->prox = (r->prox + 1) % RELOGIO_AMOSTRAS;
    if (r->n < RELOGIO_AMOSTRAS) r->n++; }
  r->ultPos = posSeg; r->ultAgora = agora; r->temAmostra = 1;
}

double relogio_ler(Relogio *r, double agora) {
  double v;
  if (!r->temAmostra) return 0.0;
  if (!r->tocando || !r->n) { r->saida = r->ultPos; return r->ultPos; }
  v = agora + maxOff(r);
  if (v > r->ultPos + REL_TETO) v = r->ultPos + REL_TETO;
  if (v < r->ultPos - REL_RECUO) v = r->ultPos;
  // Monotonico entre saltos: um recuo pequeno (amostra que chegou adiantada
  // saiu da janela) segura o valor em vez de voltar.
  if (v < r->saida && r->saida - v < REL_RECUO) v = r->saida;
  r->saida = v;
  return v;
}
