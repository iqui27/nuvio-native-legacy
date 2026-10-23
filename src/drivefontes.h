#ifndef NV_DRIVEFONTES_H
#define NV_DRIVEFONTES_H
#include "drivelocal.h"
/* Worker-only encrypted font assets. Upload the main snapshot LAST; font
 * references are inside its authenticated ciphertext, never public metadata. */
int drivefontes_enviar(const DriveLocal *,const char *dir,const char *snapshot,size_t n,char **saida,size_t *);
int drivefontes_baixar(const DriveLocal *,const char *dir,BackupLocal *);
#endif
