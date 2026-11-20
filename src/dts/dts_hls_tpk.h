/* DTS / TrueHD -> Dolby Digital on the Samsung .tpk: the local HTTP server the
 * TV's own player reads the converted stream from (docs/features/dts/README.md).
 * The converter is dts_remux.c.
 *
 * Delivery: HLS on 127.0.0.1, segments made when the player asks. A source
 * without a keyframe index to its end (MKV without Cues) gets one continuous
 * MPEG-TS instead, restarted on seek (video_tpk.c switches by itself).
 *
 * Every function is safe from the app thread: nothing here blocks on the
 * network. Shared by the .tpk and the webOS HLS test build. */
#ifndef NV_DTS_HLS_TPK_H
#define NV_DTS_HLS_TPK_H
#include <stddef.h>

enum {
  DTSTPK_HLS = 0,      /* HLS VOD, segments made on request              */
  DTSTPK_PROG_TS,      /* one continuous MPEG-TS, restarted on seek (?t=) */
  DTSTPK_DESLIGADO,    /* conversion off (Settings): as on master         */
  DTSTPK_MODOS
};
enum { DTSTPK_NADA = 0, DTSTPK_SONDANDO = 1, DTSTPK_PRONTO = 2, DTSTPK_FALHOU = -1 };

/* 1 when this build can convert (FFmpeg linked, decoders and muxer present). */
int dts_tpk_disponivel(void);
const char *dts_tpk_modo_nome(int modo);

/* New session for `url` (raw "Name: value" header lines): converts the k-th
 * audio track of the file, counted from 0 in container order (-1 = the first
 * DTS/TrueHD/MLP). The source is
 * probed in a thread; the URL written to `out` is what the player opens (its
 * requests wait for the probe). inicio: start of the first progressive URL.
 * 0 when the server cannot run. Replaces the previous session. */
int dts_tpk_abrir(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                  int modo, double inicio, char *out, size_t n);
#ifdef NV_WEBOS_HLS_TEST
#include "dts_remux.h"
/* HLS VOD with AC-3 audio. A P7 source uses fMP4 and the P8.1 converter. */
int dts_hls_webos_open(const char *url, const char *headers, int audio_ordinal,
                      const DtsWebosHls *options, char *out, size_t n);
/* The old session's workers still own their reader buffers. */
int dts_hls_webos_draining(void);
/* Snapshot a verified end for an audio change within the same source. */
void dts_hls_webos_video_end(const char *url, const char *headers, DtsHlsVideoEnd *);
/* Input payload and ring occupancy, excluding local HLS output. */
void dts_hls_webos_buffer_info(DtsBufferInfo *out);
/* Input tracks for the audio menu. The TV receives only the selected track. */
int dts_hls_webos_audio_tracks(DtsHlsAudioTrack *out, int max, int *selected);
#endif
/* Continuous TS: the URL that restarts the stream at s (same session). */
int dts_tpk_url_em(double s, char *out, size_t n);
/* Ends the session: cancels the probe and every generator, logs a summary. */
void dts_tpk_fechar(void);
int  dts_tpk_estado(void);
const char *dts_tpk_erro(void);          /* reason of DTSTPK_FALHOU, no URL */
double dts_tpk_duracao(void);            /* seconds, once ready */
int  dts_tpk_modo(void);                 /* mode of the session, -1 none */
/* Progressive: source time of the keyframe the CURRENT stream starts at (the
 * player counts from there). -1 until the stream's request has started. */
double dts_tpk_inicio_stream(void);
const char *dts_tpk_caminho(void);       /* decoder path, e.g. "DTS core only" */
/* Player prebuffer (ms) for the converted stream: video_tpk.c hands it to the
 * host as the pseudo header X-Nuvio-Prebuffer (player_set_streaming_buffering_time). */
int dts_tpk_prebuffer_ms(void);
/* 1 when the session FAILED because HLS needs a keyframe index the source
 * lacks (MKV without Cues): the continuous TS can still play it. */
int  dts_tpk_sem_indice(void);
int  dts_tpk_e_url(const char *url);     /* is this a URL of this server */

#endif
