#ifndef NV_BACKUPLOCAL_H
#define NV_BACKUPLOCAL_H
#include <stddef.h>
// Local preferences only. These identifiers are not filesystem paths.
enum {
  BACKUP_AJUSTES_APARELHO, BACKUP_AJUSTES_PERFIL, BACKUP_PLAYER,
  BACKUP_FONTES_COMPARTILHADAS, BACKUP_FONTES_PERFIL, BACKUP_FILEIRAS,
  BACKUP_AJUSTES_HARDWARE, BACKUP_LOCAIS, BACKUP_FONTES_IMPORTADAS,
  BACKUP_PARTES_N
};
#define BACKUP_LOCAL_MAX (24u * 1024u * 1024u)
#define BACKUP_PARTE_MAX (9u * 1024u * 1024u)
typedef struct BackupLocal BackupLocal;
typedef struct BackupFontStage BackupFontStage;
// Local-only staged assets are never serialized as filesystem paths.
void backuplocal_fontes_vincular(BackupLocal *,BackupFontStage *);
BackupFontStage *backuplocal_fontes(const BackupLocal *);
BackupLocal *backuplocal_novo(const char *dono,int perfil,long long criado);
void backuplocal_liberar(BackupLocal *b);
// Copies bounded text. Local connections live only in an encrypted transport;
// module IDs are fixed allowlisted stores, never arbitrary downloaded paths.
int backuplocal_adicionar(BackupLocal *b,int tipo,const char *texto,size_t n);
const char *backuplocal_parte(const BackupLocal *b,int tipo,size_t *n);
int backuplocal_quantidade(const BackupLocal *b);
long long backuplocal_criado(const BackupLocal *b);
int backuplocal_compativel(const BackupLocal *b,const char *dono,int perfil);
// Read-only metadata for listing/local journal recovery, not authorization.
// Imports still call backuplocal_ler with the CURRENT authenticated identity.
int backuplocal_identidade(const char *texto,size_t n,char *dono,size_t cap,int *perfil);
// Allocates output. The caller frees it. CRC32 detects accidental corruption;
// it is not authentication. Module payloads still need their owners' validation.
int backuplocal_exportar(const BackupLocal *b,char **saida,size_t *n);
// Requires the current account and stable server profile ID, never a slot mapping.
// No file writes or runtime changes; failure leaves output NULL.
int backuplocal_ler(const char *texto,size_t n,const char *dono,int perfil,BackupLocal **saida);
#endif
