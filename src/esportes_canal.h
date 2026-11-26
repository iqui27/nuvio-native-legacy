// ESPORTE CASADO COM O CANAL — a regra que diz QUAL canal da pessoa transmite
// um jogo, e em que ordem mostrar os candidatos.
//
// Regra pura: sem rede, sem tela, sem guia. Quem chama (guiav5_evento.inc na
// ficha do jogo, esportes_casar.c no lembrete) ja conferiu a grade com
// esportes_corresponde e so pergunta a nota de cada canal.
#ifndef NV_ESPORTES_CANAL_H
#define NV_ESPORTES_CANAL_H
#include "esportes.h"

// A partir daqui o canal e oferecido sozinho ("começa agora no ..."): a grade
// confirmou o jogo, ou o nome do canal E a emissora que a agenda informou.
#define ESP_CANAL_CERTO 60

// Nota do canal `nome` para o evento. 0 = nao e candidato.
//   +100  a grade (EPG) do canal confirma o jogo (`epg` != 0)
//   +60   o canal e exatamente a emissora informada ("ESPN HD" para ESPN)
//   +20   o canal so contem o nome da emissora ("ESPN 2" para ESPN): um
//         canal irmao, listado por ultimo e nunca oferecido sozinho
//   +0..2 qualidade no nome (HD 1, FHD/4K 2), SD -1: desempata iguais
int esportes_canal_nota(const EspEvento *ev, const char *nome, int epg);

// Lembrete que so guardou o titulo ("Lakers at Celtics", "Flamengo x Vasco"):
// refaz um evento minimo para conferir a grade. 1 = achou os dois lados;
// 0 = evento de um nome so (corrida, luta), conferido pelo titulo inteiro.
int esportes_canal_evento_do_titulo(const char *titulo, const char *modalidade,
                                    time_t inicio, time_t fim, EspEvento *out);
#endif
