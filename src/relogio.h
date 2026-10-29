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
// Carimbadas (.tpk): a ~25 leituras/s, 32 cobrem ~1,3 s.
#define RELOGIO_AMOSTRAS_C 32

typedef struct {
  double off[RELOGIO_AMOSTRAS_C];
  int    n, prox;
  int    carimbo;            // 1 = alimentado por relogio_amostra_carimbo
  double ultPos, ultAgora;   // ultima amostra
  double saida;              // ultimo valor devolvido (para nao andar para tras)
  int    tocando, temAmostra;
  double taxa;               // #202: velocidade (1,5 = 1,5x); 0 = 1x
} Relogio;

void   relogio_zerar(Relogio *r);
/* Chame a cada quadro: posSeg e o numero do pipeline (pode repetir), agora
 * em segundos monotonicos. Amostra nova e detectada pela mudanca de posSeg. */
void   relogio_amostra(Relogio *r, double posSeg, double agora, int tocando);
double relogio_ler(Relogio *r, double agora);
/* VELOCIDADE (#202). A 1,5x o video anda 1,5 s por segundo do relogio: com o
 * deslocamento contado a 1x, entre duas amostras a previsao andava devagar
 * demais (a 2x, ate 200 ms atrasada) e a 0,75x o maximo das amostras velhas
 * a punha ate o teto adiantada. Com a taxa, off = pos - agora*taxa. Mudar a
 * taxa zera o historico (as amostras velhas foram tiradas a outra taxa). */
void   relogio_taxa(Relogio *r, double taxa);
/* LEITURA CARIMBADA (.tpk, dono 25/10). No Samsung a posicao vem de uma
 * consulta ao player que leva 50-600 ms (1,3 s no pior do log); carimbada na
 * CHEGADA, a legenda atrasava o tanto da consulta mais rapida da janela, mais
 * nas cenas pesadas. O host carimba o MEIO da consulta (`agora`, mesmo relogio
 * monotonico) e diz a meia duracao (`incerteza`): a posicao foi lida em algum
 * ponto de [agora - incerteza, agora + incerteza]. O deslocamento e a
 * MEDIANA das ultimas amostras (25/10: no log da S95C a legenda ora adiantava
 * ora atrasava um quadro; a mediana nao segue uma leitura fora da curva).
 * Posicao parada com o carimbo andando (buffering, fim) segura a legenda. */
void   relogio_amostra_carimbo(Relogio *r, double posSeg, double agora, double incerteza, int tocando);

#endif
