#ifndef NV_BACKUPPLANO_H
#define NV_BACKUPPLANO_H
#include "backuplocal.h"
#include <stdint.h>
typedef struct BackupPlano BackupPlano;
// Read an owned native store. 1=present (malloc output), 0=absent, -1=error.
typedef int (*BackupLerAtual)(void *ctx,int tipo,char **texto,size_t *n);
enum { BACKUP_PLANO_OK=1,BACKUP_PLANO_CONTEXTO=-1,BACKUP_PLANO_INVALIDO=-2,BACKUP_PLANO_LEITURA=-3,BACKUP_PLANO_NAO_SUPORTADO=-4 };
// Caller pauses store writers first; the session generation is checked again
// by the commit coordinator. This validates EVERY payload before any write.
int backupplano_criar(const BackupLocal *backup,const char *dono,int perfil,uint64_t sessao,
                      BackupLerAtual ler,void *ctx,BackupPlano **saida);
void backupplano_liberar(BackupPlano *p);
#endif
