// AUDIO SYNC SESSION (F06, 1.8): decoded PCM of the playing audio -> speech
// segments -> a REFERENCE DOCUMENT for the AutoSync engine (autosync.c).
//
// Only a backend that really hands decoded PCM to the app registers here. On
// 04/10/2026 that is Android (Media3 sink tap, NvPlayer.kt + AudioSyncTap.kt).
// LG uMS, Samsung AVPlay (.wgt) and the .tpk players do not: capability stays
// PLATAFORMA and the UI says so (docs/plans/player-1.8/AUDIOSYNC-CAPACIDADE.md).
//
// FLOW (one session at a time, one worker thread, created on first request):
//   1. audsync_pedir arms the tap. The backend delivers mono 16 kHz int16 with
//      the media time of the first sample (audsync_pcm, playback thread). It
//      only copies into a bounded ring under a short lock: no allocation, never
//      blocks on the worker. Ring full = the chunk is dropped and the window
//      restarts (the timeline must be contiguous).
//   2. The worker runs the VAD (audvad.c) until `alvoSeg` of contiguous media
//      time was heard. A seek/pts jump, a format change or the UI pausing for
//      seek/buffer restarts the window. Bitstream (passthrough/offload) never
//      reaches the tap: it is reported as PASSTHROUGH and never switched off.
//   3. The tap is disarmed, the speech is aligned with the active external
//      subtitle (candidate offset) and two documents are built for the SAME
//      window: the speech segments (reference) and the external cues cropped
//      to it. legsync.c hands both to the engine; only an ACCEPTED engine
//      result changes the offset.
// Cancel: new session, source/track change, player close (legsync.c).
#ifndef NV_AUDSYNC_H
#define NV_AUDSYNC_H
#include "legenda.h"
#include "audvad.h"
#include <stdint.h>

// Manual audio presentation offset: positive delays audio; zero is unchanged.
#define AUDSYNC_OFFSET_MAX_MS 500
#define AUDSYNC_OFFSET_STEP_MS 20
static inline int audsync_offset_ms(int ms) {
  return ms < -AUDSYNC_OFFSET_MAX_MS ? -AUDSYNC_OFFSET_MAX_MS :
         ms > AUDSYNC_OFFSET_MAX_MS ? AUDSYNC_OFFSET_MAX_MS : ms;
}

// --- backend side (any thread) --------------------------------------------------------
enum { AUDSYNC_FMT_NENHUM = 0, AUDSYNC_FMT_PCM = 1, AUDSYNC_FMT_BITSTREAM = 2 };
// `ligar(1/0)` turns the platform tap on/off; called on the UI thread only.
void audsync_backend(void (*ligar)(int on));
void audsync_formato(int fmt);
// Returns 1 if the chunk was queued (tap armed and room in the ring).
int  audsync_pcm(const int16_t *mono16k, int n, int64_t ptsUs);

typedef enum {
  AUDSYNC_CAP_PLATAFORMA = 0, AUDSYNC_CAP_SEM_AUDIO, AUDSYNC_CAP_PASSTHROUGH, AUDSYNC_CAP_OK
} AudSyncCap;
AudSyncCap audsync_capacidade(void);

// --- session side (UI thread / legsync) -------------------------------------------------
typedef enum {
  AUDSYNC_PARADO = 0, AUDSYNC_OUVINDO, AUDSYNC_ALINHANDO, AUDSYNC_PRONTO, AUDSYNC_FALHOU
} AudSyncFase;
typedef enum {
  AUDSYNC_M_OK = 0, AUDSYNC_M_PLATAFORMA, AUDSYNC_M_PASSTHROUGH, AUDSYNC_M_SEM_FALA,
  AUDSYNC_M_CONTINUA, AUDSYNC_M_SEM_LEGENDA, AUDSYNC_M_CONFIANCA, AUDSYNC_M_JANELA,
  AUDSYNC_M_MEMORIA
} AudSyncMotivo;
const char *audsync_motivo(AudSyncMotivo m);   // stable English reason for logs

typedef struct {
  uint64_t pedido;
  AudSyncFase fase;
  AudSyncMotivo motivo;
  int progresso;        // 0..100 of the listening window
  int pausado;          // UI paused it (seek/buffer); window restarts on resume
  int reinicios;        // window restarts (seek, jump, overflow, format change)
  long descartados;     // chunks dropped because the ring was full
  int estimativaMs;     // candidate offset (audvad) once aligned
  double confianca;     // candidate score (heuristic, not a probability)
} AudSyncStatus;

#define AUDSYNC_ALVO_SEG 300     // listening window (engine needs >= 180 s overlap)
#define AUDSYNC_RAIO_MS  30000   // offset search radius for audio

// prim: the complete external document of THIS session (retained here).
uint64_t audsync_pedir(uint64_t sessao, LegendaDocumento *prim, int alvoSeg, int raioMs);
void audsync_cancelar(void);
void audsync_pausar(int sensivel);
void audsync_passo(void);                 // applies tap on/off on the UI thread
AudSyncStatus audsync_status(void);
// PRONTO only: hands over the reference and the cropped external document
// (caller owns both). 0 if `pedido` is not the current, finished request.
int  audsync_tomar(uint64_t pedido, LegendaDocumento **ref, LegendaDocumento **recorte);
void audsync_destruir(void);              // join; app teardown

// Tests: free space in the ring (samples) and its fixed capacity.
int audsync_teste_livre(void);
void audsync_teste_travar(int travado);
#define AUDSYNC_RING (AUDVAD_HZ * 4)
#endif
