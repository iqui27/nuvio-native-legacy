// ASS (libass) SETTINGS FOR THE SAMSUNG .tpk (owner, 23-26/10/2026).
//
// The frame-by-frame anime signs (the phone scene of Kaguya-sama, a "Signs &
// Songs" track with 36,944 blocks) made the subtitles late and jerky on the
// S95C. Each fix was tested on the TV behind its own option; the winners are
// now fixed here (the TPK column of assperf.c) and the test panel is gone.
//
// What the person can still change lives in Subtitles > Style on an ASS/SSA
// track (faixas.c): size and opacity of the dialogue, contrast and gamma to
// match the TV, render resolution and libass itself (on/off). They are kept in
// assperf.txt (data), on this TV only.
//
// ONLY THE .tpk CHANGES. assrender.c is shared with webOS, the .wgt and
// Android: there ASS_NOVO is 0 and assperf_get returns 0, the behaviour from
// before 23/10.
//
// BLUR: libass's own, on the CPU (NEON, 3 threads). A GPU blur was tried on
// the S95C (24-27/10) and removed: CPU only was faster and lighter there.
#ifndef NV_ASSPERF_H
#define NV_ASSPERF_H

#include <stddef.h>

/* THE ENGINE. Every .tpk (Tizen 4-9, armv7 NEON: the whole .tpk is built
 * with -mfpu=neon) uses the engine tuned on the S95C: libass's NEON bitmap
 * engine and 3 blur threads, Alpha 8-bit upload with a texture cache, upload
 * outside the queue lock, incremental batches with past events dropped after
 * 2 min (restored on a seek back), render 100 ms ahead with held frames,
 * render only on changes, authored ASS styles, the app's cue copy every 10 s,
 * subtitle blocks from the local relay with 6 MB parked ahead, attached fonts
 * up to 64 MB. ASS_NOVO = 0 everywhere else: the engine of before 23/10.
 * NV_ASS_NOVO turns it on in the computer tests. */
#if defined(NV_TPK) || defined(NV_ASS_NOVO)
#define ASS_NOVO 1
#else
#define ASS_NOVO 0
#endif

// The person's options (Subtitles > Style on an ASS/SSA track, .tpk):
enum {
  AP_ESCALA,       // render resolution: 0 100%, 2 75%, 5 Auto (100%, 75% while busy)
  AP_GAMA,         // gamma like the TV's slider: 0 off, 1..5 = -1..-5, 6..10 = +5..+1
  AP_CONTRASTE,    // contrast like the TV's: 0 = 30 (TV default) .. 30 = 0
  AP_LIBASS,       // 1 = libass draws ASS/SSA; 0 = dialogue only, as plain text
  AP_ASSTAM,       // dialogue size, %: 50..200 (100)
  AP_ASSOPAC,      // dialogue opacity, %: 20..100 (100)
  AP_N
};

// Current value (cheap). Outside the .tpk: always 0.
int  assperf_get(int id);
// Reads assperf.txt once (later calls do nothing).
void assperf_iniciar(void);
// Saves one of the person's options.
void assperf_definir(int id, int v);
// The person's options back to their defaults.
void assperf_restaurar(void);
// Changes with every assperf_definir (the worker re-applies).
unsigned assperf_versao(void);

// Fixed values of the engine (0 / the old value where ASS_NOVO is 0).
#define ASSPERF_FONTES_TETO ((size_t)(ASS_NOVO ? 64 : 16) * 1024u * 1024u)   // attached fonts, bytes
#define ASSPERF_FIOS        (ASS_NOVO ? 3 : 1)          // NEON blur threads
#define ASSPERF_ADIANTE_MS  (ASS_NOVO ? 100 : 0)        // render ahead
#define ASSPERF_PODA_MS     (ASS_NOVO ? 120000L : 0L)   // drop events that ended this long ago
#define ASSPERF_PARQUE      (ASS_NOVO ? (6L << 20) : 0L)   // relay blocks parked ahead, bytes
double assperf_escala(void);                 // render resolution now (Auto: the one in use)
// Subtitle colour like the video's (AP_GAMA, AP_CONTRASTE): the TV's picture
// processing only touches the video plane.
double assperf_gama(void);                   // exponent: 1.0 = off
double assperf_contraste(void);              // gain: 1.0 (30) .. 0.70 (0)

// Render resolution Auto and the busy-scene flag (picture sync in legimagem.c).
double assperf_escala_quadro(void);          // worker: the scale for the next render
void   assperf_escala_reset(void);           // track change: back to 100 %
void   assperf_escala_custo(double libassMs);   // worker: a render that changed the picture
void   assperf_carga_gl(double ms);          // GL thread: cost of a new frame (0 = playing tick)
// Worker: lines starting in the next second, their text size and the pictures
// in this frame (busy-scene forecast); and the same for the second starting
// 0.5 s ahead (render resolution Auto only, so it drops to 75 % just before).
void   assperf_previsao(int inicios, long chars, int imagens, int iniciosCedo, long charsCedo);
int    assperf_pesado(void);                 // 1 = busy scene now

#ifdef NV_ASSPERF_TESTE
void assperf_teste_definir(int id, int v);   // tests on the computer
#endif

#endif
