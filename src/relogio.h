#ifndef NV_RELOGIO_H
#define NV_RELOGIO_H

/* Relogio de video INTERPOLADO (#92). O pipeline da LG so diz a posicao no
 * evento currentTime, a cada ~200 ms (medido na C9 em 23/09: 1732 intervalos,
 * media 200,9 ms, 197..247). Quem desenha a 60 quadros por segundo e usa o
 * ultimo numero recebido anda aos degraus e, em media, 100 ms atras.
 *
 * Cada amostra (posicao, instante monotonico em que chegou) da uma estimativa
 * do deslocamento entre o relogio monotonico e o do video: off = pos - agora.
 * O atraso de entrega so SOMA ao instante de chegada, entao a amostra menos
 * atrasada e a de MAIOR off — o relogio usa o maximo das ultimas amostras. */

#define RELOGIO_AMOSTRAS 12

typedef struct {
  double off[RELOGIO_AMOSTRAS];
  int    n, prox;
  double ultPos, ultAgora;   // ultima amostra
  double saida;              // ultimo valor devolvido (para nao andar para tras)
  int    tocando, temAmostra;
} Relogio;

void   relogio_zerar(Relogio *r);
/* Chame a cada quadro: posSeg e o numero do pipeline (pode repetir), agora
 * em segundos monotonicos. Amostra nova e detectada pela mudanca de posSeg. */
void   relogio_amostra(Relogio *r, double posSeg, double agora, int tocando);
double relogio_ler(Relogio *r, double agora);

#endif
