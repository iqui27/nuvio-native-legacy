// Tizen 4-5.5 .tpk (NuvioTpk40, TVs de 2018-2020): memoria da conversao
// DTS/TrueHD. (As miniaturas feitas na TV nao rodam no 4/5: miniatura.c, logs
// de 11/2026.)
//
// Estas TVs tem menos RAM que a S95C onde tudo foi medido (registro 8825: 1108
// MB no total), e ninguem mediu quanto sobra livre nelas. Por isso:
//  - SEGURANCA (fica): o conversor abre com valores tirados da memoria LIVRE de
//    agora, nao so da versao da Tizen. O resto e o que todo aparelho ja tem: o
//    vigia do app (memlog.h: abaixo de 150 MB livres solta texturas, caches e
//    heap). Perfil e entrega ficam no Auto, e cada linha do log diz com qual
//    rodou.
//
// SEM macro de build neste .h: o tools/tpk.sh reaproveita na .so do 4/5 os
// objetos comuns de quem nao cita NV_TPK40, entao quem chama pergunta em
// execucao (tpk45_ativo). Na .so dos hosts 6+ tudo aqui e stub e nada muda.
#ifndef NV_TPK45_H
#define NV_TPK45_H
#include <stddef.h>

enum { TPK45_PERFIL_AUTO, TPK45_PERFIL_NORMAL, TPK45_PERFIL_LEVE, TPK45_PERFIL_MINIMO, TPK45_PERFIS };
enum { TPK45_ENTREGA_AUTO, TPK45_ENTREGA_HLS, TPK45_ENTREGA_CONTINUO, TPK45_ENTREGAS };

int tpk45_ativo(void);            // 1 so na .so do NuvioTpk40
int tpk45_perfil(void);           // TPK45_PERFIL_AUTO no 4/5
int tpk45_perfil_efetivo(void);   // o Auto resolvido pela memoria livre (nunca AUTO)
int tpk45_entrega(void);          // TPK45_ENTREGA_AUTO no 4/5
// "perfil light (auto), entrega auto": vai nas linhas de log de quem usa os valores.
const char *tpk45_marca(void);

// Conversor: anel do leitor e teto da fila, em MiB, para a sessao que abre agora.
int tpk45_anel_mib(void);
int tpk45_fila_mib(void);
// Amostra do conversor (a cada 500 ms): +1 quando a TV entra abaixo do piso do
// app, -1 quando sai com folga, 0 no resto. `apertado` e o estado de quem chama.
// So AVISA: o teto da fila e erro fatal do conversor, nao um freio, entao
// baixa-lo com a conversao em curso derrubaria o som em vez de poupar memoria.
int tpk45_aperto(long livreKb, int *apertado);

// Log: a TV (uma vez por execucao) e a linha de 10 s enquanto ha video.
void tpk45_tv(void);
void tpk45_tick(int convertendo, const char *modo);
#endif
