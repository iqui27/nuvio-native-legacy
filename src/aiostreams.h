// Read-only AIOStreams inventory. No configuration writes or playback probes.
#ifndef NV_AIOSTREAMS_H
#define NV_AIOSTREAMS_H
#include <stddef.h>
#define AIO_MAX 128
#define AIO_ID_PREFIX "com.aiostreams."
#define AIO_CATALOGO 1
#define AIO_STREAM 2
#define AIO_LEGENDA 4
#define AIO_META 8
#define AIO_ADDONS 16
typedef struct { char nome[128], id[128], tipo[32]; } AioCatalogo;
typedef struct { char nome[96]; int ativo, legenda; } AioItem;
typedef struct { char nome[192], tipo[24]; int cache, altura, mp4; } AioResultado;
typedef struct {
  int valida, requerida, configurada, recursos, catalogos, servicos, addons;
  int resultados, links, p2p, regras[8], http[4], ms[4], retry[4];
  char nome[96], versao[40], host[256], resumo[8][256];
  int regraCache, regraUncached;
  char resolucaoObrigatoria[128], resolucaoExcluida[128], idiomaPreferido[128];
  AioCatalogo catalogo[AIO_MAX];
  AioItem servico[32], addon[AIO_MAX];
  AioResultado resultado[AIO_MAX];
} AioSnapshot;
int aio_url(const char *url, char *origem, size_t n);
int aio_autorizacao(const char *manifesto, char *dst, size_t n);
int aio_manifesto(AioSnapshot *s, const char *json);
void aio_configuracao(AioSnapshot *s, const char *json);
void aio_resultados(AioSnapshot *s, const char *json);
#ifdef NV_SHOT_HOOKS
void aio_shot(const AioSnapshot *s);
#endif
#endif
