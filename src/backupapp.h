#ifndef NV_BACKUPAPP_H
#define NV_BACKUPAPP_H
#include "backuplocal.h"
#include <stdint.h>
typedef struct {char usuario[81];int perfil;uint64_t sessao;} BackupContexto;
// Main-thread API; callers must not already hold the profile/session mutex.
// Capture local identity without refreshing tokens or starting network I/O.
int backupapp_contexto(BackupContexto *saida);
int backupapp_capturar(const BackupContexto *,long long criado,BackupLocal **saida);
// Pure module validation for the restore preview, before closing any UI.
int backupapp_selecionar(const BackupContexto *,const BackupLocal *,int hardware,BackupLocal **saida);
int backupapp_validar(const BackupContexto *,const BackupLocal *);
// Main thread, after the app has stopped playback/preloads and closed the
// outgoing profile's transient UI. Selected store writers run on this thread;
// old account/profile workers are invalidated by that normal lifecycle first.
// Preserves this device's account-sharing policy and reloads through the normal
// Settings/player owners before returning. Uses BACKUP_RESTORE_* results.
// RECUPERAR => caller must keep writers stopped and restart for recovery.
int backupapp_restaurar(const BackupContexto *,const BackupLocal *);
#endif
