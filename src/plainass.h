#ifndef NV_PLAINASS_H
#define NV_PLAINASS_H
#include "legenda.h"

/* Generated plain subtitles are distinct from authored ASS: the user's
 * controls still own the style. Logical Unicode stays unchanged for libass. */
typedef struct {
  char font[96];
  int size, rgb, background, border, bold, marginV;
  int outlineRgb;   // 0xRRGGBB; zero preserves the existing black edge
  int overrides;    // Authored ASS: only explicitly changed fields.
} PlainAssStyle;

#define PLAIN_ASS_COLOR   1
#define PLAIN_ASS_FONT    2
#define PLAIN_ASS_OUTLINE 4
#define PLAIN_ASS_BORDER  8
#define PLAIN_ASS_BG      16
#define PLAIN_ASS_POS     32
#define PLAIN_ASS_BOLD    64

extern int plainass_arabic_bold;
int plainass_has_arabic(const char *text);
/* NULL for a plain LTR-only track or allocation failure; caller frees. */
char *plainass_document(const LegendaCue *cues, int count);
#endif
