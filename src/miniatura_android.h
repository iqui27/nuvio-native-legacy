// SEEK-BAR THUMBNAILS MADE ON THIS DEVICE (Android, test). A third provider for
// seekr.c next to Seekr.tv and OpenSeek: Settings > "Provedor das miniaturas" >
// "No aparelho (teste)".
//
// While a title plays, worker threads make a 480x270 thumbnail per 10 s from the
// nearest KEYFRAME of a SMALLER COPY of the same title from the source list
// (cached, 480p-1080p, same edition words, runtime within 10 %; the same file
// from two add-ons counts once), after a frame check that the two line up
// (OpenSeek's timeline check, backend/openseek/timeline.py: anchors, 32x18 grey
// signatures, a fine pass on exact copy frames, one offset/speed mapping or a
// rejection). Decoding: MiniaturaMotor.kt (the device decoder as a second
// session above 1080p; Android's software decoder first for the copies).
// Thumbnails are NEVER made from a file above 1080p: no copy, or none passing the
// check, means no thumbnails (the check reads ~20 frames of the playing file).
//
// The network is the player's first: one request in flight for the whole engine
// (~4/s, adaptive), the playing file's sockets closed after each read, nothing
// read while the player has < 20 s buffered, and a refused or silent connection
// ends the title (debrid CDNs limit connections per user).
//
// Order: where the person scrubs, +-1 min around the playhead, one every 5 min,
// +-5 % around the playhead, one every 60 s, the rest, then exact frames for real
// gaps. A set is kept on this device 2 h after its last use (miniaturas/, 200 MB
// cap); nothing is sent anywhere.
#ifndef NV_MINIATURA_ANDROID_H
#define NV_MINIATURA_ANDROID_H
#include "seekr.h"

// NULL outside the Android build.
const SeekrProvedor *miniatura_provedor(void);
// 1 for 4 s after the playing title's set completed: the player shows "Miniaturas geradas" in its pill.
int miniatura_prontas_agora(void);
// Four-second milestone in the existing player pill: 1=started, 2=50%, 3=complete, 4=cached.
int miniatura_status_agora(void);
// Current set belongs to the playing file, not a time-mapped copy.
int miniatura_direto(void);
// The player is OPENING a title (link known, not ready yet): open the file and create the thumbnail hardware decoder
// now, before the audio starts (creating it later causes an audible HDMI underrun). Cheap to call every frame.
void miniatura_preaquecer(const char *url, const char *cabecalhos);
#endif
