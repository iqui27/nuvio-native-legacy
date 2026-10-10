#ifndef NV_AUTOSYNC_H
#define NV_AUTOSYNC_H
#include "legenda.h"
/* Positive offsets advance subtitles, like Legenda's atrasoMs. */
typedef enum { AUTOSYNC_QUICK, AUTOSYNC_THOROUGH } AutoSyncModo;
typedef enum {
  AUTOSYNC_UNAVAILABLE, AUTOSYNC_ANALYSING, AUTOSYNC_ACCEPTED,
  AUTOSYNC_REJECTED, AUTOSYNC_CANCELLED
} AutoSyncEstado;
typedef enum {
  AUTOSYNC_OK, AUTOSYNC_NO_REFERENCE, AUTOSYNC_INCOMPLETE,
  AUTOSYNC_FORCED_SIGNS, AUTOSYNC_SPARSE, AUTOSYNC_REPEATED,
  AUTOSYNC_LOW_CONFIDENCE, AUTOSYNC_AMBIGUOUS, AUTOSYNC_REGION_DISAGREEMENT,
  AUTOSYNC_SESSION_CHANGED, AUTOSYNC_BUDGET, AUTOSYNC_MEMORY,
  AUTOSYNC_EXCLUDED_REFERENCE, AUTOSYNC_INVALID_ARGUMENT
} AutoSyncMotivo;
typedef struct {
  AutoSyncModo modo;
  int toleranciaMs; /* residual alignment error, NOT search range */
  int raioBuscaMs;  /* maximum absolute offset, <=180000 (whole reference for CD1/CD2) */
  int orcamentoMs;  /* wall-time bound, <=20000 */
  int manterMs;     /* plain offset this small keeps the original timing (0..500) */
} AutoSyncConfig;
/* What an ACCEPTED result changes. OFFSET: offsetMs only. ESCALA: one linear
 * transform (framerate). TRECHOS: 2..4 pieces (cut/insertion) and/or line
 * anchors. Anything but OFFSET is applied through AutoSyncMapa. */
typedef enum { AUTOSYNC_T_OFFSET, AUTOSYNC_T_ESCALA, AUTOSYNC_T_TRECHOS } AutoSyncTipo;
typedef struct {
  AutoSyncEstado estado;
  AutoSyncMotivo motivo;
  int referenciaInvalida; // falha ao preparar a referência, não a externa
  int offsetMs, regioes, erroMs, tempoMs;
  double confianca, alternativa;
  uint64_t documento, referencia, sessao;
  AutoSyncTipo tipo;
  double escala;     /* video seconds per subtitle second (1 = same rate) */
  int trechos, ancoras, casadas, mantida;
  double cobertura;  /* matched external dialogue lines / all of them */
} AutoSyncResultado;
/* Video time -> subtitle time. Immutable once built; NULL = identity. */
typedef struct AutoSyncMapa AutoSyncMapa;
#define AUTOSYNC_SEM_LEGENDA (-1e9) /* nothing of this subtitle belongs here */
double autosync_mapa_tempo(const AutoSyncMapa *mapa, double posVideo);
void autosync_mapa_liberar(AutoSyncMapa *mapa);
typedef int (*AutoSyncCancelar)(void *usuario);
AutoSyncConfig autosync_config(AutoSyncModo modo);
const char *autosync_motivo(AutoSyncMotivo motivo); /* stable English event reason */
/* Pure comparison never changes playback/selection. Run on a worker. */
AutoSyncResultado autosync_comparar(const LegendaDocumento *doc,
                                   const LegendaDocumento *referencia,
                                   const AutoSyncConfig *config,
                                   AutoSyncCancelar cancelar, void *usuario);
/* The same, also handing over the map of an ACCEPTED result (caller frees;
 * NULL otherwise). A caller that can only apply offsetMs must check tipo. */
AutoSyncResultado autosync_alinhar(const LegendaDocumento *doc,
                                  const LegendaDocumento *referencia,
                                  const AutoSyncConfig *config,
                                  AutoSyncCancelar cancelar, void *usuario,
                                  AutoSyncMapa **mapa);
/* One background worker per player context, two independent language slots.
 * No network, track switch or playback wait. UI polls on its own thread and
 * uses manual+automatic as render offset. Context retains documents. */
typedef struct AutoSync AutoSync;
AutoSync *autosync_criar(void);
void autosync_destruir(AutoSync *sync); /* cancel + join at player teardown */
void autosync_iniciar(AutoSync *sync, uint64_t sessao);
int autosync_selecionar(AutoSync *sync, int slot, LegendaDocumento *doc);
int autosync_solicitar(AutoSync *sync, int slot, LegendaDocumento *referencia,
                       const AutoSyncConfig *config);
void autosync_cancelar(AutoSync *sync, int slot);
int autosync_manual(AutoSync *sync, int slot, int atrasoMs);
int autosync_offset_ms(AutoSync *sync, int slot);
/* Subtitle time for video `posSeg` from an accepted ESCALA/TRECHOS result
 * (posSeg itself otherwise). Add autosync_offset_ms on top, as before. */
double autosync_posicao(AutoSync *sync, int slot, double posSeg);
void autosync_desfazer(AutoSync *sync, int slot); /* preserves manual offset */
/* Excludes last reference in this session/selection; cancels stale work and
 * undoes auto correction. Caller picks another real reference and requests it. */
int autosync_tentar_outra(AutoSync *sync, int slot);
int autosync_referencia_permitida(AutoSync *sync, int slot,
                                  const LegendaDocumento *referencia);
AutoSyncResultado autosync_estado(AutoSync *sync, int slot);
#endif
