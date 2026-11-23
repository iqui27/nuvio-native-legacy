// MOTOR ASS EM TEMPO DE EXECUCAO (parte leve, sem ajustes.c): padrao ligado
// sem dono registrado, ass_novo_registrar trocando ao vivo e os valores fixos
// ASSPERF_* acompanhando. Mais o layout de MkvFaixa (quadroNs/transferencia,
// PR 734): 176 bytes com long de 8, 168 com long de 4.
#include "../src/assperf.h"
#include "../src/mkv.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static int leitor;
static int ler(void) { return leitor; }

int main(void) {
  // Sem dono (binario de teste sem ajustes): motor novo ligado, o padrao.
  assert(ass_novo_ligado() == 1);
  assert(ASSPERF_FIOS == 3);
  assert(ASSPERF_ADIANTE_MS == 100);
  assert(ASSPERF_PODA_MS == 120000L);
  assert(ASSPERF_FONTES_TETO == (size_t)64 * 1024u * 1024u);
  assert(ASSPERF_PARQUE == (6L << 20));

  // Dono registrado: a leitura manda — 0 volta ao motor antigo NA HORA.
  ass_novo_registrar(ler);
  leitor = 0;
  assert(ass_novo_ligado() == 0);
  assert(ASSPERF_FIOS == 1);
  assert(ASSPERF_ADIANTE_MS == 0);
  assert(ASSPERF_PODA_MS == 0L);
  assert(ASSPERF_FONTES_TETO == (size_t)16 * 1024u * 1024u);
  assert(ASSPERF_PARQUE == 0L);
  leitor = 1;
  assert(ass_novo_ligado() == 1);
  assert(ASSPERF_FIOS == 3 && ASSPERF_ADIANTE_MS == 100);

  // MkvFaixa: os dois campos novos moram no fim, nesta ordem, e o tamanho
  // e o do contrato C x Rust (src/mkvcore.h, rust/mkvcore/src/lib.rs).
  // transferencia fecha os dados; depois dele so vem o padding do u64.
  assert(sizeof(MkvFaixa) == (sizeof(long) == 8 ? 176 : 168));
  assert(offsetof(MkvFaixa, quadroNs) + 8 == offsetof(MkvFaixa, transferencia));
  assert(offsetof(MkvFaixa, transferencia) + sizeof(int) <= sizeof(MkvFaixa));

  printf("assmotor_leve: ok\n");
  return 0;
}
