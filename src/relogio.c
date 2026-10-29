#include "relogio.h"

#include <math.h>
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

static double taxaDe(const Relogio *r) { return r->taxa > 0.0 ? r->taxa : 1.0; }

void relogio_taxa(Relogio *r, double taxa) {
  if (taxa <= 0.0) taxa = 1.0;
  if (taxa == taxaDe(r)) return;
  r->taxa = taxa;
  r->n = r->prox = 0;
}

static double maxOff(const Relogio *r) {
  int i; double m = r->off[0];
  for (i = 1; i < r->n; i++) if (r->off[i] > m) m = r->off[i];
  return m;
}

/* Carimbadas: a mediana dos deslocamentos (robusta a uma leitura fora da
 * curva e a posicao que anda em degraus). */
static double offCarimbo(const Relogio *r) {
  double v[RELOGIO_AMOSTRAS_C];
  int i, j, n = r->n;
  for (i = 0; i < n; i++) {
    double x = r->off[i];
    for (j = i; j > 0 && v[j - 1] > x; j--) v[j] = v[j - 1];
    v[j] = x;
  }
  return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) * 0.5;
}

static double offAtual(const Relogio *r) { return r->carimbo ? offCarimbo(r) : maxOff(r); }

/* Parada sem pausa: a posicao nao andou e o carimbo andou isto. */
#define REL_PARADO 0.25
/* Carimbadas: a previsao para depois disto alem da ultima leitura. */
#define REL_TETO_CARIMBO 1.0

void relogio_amostra_carimbo(Relogio *r, double posSeg, double agora, double incerteza, int tocando) {
  (void)incerteza;   // the median over the window already absorbs it (relogio.h)
  r->carimbo = 1;
  if (!tocando) {
    r->tocando = 0; r->n = r->prox = 0;
    r->ultPos = posSeg; r->ultAgora = agora; r->temAmostra = 1;
    r->saida = posSeg;
    return;
  }
  if (!r->tocando) { r->tocando = 1; r->n = r->prox = 0; }
  if (r->temAmostra && posSeg == r->ultPos) {
    // Mesma posicao: so conta se o carimbo andou o bastante para ser parada.
    if (agora - r->ultAgora > REL_PARADO) { r->n = r->prox = 0; r->saida = posSeg; r->ultAgora = agora; }
    return;
  }
  { double off = posSeg - agora * taxaDe(r);
    if (r->n) {
      double atual = offCarimbo(r);
      if (fabs(off - atual) > REL_SALTO || (r->temAmostra && posSeg < r->ultPos - REL_RECUO)) {
        r->n = r->prox = 0;
        r->saida = posSeg;
      }
    }
    r->off[r->prox] = off;
    r->prox = (r->prox + 1) % RELOGIO_AMOSTRAS_C;
    if (r->n < RELOGIO_AMOSTRAS_C) r->n++; }
  r->ultPos = posSeg; r->ultAgora = agora; r->temAmostra = 1;
}

void relogio_amostra(Relogio *r, double posSeg, double agora, int tocando) {
  int nova = !r->temAmostra || posSeg != r->ultPos;
  if (r->carimbo && tocando) return;   // tocando, quem alimenta e relogio_amostra_carimbo

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
  { double off = posSeg - agora * taxaDe(r);
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
  v = agora * taxaDe(r) + offAtual(r);
  // O teto e em tempo de RELOGIO: a 2x, 350 ms de relogio sao 700 ms de video.
  // Carimbadas: a leitura e de quando foi tirada (pode ter 600 ms ao chegar);
  // o teto conta do carimbo dela.
  if (r->carimbo) {
    double teto = r->ultPos + ((agora - r->ultAgora) < REL_TETO_CARIMBO ? (agora - r->ultAgora) : REL_TETO_CARIMBO) * taxaDe(r)
                  + REL_TETO * taxaDe(r) * 0.25;
    if (v > teto) v = teto;
  } else if (v > r->ultPos + REL_TETO * taxaDe(r)) v = r->ultPos + REL_TETO * taxaDe(r);
  if (v < r->ultPos - REL_RECUO) v = r->ultPos;
  // Monotonico entre saltos: um recuo pequeno (amostra que chegou adiantada
  // saiu da janela) segura o valor em vez de voltar.
  if (v < r->saida && r->saida - v < REL_RECUO) v = r->saida;
  r->saida = v;
  return v;
}
