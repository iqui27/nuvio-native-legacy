#ifndef NV_BACKUPFONTS_H
#define NV_BACKUPFONTS_H
#include <stddef.h>
#define BACKUP_FONT_N 8
#define BACKUP_FONT_CHUNK (8u*1024u*1024u)
#define BACKUP_FONT_BYTES (32u*1024u*1024u)
#define BACKUP_FONT_CHUNKS 4
typedef struct {
  char id[21];size_t n;int chunks;
  char remoto[BACKUP_FONT_CHUNKS][200];
} BackupFont;
typedef struct {BackupFont font[BACKUP_FONT_N];int n;} BackupFontManifest;
typedef struct BackupFontStage BackupFontStage;
// Startup only, after journal recovery and before any backup worker starts.
void backupfonts_limpar_pendentes(const char *dir);
int backupfonts_id(const char *);
int backupfonts_manifest_ler(const char *,size_t,BackupFontManifest *);
int backupfonts_manifest_exportar(const BackupFontManifest *,char **,size_t *);
/* Private immutable assets, independent of the active font registry. */
int backupfonts_enumerar(const char *dir,BackupFontManifest *);
int backupfonts_chunk_ler(const char *dir,const BackupFont *,int chunk,unsigned char **,size_t *);
BackupFontStage *backupfonts_stage(const char *dir);
void backupfonts_reter(BackupFontStage *);
void backupfonts_soltar(BackupFontStage *);
int backupfonts_stage_chunk(BackupFontStage *,const BackupFont *,int chunk,const void *,size_t);
int backupfonts_stage_completo(BackupFontStage *,const BackupFontManifest *);
int backupfonts_stage_validado(const BackupFontStage *);
void backupfonts_stage_validar(BackupFontStage *);
const BackupFontManifest *backupfonts_stage_manifest(const BackupFontStage *);
int backupfonts_stage_ler(const BackupFontStage *,int indice,unsigned char **,size_t *);
int backupfonts_stage_plano(const BackupFontStage *,char **,size_t *);
/* Fixed native font-file module for journal/rollback. No downloaded paths. */
int backupfonts_disco_ler(int raiz,char **,size_t *);
int backupfonts_disco_gravar(int raiz,const char *,size_t);
int backupfonts_plano(const char *antes,const char *entrada,char **rollback,char **depois);
int backupfonts_iguais(const char *,const char *);
#endif
