#ifndef NV_AUDIOEXTLISTA_H
#define NV_AUDIOEXTLISTA_H
// "FROM ANOTHER VERSION" IN THE AUDIO MENU (audioext.h): which other sources
// of the same title can lend their audio, and in what order.
//
// Pure: the caller turns each source of the list into an AudioExtEntrada
// (what the add-on declared: languages, audio codec, channels, cache, size)
// and gets back the indexes to show. Nothing here reads the network or the
// files; the real tracks are only known when the converter opens the file.
//
// Rules (owner, 06/11/2026, reworked 13-14/11 and 17/11/2026: the audio menu
// works like the subtitles' one, a list per language):
//  - only sources with a direct link, not HLS, not a warning page, not the
//    one playing;
//  - a source is listed under EVERY language it has (declared by the add-on
//    or hinted by its labels, streams.c), one row per language;
//  - inside a language, in this order (owner 17/11):
//      1. sources whose audio can be read without the picture's bytes
//         (MP4/MOV: the audio samples are read on their own), as they play;
//      2. MKV and other containers, as they play;
//      3. DTS / TrueHD (converted, wherever they are) last;
//    then, in each: not cached last, 5.1 or more first, the best audio
//    (FLAC, DD+ Atmos, DD+, DD, Opus, AAC, undeclared), smaller files first,
//    the list's order. "Sort like the addons" keeps 1-3 and orders inside
//    each by the add-ons' order;
//  - copies of the same file under one language (same hash, or same size,
//    codec, channels and languages) show once, the best one; the others stay
//    as its fallbacks;
//  - at most capPref rows per language of the preference (the first and
//    second audio language, and English: 24) and capOutro for any other (6),
//    like the subtitles' per-language caps (12 / 6). 0 = no cap.
#include <stdint.h>

// AEXT_FLAC / AEXT_OPUS play as they are (fMP4 renditions, owner 14-16/11).
enum { AEXT_DDP = 0, AEXT_DD, AEXT_AAC, AEXT_DESCONHECIDO, AEXT_DTS, AEXT_TRUEHD, AEXT_OUTRO, AEXT_FLAC, AEXT_OPUS };
#define AUDIOEXT_IDIOMAS 16
#define AUDIOEXT_LISTA_MAX 1000
#define AUDIOEXT_RESERVAS 3
// Groups: the preference's languages, no language, the others.
enum { AEXT_G_PREF1 = 0, AEXT_G_PREF2, AEXT_G_PREF3, AEXT_G_SEM, AEXT_G_OUTRA };

typedef struct AudioExtEntradaS {
  int temUrl;                       // direct link (not a torrent hash only)
  int hls, naoVideo, atual;         // excluded
  char idiomas[AUDIOEXT_IDIOMAS][8];// declared audio languages, ISO codes
  int nIdiomas;
  int codec;                        // AEXT_*
  int canais;                       // 2, 6, 8; 0 = unknown
  int cache;                        // 1 cached, 0 unknown, -1 not cached
  uint64_t bytes;                   // 0 = unknown
  char ident[64];                   // the file itself (hash, torrent:file); "" unknown
  int foraFiltro;                   // its add-on switched off in Settings > Addons
  int ordemAddon;                   // the add-on's place (Stream.addonOrdem), for "Sort like the addons"
  int atmos;                        // Dolby Atmos declared
  int soAudio;                      // MP4/MOV: its audio is read without the picture's bytes
  int legendas;                     // it declares subtitles (the sync matches by them first)
} AudioExtEntrada;

typedef struct {
  int indice;                       // into the caller's list
  char idioma[8];                   // the language it is listed under ("" none)
  int grupo;                        // AEXT_G_*
  int nReservas;                    // hidden copies of the same file
  int reservas[AUDIOEXT_RESERVAS];  // their indexes, best first
} AudioExtCand;

typedef struct {
  const char *pref1, *pref2, *pref3;   // "" / NULL skipped (pref3: English, as the subtitles)
  int capPref, capOutro;            // rows per language (0 = no cap)
  int ocultarForaCache, semDts;
  int ocultarCopias;                // one row per file (Settings, on by default)
  // "Sort like the addons": inside a language, the add-ons' own order
  // (the add-on's place, then the list's order) instead of cache/channels/
  // codec/size. Off by default.
  int ordemAddon;
  // compares languages (the app passes ling_casa); NULL: case-insensitive.
  int (*casa)(const char *, const char *);
} AudioExtOpcoes;

// The candidates in screen order. Returns the count (at most `max`).
int audioext_lista(const AudioExtEntrada *e, int n, const AudioExtOpcoes *o, AudioExtCand *out, int max);
// 1 when `a` and `b` are the same file (the rule above).
int audioext_mesmo_arquivo(const AudioExtEntrada *a, const AudioExtEntrada *b);
// 1 when this codec is converted on the way to the TV (DTS, TrueHD, other).
int audioext_converte(int codec);
// The tier of a version inside its language (0 audio-only container, 1 other
// container, 2 converted), as the list orders them.
int audioext_nivel(const AudioExtEntrada *e);

// "DD+ 5.1", "DD 2.0", "AAC 2.0", "FLAC 2.0", "Opus 5.1", "DTS 5.1", "TrueHD 7.1"; "" undeclared/other.
void audioext_codec_texto(int codec, int canais, char *out, unsigned tam);
// "6.1 GB", "850 MB"; "" when unknown. The decimal separator is the caller's.
void audioext_tamanho_texto(uint64_t bytes, char *out, unsigned tam);
#endif
