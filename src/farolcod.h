// FAROL — barreira do produtor sobre o `cod` nativo (correcao 7B).
//
// farolerro_ler pode devolver cod com texto livre: lerWgt recorta ate 20
// bytes depois de "player_error" em QUALQUER posicao da mensagem — inclusive
// dentro de uma URL que o prepareAsync entrega inteira — e o saneamento do
// anel (farolanel_token) so confere o alfabeto, entao "av_canarioprivado"
// entrava. Ao anel so vai o que a gramatica fechada do farolnativo_cod_ok
// aceita (a mesma lista que o redator usa): os codigos fixos do farolerro, as
// familias numericas ums_<n>/m3_<n> e os 11 nomes do enum AVPlayError. O que
// nao casar vira "<plataforma>_outro" — o desconhecido convencional que ja
// esta na lista fechada, sem perder a plataforma do erro.
#ifndef NV_FAROLCOD_H
#define NV_FAROLCOD_H
#include "farolnativo.h"
#include <stdio.h>

static inline const char *farolcod_anel(const FarolErro *fe, FarolPlat p,
                                        char *buf, size_t n) {
  if (fe && farolnativo_cod_ok(fe->cod)) return fe->cod;
  snprintf(buf, n, "%s_outro", farolerro_prefixo(p));
  return buf;
}
#endif
