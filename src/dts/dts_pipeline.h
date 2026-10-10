#ifndef NV_DTS_PIPELINE_H
#define NV_DTS_PIPELINE_H
#include "dts_media.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct DtsPipeline DtsPipeline;
DtsPipeline *dts_pipeline_create(const char *, const char *, int,
                                void (*)(void *, const char *), void *);
int dts_pipeline_available(int);
/* A resposta acima fica guardada pelo processo. Esquecer serve a quem troca o
 * NUVIO_DTS_ADAPTER_DIR no meio (testes); o app nao precisa. */
void dts_pipeline_available_esquecer(void);
int dts_pipeline_load(DtsPipeline *, const DtsMediaInfo *, double);
/* 1 accepted, 0 retry with same frame, -1 fatal. Caller can release data on return. */
int dts_pipeline_feed(DtsPipeline *, const DtsFrame *);
int dts_pipeline_play(DtsPipeline *);
int dts_pipeline_pause(DtsPipeline *);
int dts_pipeline_flush(DtsPipeline *, double);
int dts_pipeline_eos(DtsPipeline *);
/* Optional native capability: returns zero when unavailable/refused. */
int dts_pipeline_volume(DtsPipeline *, int pct);
void dts_pipeline_destroy(DtsPipeline *);
const char *dts_pipeline_media_id(DtsPipeline *);
const char *dts_pipeline_error(DtsPipeline *);
#ifdef __cplusplus
}
#endif
#endif
