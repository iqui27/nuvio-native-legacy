#ifndef NV_BACKUPRESTAURO_H
#define NV_BACKUPRESTAURO_H
#include "backupplano.h"
enum { BACKUP_IO_DIARIO=BACKUP_PARTES_N };
typedef struct {
  void *ctx;
  // 1=present (malloc text), 0=absent, -1=read failure. Journal uses NULL/0 identity.
  int (*ler)(void *ctx,const char *dono,int perfil,int tipo,char **texto,size_t *n);
  // Atomic, DURABLE writes/removals. A false result can have changed a file;
  // the prepared journal remains available to recover that uncertainty.
  int (*gravar)(void *ctx,const char *dono,int perfil,int tipo,const char *texto,size_t n);
  int (*apagar)(void *ctx,const char *dono,int perfil,int tipo);
  int (*atual)(void *ctx,const char *dono,int perfil,uint64_t sessao);
} BackupIO;
enum {
  BACKUP_RESTORE_NADA=0,BACKUP_RESTORE_OK=1,BACKUP_RESTORE_LIMPEZA=2,
  BACKUP_RESTORE_INVALIDO=-1,BACKUP_RESTORE_CONTEXTO=-2,
  BACKUP_RESTORE_FALHOU=-3,BACKUP_RESTORE_RECUPERAR=-4
};
// Caller holds exclusive ownership of the selected stores through planning,
// commit/rollback and normal module reload. On RECUPERAR, keep those writers
// paused until recovery succeeds. No URL, token or arbitrary file path is used.
int backuprestauro_aplicar(const BackupPlano *p,const BackupIO *io);
// Run on the private local journal BEFORE loading preferences at startup.
// Prepared => restore old stores; committed => keep new stores and only clean up.
// Recovery deliberately uses the journal's validated original account/profile,
// which may differ from the current login after an interrupted session change.
int backuprestauro_recuperar(const BackupIO *io);
#endif
