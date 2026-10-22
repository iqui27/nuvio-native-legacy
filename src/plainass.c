#include "plainass.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef NV_ASS_LIBASS
#include <fribidi/fribidi.h>

static int utf8_char(const char *s, uint32_t *cp) {
  const unsigned char *u = (const unsigned char *)s;
  *cp = *u;
  if ((*u & 0xe0) == 0xc0 && u[1]) {
    *cp = ((*u & 31u) << 6) | (u[1] & 63u); return 2;
  }
  if ((*u & 0xf0) == 0xe0 && u[1] && u[2]) {
    *cp = ((*u & 15u) << 12) | ((u[1] & 63u) << 6) | (u[2] & 63u); return 3;
  }
  if ((*u & 0xf8) == 0xf0 && u[1] && u[2] && u[3]) {
    *cp = ((*u & 7u) << 18) | ((u[1] & 63u) << 12) |
          ((u[2] & 63u) << 6) | (u[3] & 63u); return 4;
  }
  return 1;
}

/* Some SRT editors put an invisible LRM before a speaker marker. That
 * makes a neutral dash appear at the left of otherwise readable Arabic.
 * Establish RTL only for a leading dialogue marker followed by Arabic
 * as the first substantive strong letter. Do not move punctuation or
 * reverse text; remove leading LRM but preserve embedded controls. */
static const char *arabic_dialogue(const char *s) {
  uint32_t cp; int len; const char *marker;
  while (*s) {
    len = utf8_char(s, &cp);
    if (cp != ' ' && cp != '\t' && cp != 0x200e && cp != 0x200f && cp != 0xfeff) break;
    s += len;
  }
  if (!*s) return 0;
  len = utf8_char(s, &cp);
  if (cp != '-' && !(cp >= 0x2010 && cp <= 0x2014)) return 0;
  marker = s; s += len;
  /* A dash attached to a digit is a numeric minus, not a speaker marker. */
  utf8_char(s, &cp);
  if ((cp >= '0' && cp <= '9') || (cp >= 0x0660 && cp <= 0x0669) ||
      (cp >= 0x06f0 && cp <= 0x06f9)) return 0;
  while (*s && *s != '\n' && *s != '\r') {
    FriBidiCharType type;
    len = utf8_char(s, &cp); s += len;
    if (cp == 0x200e || cp == 0x200f) continue;
    type = fribidi_get_bidi_type(cp);
    if (type == FRIBIDI_TYPE_AL) return marker;
    if (type == FRIBIDI_TYPE_LTR || type == FRIBIDI_TYPE_RTL) return 0;
  }
  return 0;
}
#else
static const char *arabic_dialogue(const char *s) { (void)s; return NULL; }
#endif

int plainass_has_arabic(const char *text) {
  const unsigned char *p = (const unsigned char *)text;
  if (!p) return 0;
  while (*p) {
    uint32_t cp; int n;
    if (*p < 0x80) { p++; continue; }
    if ((*p & 0xe0) == 0xc0 && p[1]) {
      cp = ((*p & 31u) << 6) | (p[1] & 63u); n = 2;
    } else if ((*p & 0xf0) == 0xe0 && p[1] && p[2]) {
      cp = ((*p & 15u) << 12) | ((p[1] & 63u) << 6) | (p[2] & 63u); n = 3;
    } else if ((*p & 0xf8) == 0xf0 && p[1] && p[2] && p[3]) {
      cp = ((*p & 7u) << 18) | ((p[1] & 63u) << 12) |
           ((p[2] & 63u) << 6) | (p[3] & 63u); n = 4;
    } else { p++; continue; }
    if ((cp >= 0x0600 && cp <= 0x06ff) ||
        (cp >= 0x0750 && cp <= 0x077f) ||
        (cp >= 0x0870 && cp <= 0x089f) ||
        (cp >= 0x08a0 && cp <= 0x08ff) ||
        (cp >= 0xfb50 && cp <= 0xfdff) ||
        (cp >= 0xfe70 && cp <= 0xfefc) ||
        (cp >= 0x1ee00 && cp <= 0x1eeff))
      return 1;
    p += n;
  }
  return 0;
}

/* ASS has no \\ escape. A zero-width no-break space after a literal
 * backslash stops it forming an ASS escape/tag, without reversing text.
 * Braces have supported literal escapes. Newlines alone become \N. */
static void append(char *dst, size_t *n, const char *s, size_t len) {
  if (dst) memcpy(dst + *n, s, len);
  *n += len;
}

static size_t escape_text(char *dst, const char *src) {
  size_t n = 0; int arabicFont = 0, lineStart = 1;
  const char *dialogueMarker = NULL;
  while (*src) {
    const unsigned char *u = (const unsigned char *)src;
    unsigned cp = *u; int len = 1, arabic, control;
    if (lineStart) {
      dialogueMarker = arabic_dialogue(src);
      if (dialogueMarker) append(dst, &n, "\xe2\x80\x8f", 3);
      lineStart = 0;
    }
    if ((*u & 0xe0) == 0xc0 && u[1]) {
      cp = ((*u & 31u) << 6) | (u[1] & 63u); len = 2;
    } else if ((*u & 0xf0) == 0xe0 && u[1] && u[2]) {
      cp = ((*u & 15u) << 12) | ((u[1] & 63u) << 6) | (u[2] & 63u); len = 3;
    } else if ((*u & 0xf8) == 0xf0 && u[1] && u[2] && u[3]) {
      cp = ((*u & 7u) << 18) | ((u[1] & 63u) << 12) |
           ((u[2] & 63u) << 6) | (u[3] & 63u); len = 4;
    }
    /* Remove only an editor's leading LRM before a recognized Arabic
     * speaker marker. Leaving it there can bind the dash to an EN digit. */
    if (dialogueMarker && src < dialogueMarker && cp == 0x200e) { src += len; continue; }
    arabic = (cp >= 0x0600 && cp <= 0x06ff) ||
             (cp >= 0x0750 && cp <= 0x077f) ||
             (cp >= 0x0870 && cp <= 0x089f) || (cp >= 0x08a0 && cp <= 0x08ff) ||
             (cp >= 0xfb50 && cp <= 0xfdff) || (cp >= 0xfe70 && cp <= 0xfefc) ||
             (cp >= 0x1ee00 && cp <= 0x1eeff);
    control = cp == 0x200c || cp == 0x200d || cp == 0x200e || cp == 0x200f ||
              (cp >= 0x202a && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069);
    /* libass normalizes each face by its ascender + descender. Naskh's
     * tall metrics (1703 / 1000 em) make its alef 0.394 of the requested
     * size, versus Inter's cap height 0.601. A 1.5x script scale balances
     * the visible letters with Latin/digits without changing the shared
     * size setting. Scale both axes to preserve letter proportions.
     * The SDK disables system font providers, so select the face explicitly.
     * Whole-event layout still shapes/BiDi-processes across these tags. */
    if (!control && arabic != arabicFont) {
      const char *tag = arabic ? "{\\fnNoto Naskh Arabic\\fscx150\\fscy150}"
                               : "{\\fn\\fscx100\\fscy100}";
      append(dst, &n, tag, strlen(tag)); arabicFont = arabic;
    }
    if (cp == '\r') { src++; continue; }
    if (cp == '\n') { append(dst, &n, "\\N", 2); lineStart = 1; }
    else if (cp == '{' || cp == '}') {
      append(dst, &n, "\\", 1); append(dst, &n, src, 1);
    } else if (cp == '\\') append(dst, &n, "\\\xef\xbb\xbf", 4);
    else append(dst, &n, src, len);
    src += len;
  }
  if (arabicFont) {
    static const char reset[] = "{\\fn\\fscx100\\fscy100}";
    append(dst, &n, reset, sizeof reset - 1);
  }
  return n;
}

static void timestamp(char *dst, double seconds) {
  long long cs = llround(seconds * 100.0);
  if (cs < 0) cs = 0;
  snprintf(dst, 48, "%lld:%02lld:%02lld.%02lld", cs / 360000,
           cs / 6000 % 60, cs / 100 % 60, cs % 100);
}

char *plainass_document(const LegendaCue *cues, int count) {
  static const char header[] =
    "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\n"
    "WrapStyle: 0\nScaledBorderAndShadow: yes\n"
    "[V4+ Styles]\n"
    "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
    /* Encoding -1 opts into libass paragraph direction detection and
     * whole-event layout across the font overrides in mixed text. */
    "Style: Default,Inter Display,48,&H00FFFFFF,&H00FFFFFF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,0,2,130,130,80,-1\n"
    "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
  size_t cap = sizeof header; int rtl = 0, i; char *doc, *out;
  if (!cues || count <= 0 || count > 8000) return NULL;
  for (i = 0; i < count; i++) {
    size_t len = strnlen(cues[i].texto, sizeof cues[i].texto);
    if (len == sizeof cues[i].texto) return NULL;
    rtl |= plainass_has_arabic(cues[i].texto);
    cap += escape_text(NULL, cues[i].texto) + 180u;
  }
  if (!rtl || !(doc = malloc(cap))) return NULL;
  memcpy(doc, header, sizeof header - 1); out = doc + sizeof header - 1;
  for (i = 0; i < count; i++) {
    char start[48], end[48];
    if (!isfinite(cues[i].inicio) || !isfinite(cues[i].fim) ||
        cues[i].inicio < 0 || cues[i].fim <= cues[i].inicio ||
        cues[i].fim > 36000000.0) continue;
    timestamp(start, cues[i].inicio); timestamp(end, cues[i].fim);
    out += sprintf(out, "Dialogue: 0,%s,%s,Default,,0,0,0,,", start, end);
    out += escape_text(out, cues[i].texto); *out++ = '\n';
  }
  *out = 0;
  return doc;
}
