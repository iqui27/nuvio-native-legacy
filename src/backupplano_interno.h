#ifndef NV_BACKUPPLANO_INTERNO_H
#define NV_BACKUPPLANO_INTERNO_H
#include "backupplano.h"
// Only the validated planner and transaction engine use this representation.
struct BackupPlano {
  char dono[81];int perfil;uint64_t sessao;long long criado;
  struct {int usada,existia;char *antes,*depois;size_t nAntes,nDepois;} parte[BACKUP_PARTES_N];
};
#endif
