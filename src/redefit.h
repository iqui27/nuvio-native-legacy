// StreamFit DURING playback (Android [rede] log, video_android.c): measured
// download against the source's declared bitrate. Bytes only count in seconds
// the loader was active, so a full buffer (loader paused) is never a deficit.
// Flags once per open: REDEFIT_BAIXAS consecutive 10 s windows below the
// declared bitrate, or REDEFIT_REBUF_MAX rebuffers within REDEFIT_REBUF_MS.
// A slow window only counts while the buffer is short (< REDEFIT_PISO_MS) or
// fell by REDEFIT_QUEDA_MS during it: with a steady, deep buffer the loader
// mostly downloads as fast as playback reads, so the speed is the scene's
// bitrate, not the link's (Homatics log 27/11/2026: 15 s buffer, no stall,
// 66.6 < 71.8 Mbps flagged and autoplay dropped a good source).
// Pure, no SDL: tests/redefit.sh compiles it on the Mac.
#ifndef NV_REDEFIT_H
#define NV_REDEFIT_H

#define REDEFIT_JANELA_S 10
#define REDEFIT_CARGA_MIN 8      // loader active >= 8 of 10 s to judge a window
#define REDEFIT_BAIXAS 3
#define REDEFIT_REBUF_MAX 2
#define REDEFIT_REBUF_MS 60000u
#define REDEFIT_PISO_MS 8000     // buffer below this: a slow window counts
#define REDEFIT_QUEDA_MS 1000    // buffer fell this much in the window: it counts
enum { REDEFIT_OK = 0, REDEFIT_VAZAO = 1, REDEFIT_REBUFFER = 2 };

typedef struct {
  long long bytes0, bytesJan;
  int segs, segsCarga, cargaUlt, baixas, rebufs, sinal, armado, bufIni, bufArmado;
  unsigned rebufMs[REDEFIT_REBUF_MAX];
} RedeFit;

// One 1 s stats tick. bytes = cumulative socket bytes (< 0 unknown), bufferMs =
// buffered media ahead (< 0 unknown: speed alone decides). Returns 1 when a
// window closes; *mbps = throughput while loading (-1 = unmeasured).
static inline int redefit_segundo(RedeFit *r, long long bytes, int carregando, int kbps,
                                  int bufferMs, double *mbps) {
  long long d = 0;
  int curto;
  if (!r->bufArmado) { r->bufIni = bufferMs; r->bufArmado = 1; }
  if (bytes >= 0 && r->armado && bytes >= r->bytes0) d = bytes - r->bytes0;
  else carregando = 0;   // unmeasured second (first, counter reset or unknown)
  r->armado = bytes >= 0; r->bytes0 = bytes;
  if (carregando) { r->bytesJan += d; r->segsCarga++; }
  if (++r->segs < REDEFIT_JANELA_S) return 0;
  *mbps = r->segsCarga ? r->bytesJan * 8.0 / (r->segsCarga * 1e6) : -1;
  curto = bufferMs < 0 || r->bufIni < 0 || bufferMs < REDEFIT_PISO_MS ||
          bufferMs <= r->bufIni - REDEFIT_QUEDA_MS;
  r->bufIni = bufferMs;
  if (r->segsCarga >= REDEFIT_CARGA_MIN && kbps > 0 && *mbps * 1000.0 < kbps && curto) {
    if (++r->baixas >= REDEFIT_BAIXAS && !r->sinal) r->sinal = REDEFIT_VAZAO;
  } else r->baixas = 0;
  r->cargaUlt = r->segsCarga;
  r->segs = r->segsCarga = 0; r->bytesJan = 0;
  return 1;
}

// A rebuffer while playing (not a seek or track switch: the caller filters).
static inline void redefit_rebuffer(RedeFit *r, unsigned agoraMs) {
  int i;
  r->rebufs++;
  for (i = 0; i < REDEFIT_REBUF_MAX - 1; i++) r->rebufMs[i] = r->rebufMs[i + 1];
  r->rebufMs[REDEFIT_REBUF_MAX - 1] = agoraMs;
  if (r->rebufs >= REDEFIT_REBUF_MAX && !r->sinal &&
      agoraMs - r->rebufMs[0] <= REDEFIT_REBUF_MS) r->sinal = REDEFIT_REBUFFER;
}
#endif
