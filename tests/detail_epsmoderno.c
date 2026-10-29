#define main epcard_main
#include "detail_epcard.c"
#undef main

int main(void) {
  // The renderer must join only present values, including missing duration/date.
  char text[200];
  const char *parts[] = {"1080p", "HDR10", "DTS-HD MA 5.1"};
  for (int mask = 0; mask < 8; mask++) {
    char expected[200] = "";
    for (int i = 0; i < 3; i++) if (mask & (1 << i)) {
      if (expected[0]) strcat(expected, " · ");
      strcat(expected, parts[i]);
    }
    epMeta(text, sizeof text, mask & 1 ? parts[0] : NULL,
           mask & 2 ? parts[1] : "", mask & 4 ? parts[2] : NULL);
    assert(!strcmp(text, expected));
    assert(!strstr(text, "· ·"));
  }
  epMeta(text, sizeof text, NULL, "March 1, 2024", NULL);
  assert(!strcmp(text, "March 1, 2024"));
  char small[5] = {0}; epMeta(small, sizeof small, "1080p", "HDR10", "AAC");
  assert(small[sizeof small - 1] == 0);
  epMeta(NULL, 0, "1080p", NULL, NULL);
  puts("PASS: metadata joins all eight empty/non-empty combinations");
  return epcard_main();
}
