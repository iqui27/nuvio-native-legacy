#ifndef NV_BACKUPCATEGORIES_H
#define NV_BACKUPCATEGORIES_H
#include <stdint.h>
typedef struct {int tipo;unsigned antes,depois;} BackupCategoriaMapa;
typedef struct BackupCategorias BackupCategorias;
/* Portable hidden-category origins. A hash includes its original account ID;
 * keep its origin alongside the destination ID instead of guessing an inverse. */
int backupcategorias_ler(const char *,BackupCategorias **);
void backupcategorias_liberar(BackupCategorias *);
int backupcategorias_oculta(const BackupCategorias *,int tipo,unsigned id,const char *base,const char *nome);
uint64_t backupcategorias_chave(int tipo,unsigned id,const char *base,const char *nome);
int backupcategorias_mesclar(const char *destino,const char *destinoOcultas,const BackupCategoriaMapa *,int nd,
                            const char *origem,const char *ocultas,
                            const BackupCategoriaMapa *,int n,char **saida);
#endif
