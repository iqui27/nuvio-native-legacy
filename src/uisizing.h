// Shared physical-panel heuristic. Resolution alone does not reveal TV size.
#ifndef NV_UISIZING_H
#define NV_UISIZING_H
#include <math.h>
#include <stdlib.h>
#include <ctype.h>

// Append only: the first four indexes are already saved in ajustes.txt.
#define UI_SIZE_AUTO 8
static inline float ui_size_inches(float inches) {
  if (!isfinite(inches) || inches < 20 || inches > 120) return 1.0f;
  return inches >= 75 ? .85f : inches <= 43 ? 1.15f : 1.0f;
}
static inline float ui_size_choice(int choice, float automatic) {
  static const float factors[] = {1.0f, 1.2f, 1.3f, 1.5f, .8f, .85f, .9f, 1.15f};
  if (choice == UI_SIZE_AUTO) return automatic;
  return choice >= 0 && choice < UI_SIZE_AUTO ? factors[choice] : 1.0f;
}
// Recognize the size prefix of TV model names (OLED83C4, UN85CU..., 50NANO...).
// ponytail: model inference is a heuristic; unknown model formats stay at 100%.
static inline float ui_size_model(const char *model) {
  if (!model) return 0;
  for (const char *p = model; *p && *p != '|' && *p != ' '; p++) {
    if (!isdigit((unsigned char)*p)) continue;
    char *end;
    long inches = strtol(p, &end, 10);
    if (end - p == 2 && inches >= 20 && inches <= 98) return (float)inches;
    p = end - 1;
  }
  return 0;
}
static inline float ui_size_diagonal(int width, int height, float xdpi, float ydpi) {
  if (width <= 0 || height <= 0 || !isfinite(xdpi) || !isfinite(ydpi) || xdpi <= 0 || ydpi <= 0) return 0;
  float inches = hypotf(width / xdpi, height / ydpi);
  return isfinite(inches) && inches >= 20 && inches <= 120 ? inches : 0;
}
static inline float ui_size_text(float body, float scale) {
  return scale < 1 && body * scale < 18 ? 18 / scale : body;
}
#endif
