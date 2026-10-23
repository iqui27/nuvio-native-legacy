#ifndef NV_BACKUPCONNECTIONS_H
#define NV_BACKUPCONNECTIONS_H
#include "backupstores.h"
/* Pure account merge. Existing destination connections/order win; new IPTV
 * accounts get collision-free IDs. No disk writes or active module changes. */
int backupconnections_mesclar(const BackupStores *atual,const BackupStores *entrada,BackupStores **saida);
int backupconnections_selecionar(const BackupStores *,int hardware,BackupStores **saida);
int backupconnections_capturar(const BackupStores *atual,BackupStores **saida);
#endif
