#ifndef NV_PLAINASS_H
#define NV_PLAINASS_H
#include "legenda.h"

/* Generated plain subtitles are distinct from authored ASS: the user's
 * controls still own the style. Logical Unicode stays unchanged for libass. */
typedef struct {
  char font[96];
  int size, rgb, background, border, bold, marginV;
} PlainAssStyle;

int plainass_has_arabic(const char *text);
/* NULL for a plain LTR-only track or allocation failure; caller frees. */
char *plainass_document(const LegendaCue *cues, int count);
#endif
