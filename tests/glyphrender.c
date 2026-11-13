// Offline source text, through the production parser, layout and rasterizer.
#include "streams.h"
#include "text.h"
#include "js.h"
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int runs, missing, collect;
static char wrapped[4096];

void glyphrender_run(const char *text, TTF_Font *font) {
  runs++;
  const unsigned char *p = (const unsigned char *)text;
  while (*p) {
    unsigned cp = *p++;
    int n = cp < 0x80 ? 0 : cp < 0xE0 ? 1 : cp < 0xF0 ? 2 : 3;
    cp &= n ? (1u << (6 - n)) - 1 : 0x7f;
    while (n--) cp = (cp << 6) | (*p++ & 0x3f);
    if (!font || !TTF_GlyphIsProvided32(font, cp)) missing++;
  }
}

void glyphrender_line(const char *text) {
  if (collect) {
    assert(strlen(wrapped) + strlen(text) < sizeof wrapped);
    strcat(wrapped, text);
    int bytes = 0;
    assert(!strcmp(text, "☀︎") ||
           (txt_addon_emoji(text, &bytes) && bytes == (int)strlen(text)));
  }
}

int glyphrender_check(const Stream *streams) {
  // Surrogate escapes and raw UTF-8 must give identical provider strings.
  const char *escaped = "{\"v\":\"\\ud83d\\udc69\\ud83c\\udffd\\u200d\\ud83d\\udcbb\"}";
  char decoded[100];
  assert(js_texto_linhas(escaped, NULL, "v", decoded, sizeof decoded));
  assert(!strcmp(decoded, "👩🏽‍💻"));
  assert(!strcmp(streams[0].rotulo, "⚡ Offline 👩🏽‍💻\n4K · 🇧🇷"));
  assert(!strcmp(streams[0].descricao,
    "➤ Sample.Movie.2160p 🗎\n👨‍👩‍👧‍👦 Family · 👍🏽 Modifier\n"
    "🏳️‍🌈 Color · 🏴‍☠️ Sequence\n🇧🇷 🇺🇸 Flags · 1️⃣ Keycap\n"
    "☀︎ Text · ☀️ Color · ✓ Check\n🔊 Audio · 📦 12 GB · ⏱️ 2h"));

  const char *emoji[] = {"👩🏽‍💻", "👨‍👩‍👧‍👦", "🏳️‍🌈", "🏳‍🌈", "🏴‍☠️",
    "👍🏽", "🇧🇷", "🇺🇸", "1️⃣", "1⃣", "☀️", "🎞️", "⏱️",
    "👩🏽‍❤️‍💋‍👨🏻", "🏴\U000e0067\U000e0062\U000e0065\U000e006e\U000e0067\U000e007f"};
  for (unsigned i = 0; i < sizeof emoji / sizeof *emoji; i++) {
    int bytes = 0, w = 0, h = 0;
    if (!txt_addon_emoji(emoji[i], &bytes) || bytes != (int)strlen(emoji[i])) {
      fprintf(stderr, "FAIL: emoji cluster %u not consumed whole (%d/%zu bytes)\n", i, bytes, strlen(emoji[i]));
      return 1;
    }
    assert(txt_addon_test_surface(TXT_PG_FIM, emoji[i], &w, &h) > 0);
    assert(w > 0 && h > 0);
  }
  int bytes = 0, w = 0, h = 0;
  assert(!txt_addon_emoji("☀︎", &bytes));
  // VS15 on a later cluster must not suppress an earlier color emoji.
  assert(txt_addon_emoji("⚡☀︎", &bytes) && bytes == (int)strlen("⚡"));
  runs = missing = 0;
  assert(txt_addon_test_surface(TXT_PG_FIM, "➤ 🗎 ☀︎ ✓ RᴇLᴇAꜱᴇ Ω Ж 日本語 العربية ไทย", &w, &h) == 0);
  assert(runs > 0 && missing == 0 && w > 0 && h > 0);
  assert(txt_addon_test_surface(TXT_PG_FIM, "☀️", &w, &h) > 0);

  const char *clusters = "👩🏽‍💻👨‍👩‍👧‍👦👍🏽🇧🇷1️⃣";
  int lines = 0;
  collect = 1; wrapped[0] = 0;
  txt_addon_bloco(TXT_PG_FIM, clusters, 0, 0, 30, 255, 0, &lines, -1.0e9f, 1.0e9f);
  collect = 0;
  assert(lines == 5 && !strcmp(wrapped, clusters));
  wrapped[0] = 0; collect = 1;
  txt_addon_bloco(TXT_PG_FIM, "☀︎☀︎", 0, 0, 1, 255, 0, &lines, -1.0e9f, 1.0e9f);
  collect = 0;
  assert(lines == 2 && !strcmp(wrapped, "☀︎☀︎"));
  // Layout-only frames must reuse the measured block, with no new font runs.
  runs = 0;
  for (int i = 0; i < 30; i++)
    txt_addon_bloco(TXT_PG_FIM, clusters, 0, 0, 30, 255, 0, NULL, -1.0e9f, 1.0e9f);
  assert(runs == 0);
  printf("PASS: decoded Unicode unchanged; complete emoji clusters; color/text selectors; embedded fallback without tofu; atomic wrapping\n");
  return 0;
}
