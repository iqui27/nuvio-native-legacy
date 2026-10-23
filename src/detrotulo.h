// O ROTULO DO BOTAO PRIMARIO DO DETALHE quando nao ha episodio-alvo (filme, ou
// serie sem episodio a retomar): "Retomar" ou "Reproduzir". Puro, para o teste
// (tests/detrotulo.sh) nao precisar do desenho inteiro de detail.c.
// `concluido` e o Percentual assistido (ajustes_cw_concluido, 90 de fabrica).
#ifndef NV_DETROTULO_H
#define NV_DETROTULO_H

static inline const char *det_rotulo_primario(int progresso, int concluido) {
  // O MESMO CORTE DO PLAYER (player.c, retomarPct) e de temInicio: do
  // Percentual assistido em diante o filme esta visto e toca do comeco.
  return progresso > 0 && progresso < concluido ? "Retomar" : "Reproduzir";
}

#endif
