#ifndef NV_BACKUPSTORES_H
#define NV_BACKUPSTORES_H
#include <stddef.h>
#include <stdint.h>
/* Explicit local-store IDs, never paths supplied by a downloaded backup.
 * Account snapshots, sessions, OAuth state, caches and migrations have no IDs.
 * In particular, Trakt/Discord rotating refresh tokens must never travel in a
 * backup: replaying them on another device can invalidate the original login. */
enum {
  BS_HOME_ROWS, BS_LIBRARY_UI, BS_COLLECTION_UI, BS_SAVED_UI, BS_SAVED_TABS,
  BS_CENTRAL_UI, BS_GUIDE_FAVORITES, BS_GUIDE_MODE, BS_GUIDE_PREVIEW,
  BS_GUIDE_ADDONS, BS_GUIDE_CATEGORIES, BS_GUIDE_REMINDERS,
  BS_GUIDE_CATEGORY_ALIASES,
  BS_SOURCE_ORDER, BS_BADGES_CHOICE, BS_BADGES_CUSTOM, BS_POSTER_CONNECTION,
  BS_FANART_KEY, BS_SEEKR_KEY, BS_P2P_CONNECTION, BS_CURATION,
  BS_DEBRID_SHARED, BS_DEBRID_PROFILE, BS_PROVIDER_SCOPE, BS_IPTV_REGISTRY,
  BS_XTREAM, BS_M3U, BS_STALKER, BS_JELLYFIN, BS_EMBY, BS_PLEX,
  BS_IMPORTED_FONT_CHOICES, BS_PINNED_LISTS, BS_AGENDA_REMINDERS,
  BS_PLUGINS_SHARED, BS_PLUGINS_PROFILE,
  BS_PIP, BS_STATS_SIZE, BS_STATS_TILES, BS_SUBTITLE_MEMORY, BS_AUDIO_MEMORY,
  BS_ART_MEMORY, BS_SOURCE_PREF_MANUAL, BS_EXTERNAL_PLAYER,
  BS_LTV_URLS, BS_LTV_PREFS, BS_LTV_FAVORITES, BS_LTV_EPG_MAP,
  BS_N
};
enum { BS_PROFILE=1, BS_CONNECTION=2, BS_HARDWARE=4, BS_MULTIPLE=8 };
#define BACKUP_STORES_MAX 256
#define BACKUP_STORES_BYTES (8u * 1024u * 1024u)
#define BACKUP_STORE_BYTES (4u * 1024u * 1024u)
typedef struct BackupStores BackupStores;
typedef struct {
  int tipo, existe;
  uint32_t sub;
  const char *texto;
  size_t n;
} BackupStore;
BackupStores *backupstores_novo(void);
void backupstores_liberar(BackupStores *);
const char *backupstores_tipo(int tipo);
unsigned backupstores_flags(int tipo);
int backupstores_adicionar(BackupStores *,int tipo,uint32_t sub,int existe,const char *,size_t);
int backupstores_remover(BackupStores *,int tipo,uint32_t sub);
int backupstores_definir(BackupStores *,int tipo,uint32_t sub,int existe,const char *,size_t);
BackupStores *backupstores_copiar(const BackupStores *);
int backupstores_n(const BackupStores *);
int backupstores_item(const BackupStores *,int indice,BackupStore *);
int backupstores_buscar(const BackupStores *,int tipo,uint32_t sub,BackupStore *);
int backupstores_exportar(const BackupStores *,char **,size_t *);
int backupstores_ler(const char *,size_t,BackupStores **);
/* Absent records are tombstones for rollback. Equality compares existing files,
 * independent of record order and explicit/missing absence markers. */
int backupstores_iguais(const BackupStores *,const BackupStores *);
/* Map a validated kind/sub-ID to this authenticated owner/profile only. */
int backupstores_reconhecer(const char *nome,const char *dono,int perfil,int *tipo,uint32_t *sub);
int backupstores_nome(int tipo,uint32_t sub,const char *dono,int perfil,char *,size_t);
#endif
