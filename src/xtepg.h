// GRADE CURTA DO XTREAM, canal a canal (#158).
//
// POR QUE EXISTE: no registro 6311 (LG C4, Xtream romeno) a grade XMLTV do
// proprio provedor passou do teto de 32 MB e foi ignorada ("grade do
// provedor: maior que o teto, ignorada"), e nenhuma das cinco fontes do
// epgshare01 que o app baixava cobria a Romenia: 900 canais sem uma linha de
// programa. O player_api.php tem get_short_epg, que devolve os proximos
// programas de UM canal em poucos KB. Pedir so os canais que a tela esta
// mostrando resolve "o que passa agora" em qualquer Xtream, sem arquivo
// grande e sem depender de casar nome.
//
// COMO: o guia chama xtepg_querer para cada canal Xtream que desenha sem
// grade; um fio proprio pede um por vez (em serie: painel responde 429 a
// rajada) e o resultado entra na tela em xtepg_passo, no fio de desenho.
// Tudo que a tela le (titulos) so muda em xtepg_passo — um EpgProg obtido
// aqui vale ate o proximo xtepg_passo, como o de epg.h vale ate o epg_passo.
//
// VALIDADE: 30 min por canal, ou menos se o ultimo programa guardado ja
// acabou; falha de rede tenta de novo em 10 min.
#ifndef NV_XTEPG_H
#define NV_XTEPG_H

#include <time.h>
#include "epg.h"

void xtepg_querer(const char *id);          // fio de desenho; nao bloqueia
void xtepg_passo(void);                     // fio de desenho, por quadro
int  xtepg_tem(const char *id);             // ha programa guardado
int  xtepg_agora(const char *id, time_t t, EpgProg *p);
int  xtepg_proximo(const char *id, time_t t, int k, EpgProg *p);
// Como epg_faixa: com `out` devolve no maximo `cap`; com out NULL, o total.
int  xtepg_faixa(const char *id, time_t de, time_t ate, EpgProg *out, int cap);
int  xtepg_faixa_desde(const char *id, time_t de, time_t ate, int pular, EpgProg *out, int cap);
// Esquece tudo (troca de perfil ou de cadastro do Xtream).
void xtepg_limpar(void);

#endif
