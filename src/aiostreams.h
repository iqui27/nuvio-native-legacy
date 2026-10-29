// Read-only AIOStreams inventory. No configuration writes or playback probes.
#ifndef NV_AIOSTREAMS_H
#define NV_AIOSTREAMS_H
#include <stddef.h>
#define AIO_MAX 128
#define AIO_CATALOG_MAX 512 // Independent of Home request limits.
#define AIO_DETAIL_MAX 2048
#define AIO_GROUPS 9
#define AIO_ID_PREFIX "com.aiostreams."
#define AIO_CATALOGO 1
#define AIO_STREAM 2
#define AIO_LEGENDA 4
#define AIO_META 8
#define AIO_ADDONS 16
typedef enum { AIO_UNKNOWN, AIO_AVAILABLE, AIO_RESTRICTED, AIO_DISABLED, AIO_FAILED } AioCapability;
typedef struct { char nome[128], id[128], tipo[32]; } AioCatalogo;
typedef struct { char nome[96]; int ativo, legenda; } AioItem;
typedef struct { char nome[192], tipo[24], destino[2048], provedor[96]; int cache, altura, mp4, rank, decisao; unsigned long long identidade; } AioResultado;
typedef struct { char idioma[48], nome[128]; int forcada, sdh; } AioLegenda;
typedef struct { int grupo; char nome[96], valor[256]; } AioRegra;
// Negative values mean unreported; zero is measured. Text is allowlisted.
#define AIO_EVIDENCE_MAX 64
#define AIO_POINTS_MAX 32
typedef enum { AIO_NOTICE_UNKNOWN, AIO_SCRAPE, AIO_REMOVAL, AIO_INCLUDED, AIO_PIPELINE, AIO_FILTER, AIO_PRECOMPUTE, AIO_SERVICE_WRAP, AIO_VARIANTS, AIO_DIGITAL, AIO_SOURCE_ERROR } AioEvidenceKind;
typedef struct { char label[40]; double ms; int count, cached, uncached, unclassified, error, approximate; } AioPoint;
typedef struct {
  char titulo[128], descricao[384], provider[32], service[16];
  int erro, status, streams, approximate, points, pointsOmitted, timeout;
  double elapsedMs;
  AioEvidenceKind kind;
  AioPoint point[AIO_POINTS_MAX];
} AioEvidencia;
typedef struct {
  AioCapability manifestAccess, searchAccess, configAccess, analyticsAccess;
  int valida, requerida, configurada, recursos, catalogos, servicos, addons;
  int resultados, links, p2p, playable, regras[8], http[6], ms[6], retry[6];
  int legendas, detalhes, detalhesOmitidos, search, estatisticas, erros, filtrados, searchAtiva, p2pInstancia;
  char versaoInstancia[40], canal[24];
  int evidencias, evidenceOmitted, responseValid;
  AioEvidencia evidencia[AIO_EVIDENCE_MAX];
  AioLegenda legenda[AIO_MAX];
  AioRegra detalhe[AIO_DETAIL_MAX];
  char nome[96], versao[40], host[256], resumo[8][256];
  int regraCache, regraUncached;
  char resolucaoObrigatoria[128], resolucaoExcluida[128], idiomaPreferido[128];
  int catalogTotal, catalogOmitted;
  AioCatalogo catalogo[AIO_CATALOG_MAX];
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
void aio_capability(AioCapability *capability, int http);
unsigned long long aio_identidade(const char *s);
void aio_evidence(AioSnapshot *s, const char *entry, const char *end, int error, int search);
#ifdef NV_SHOT_HOOKS
void aio_shot(const AioSnapshot *s);
#endif
#endif
