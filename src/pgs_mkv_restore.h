#ifndef NV_PGS_MKV_RESTORE_H
#define NV_PGS_MKV_RESTORE_H

#include <stddef.h>

#define PGS_MKV_BLOCO_MAX (8u << 20)

enum PgsMkvCodif {
  PGS_MKV_CODIF_NENHUM = 0,
  PGS_MKV_CODIF_ZLIB = 1,
  PGS_MKV_CODIF_TIRA = 2
};

/* Restaura um bloco PGS Matroska (zlib ou prefixo privado). Retorna buffer
 * novo (free pelo chamador) ou NULL em falha. */
unsigned char *pgs_mkv_restaurar(int codif, const unsigned char *tira, size_t tira_n,
                                 const unsigned char *bloco, size_t bloco_n,
                                 size_t *saida);

#endif
