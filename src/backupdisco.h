#ifndef NV_BACKUPDISCO_H
#define NV_BACKUPDISCO_H
#include "backuprestauro.h"
typedef struct BackupDisco BackupDisco;
typedef int (*BackupSessaoAtual)(void *ctx,const char *dono,int perfil,uint64_t sessao);
// Existing private native data directory only. Does not create directories,
// load preferences, lock other writers or claim persistence through IDBFS.
BackupDisco *backupdisco_abrir(const char *dir,BackupSessaoAtual atual,void *ctx);
void backupdisco_fechar(BackupDisco *d);
BackupIO backupdisco_io(BackupDisco *d);
// Before preferences or any writers start. A missing journal is a no-op,
// including installations that have never used backup. Negative => do not
// load partially restored preferences; retain the journal for recovery.
int backupdisco_recuperar_arranque(const char *dir);
#endif
