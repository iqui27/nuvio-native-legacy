#ifndef NV_DRIVEBACKUP_H
#define NV_DRIVEBACKUP_H
#include "backupapp.h"
#include <SDL2/SDL.h>
// Modal Settings route. Main/SDL thread owns every UI field; workers own
// immutable credentials/snapshots and can never write active preferences.
void drivebackup_abrir(void);
void drivebackup_fechar(void);
int drivebackup_aberto(void);
void drivebackup_evento(const SDL_Event *);
void drivebackup_atualizar(void);
void drivebackup_desenhar(void);
// Transfer an explicitly confirmed restore to the app's profile lifecycle.
// The app must revalidate before quiescing and restoring actual stores.
int drivebackup_restauro(BackupContexto *,BackupLocal **);
#endif
