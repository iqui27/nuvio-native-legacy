// Read-only AIOStreams inventory. No configuration writes or playback probes.
#ifndef NV_AIOSTREAMS_H
#define NV_AIOSTREAMS_H
#include <stddef.h>
#define AIO_MAX 128
#define AIO_DETAIL_MAX 2048
#define AIO_GROUPS 9
#define AIO_ID_PREFIX "com.aiostreams."
#define AIO_CATALOGO 1
#define AIO_STREAM 2
#define AIO_LEGENDA 4
#define AIO_META 8
#define AIO_ADDONS 16
typedef struct { char nome[128], id[128], tipo[32]; } AioCatalogo;
typedef struct { char nome[96]; int ativo, legenda; } AioItem;
typedef struct { char nome[192], tipo[24], destino[2048], provedor[96]; int cache, altura, mp4, rank, decisao; unsigned long long identidade; } AioResultado;
typedef struct { char idioma[48], nome[128]; int forcada, sdh; } AioLegenda;
typedef struct { int grupo; char nome[96], valor[256]; } AioRegra;
typedef struct { char titulo[128], descricao[384]; int erro; } AioEvidencia;
typedef struct {
  int valida, requerida, configurada, recursos, catalogos, servicos, addons;
  int resultados, links, p2p, regras[8], http[6], ms[6], retry[6];
  int legendas, detalhes, detalhesOmitidos, search, estatisticas, erros, filtrados, searchAtiva, p2pInstancia;
  char versaoInstancia[40], canal[24];
  int evidencias;
  AioEvidencia evidencia[32];
  AioLegenda legenda[AIO_MAX];
  AioRegra detalhe[AIO_DETAIL_MAX];
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
void aio_status(AioSnapshot *s, const char *json);
void aio_legendas(AioSnapshot *s, const char *json);
void aio_search(AioSnapshot *s, const char *json);
unsigned long long aio_identidade(const char *s);
#ifdef NV_SHOT_HOOKS
void aio_shot(const AioSnapshot *s);
#endif
#endif
