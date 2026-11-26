#ifndef NUVIO_TBQUEIMA_H
#define NUVIO_TBQUEIMA_H
/* TORBOX BURN-IN ON LG (video.c) AND ANDROID (video_android.c), owner 26/11/2026.
 *
 * The controller the Samsung .tpk has inside video_tpk.c (TORBOX BURN-IN
 * block), ported for the two other native players: TorBox's own audio only
 * (no Passthrough; the Original audio row stays hidden there), lean logs.
 * tbburn.c asks TorBox; this decides when, hands TorBox's playlist to the
 * player and goes back to the original file. It provides the video_tpk_*
 * burn-in calls of tbburn.h, so faixas.c / legendasui.c / app.c / player.c
 * call the same names on every native player (video_tpk.c keeps its own).
 *
 * The media URL (video_url_atual) stays the ORIGINAL file all along: Nuvio's
 * subtitles, sync's reference, resume, chapters and thumbnails keep reading
 * the MKV. Only the player's URL changes (tbq_url_player). While burned, the
 * menus list the FILE's tracks (snapshot taken at the switch); the burned
 * stream's own tracks are never shown. */
#include "video.h"

#if (defined(NV_WEBOS) || defined(NV_ANDROID) || defined(NV_TBQ_TEST)) && !defined(NV_TPK) && !defined(__EMSCRIPTEN__)
#define NV_TBQ 1
#endif

/* What the player says this frame (backend -> controller). */
typedef struct {
  int ativo;       // a player session is open
  int pronto;      // it has a picture (video_pronto)
  int tocando;     // it is playing
  int permitido;   // side connections / reopens allowed for this source (not a trailer, not a channel)
  int trailer;
  double pos;      // the player's clock (s)
  double dur;      // its duration (s)
} TbqVista;

#ifdef NV_TBQ
/* video_tocar / video_parar: a new source (or none). Drops any request. */
void tbq_tocar(void);
/* Every frame from video_bombear. */
void tbq_bombear(const TbqVista *v);
/* 1 = the player plays (or is opening) TorBox's burned stream. */
int  tbq_ativo(void);
/* The URL the player opens: TorBox's playlist while burned, else NULL (the
 * original, with the add-on's headers). TorBox's URL takes no headers. */
const char *tbq_url_player(void);
/* A player error (or a stall the backend would reconnect) while burned:
 * 1 = handled here (back to the original, or a stuck item re-added); the
 * backend then does nothing else with it. 0 = not burned: the usual path. */
int  tbq_erro_player(int cod);
/* LG: the app leaves (Home/sleep) while burned. The reopen will be the
 * original at *alvo (moved to the burned picture's time); burn-in asks TorBox
 * again once it plays. */
void tbq_fundo(double *alvo);
/* A seek the player is sent while burned (the subtitle clock, tbrelogio.h). */
void tbq_buscar(double s);
/* The menus while burned: the file's tracks (snapshot). Valid when tbq_ativo(). */
int  tbq_n_audio(void);
int  tbq_n_legenda(void);
const VideoFaixa *tbq_audio(int i);
const VideoFaixa *tbq_legenda(int i);
int  tbq_audio_atual(void);
/* An audio pick: burned = another TorBox stream with that audio (1, nothing
 * goes to the player); asking = the request follows it (0: the original
 * player switches too); else 0. */
int  tbq_escolher_audio(int i);

/* Backend side (video.c on webOS, video_android.c): reopens the player at
 * `alvo` with tbq_url_player() (NULL = the original). `audio`: the menu track
 * to pick on the original once it lists its tracks (-1 = its usual choice). */
void tbq_host_reabrir(double alvo, int audio);
#else
// Other targets (the .tpk has its own in video_tpk.c): never burned.
static inline void tbq_tocar(void) {}
static inline void tbq_bombear(const TbqVista *v) { (void)v; }
static inline int  tbq_ativo(void) { return 0; }
static inline const char *tbq_url_player(void) { return 0; }
static inline int  tbq_erro_player(int cod) { (void)cod; return 0; }
static inline void tbq_fundo(double *alvo) { (void)alvo; }
static inline void tbq_buscar(double s) { (void)s; }
static inline int  tbq_n_audio(void) { return 0; }
static inline int  tbq_n_legenda(void) { return 0; }
static inline const VideoFaixa *tbq_audio(int i) { (void)i; return 0; }
static inline const VideoFaixa *tbq_legenda(int i) { (void)i; return 0; }
static inline int  tbq_audio_atual(void) { return 0; }
static inline int  tbq_escolher_audio(int i) { (void)i; return 0; }
#endif

/* ---- pure (tests/tbqueima.sh) -------------------------------------------- */
/* 1 = a Matroska ASS/SSA codec id. */
int  tbq_codec_ass(const char *codec);
/* 1 = a video_hdr() value that says HDR (not "", "none", "sdr"). */
int  tbq_hdr_texto(const char *h);
#endif
