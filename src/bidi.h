// Arabic shaping + RTL ordering for ONE already-wrapped line of subtitle text.
//
// SDL_ttf here has no HarfBuzz (Android, probably webOS) and draws one
// codepoint at a time: Arabic came out as isolated letters in left-to-right
// order (#239, #245, #247). This turns a logical-order UTF-8 line into the
// string the glyph-per-codepoint renderer must draw: Arabic letters replaced by
// their Presentation Forms-B (contextual) codepoints, the whole line in VISUAL
// order. Self-contained, no dependency (libass/fribidi stay untouched: embedded
// ASS never goes through here).
//
// Wrap and measure on the LOGICAL text first, then call this per wrapped line.
#ifndef NUVIO_BIDI_H
#define NUVIO_BIDI_H
#include <stddef.h>

// Writes the visual form of `in` into `out` (NUL terminated, at most tam bytes).
// Returns 0 when the text needs no change (no Hebrew/Arabic codepoint): `out`
// is then a byte-identical copy. Returns 1 when it was reshaped/reordered.
// Returns -1 when `out` was too small for the result or the line is too long
// to process: `out` then holds the unchanged text, truncated at a character
// boundary. Never writes past tam; tam == 0 writes nothing.
int bidi_visual_utf8(const char *in, char *out, size_t tam);

// Same, but shaping only uses a Presentation Forms codepoint (and the lam-alef
// ligature) when tem_glifo(cp, ctx) returns non-zero for it; a letter whose form
// the font lacks keeps its original U+06xx codepoint (the line is still put in
// visual order). tem_glifo == NULL means "everything is available".
int bidi_visual_utf8_ex(const char *in, char *out, size_t tam,
                        int (*tem_glifo)(unsigned cp, void *ctx), void *ctx);

// Algum codepoint arabe em UTF-8: U+0600-06FF (D8-DB), U+0740-07BF (DD),
// U+FB50-FDFF e U+FE70-FEFF (EF AD-B7 / B9-BB). text.c e assrender.c (#335).
static inline int bidi_tem_arabe(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  for (; p && *p; p++) {
    if ((*p >= 0xD8 && *p <= 0xDB) || *p == 0xDD) return 1;
    if (*p == 0xEF && ((p[1] >= 0xAD && p[1] <= 0xB7) || (p[1] >= 0xB9 && p[1] <= 0xBB))) return 1;
  }
  return 0;
}

#endif
