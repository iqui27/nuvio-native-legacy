#ifndef NV_TVHOME_H
#define NV_TVHOME_H
#include "catalogo.h"
#include "progresso.h"
#define TVHOME_MAX 24
/* Only title identity, public artwork and playback progress cross JNI. */
typedef struct {
  char id[24], type[8], title[160], episodeTitle[120];
  char poster[NV_ARTE_URL_MAX], backdrop[512];
  int season, episode, next;
  long long positionMs, durationMs, watchedMs;
} TvHomeItem;
int tvhome_select(const ProgRegistro *records, int count, const CatItem *cards, int cardCount,
                  int profile, int minimum, int completed, TvHomeItem *out, int capacity);
#endif
