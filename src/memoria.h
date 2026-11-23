// A CLASSIFICACAO DE MEMORIA DO APARELHO — a "regra do guia" (EPG, base da
// #776), compartilhada de verdade: o guia de TV (epg.c) e as miniaturas feitas
// no aparelho (src/miniatura.c, PR 680 etapa B) usam ESTA funcao, nenhum dos
// dois tem limiar proprio.
//
//   AUTO:     pouca = RAM desconhecida (<= 0) ou <= 2048 MB;
//   BAIXA:    sempre pouca;
//   COMPLETA: nunca pouca;
//   __EMSCRIPTEN__ (.wgt): AUTO e sempre pouca.
#ifndef NV_MEMORIA_H
#define NV_MEMORIA_H
// Device-local memory policy, set on the drawing thread. Workers capture it;
// low mode retains current programmes and the next 24 hours. Full mode may
// refresh a shortened binary cache to recover later programmes.
enum { EPG_MEMORIA_AUTO, EPG_MEMORIA_BAIXA, EPG_MEMORIA_COMPLETA };
// 1 = pouca memoria sob o modo dado. Fora da faixa vira AUTO.
static inline int epg_memoria_eh_baixa(int modo, long ramMB) {
  if (modo < EPG_MEMORIA_AUTO || modo > EPG_MEMORIA_COMPLETA) modo = EPG_MEMORIA_AUTO;
  int baixa = modo == EPG_MEMORIA_BAIXA ||
              (modo == EPG_MEMORIA_AUTO && (ramMB <= 0 || ramMB <= 2048));
#ifdef __EMSCRIPTEN__
  if (modo == EPG_MEMORIA_AUTO) baixa = 1;
#endif
  return baixa;
}
#endif
