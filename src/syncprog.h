// Progresso de reproducao <-> conta (sync_pull_watch_progress /
// sync_push_watch_progress). Separado de sync.c para ter teste sem subir o
// ciclo inteiro: as tres funcoes so dependem de sessao_rpc, progresso.c e do
// catalogo, e todas as tres sao dublaveis.
//
// Formato de linha, o MESMO do app web (watchProgressSyncService.js):
//   content_id (titulo puro), content_type movie|series,
//   video_id ("__nuvio_episode__:S:E" em serie, content_id em filme),
//   season/episode (null em filme), position/duration em MS,
//   last_watched em ms, progress_key "tt123_s4e9" | "tt123".
// Era aqui que este app divergia (PLANO-PROGRESSO.md 1.1, 1.2, 1.7).
#ifndef NV_SYNCPROG_H
#define NV_SYNCPROG_H

// FIO DE SYNC. Puxa as linhas do perfil ativo para uma caixa interna. Nao
// aplica nada: a aplicacao mexe em progresso.c e no catalogo, que sao do fio
// principal. Devolve quantas linhas vieram; -1 em erro de rede.
int  syncprog_puxar(void);

// FIO DE SYNC. Empurra as linhas PENDENTES do perfil ativo. Vazio nunca vira
// push. Em 2xx, marca as chaves enviadas como empurradas. Devolve quantas
// mandou; -1 em erro.
int  syncprog_empurrar(void);

// FIO PRINCIPAL. Aplica a caixa puxada: prog_aplicar_remoto decide (pendente
// local vence; senao o mais novo), e o que ele aceitou vai para o item do
// catalogo quando o titulo esta la. Esvazia a caixa. Devolve quantas linhas o
// progresso aceitou; `casaram`, se nao NULL, recebe quantas dessas acharam
// item no catalogo.
int  syncprog_aplicar(int *casaram);

// Quantas linhas ha na caixa (para o resumo da tela de ajustes).
int  syncprog_puxadas(void);

// Esvazia a caixa sem aplicar (logout).
// Apaga uma entrada de progresso NA CONTA (sync_delete_watch_progress), pela
// chave de prog_chave. Ver a nota longa em syncprog.c.
int  syncprog_remover(const char *chave);
void syncprog_esquecer(void);

#endif
