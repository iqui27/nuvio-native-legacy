// Hebrew and Arabic runs in authored ASS (owner 25/11/2026): libass has no
// glyph fallback over in-memory fonts, so each run names its face (see
// assrender_marcar_escrita). Pure text check; no libass needed.
#include "../src/assrender.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHALOM "\xd7\xa9\xd7\x9c\xd7\x95\xd7\x9d"
#define OLAM   "\xd7\xa2\xd7\x95\xd7\x9c\xd7\x9d"
#define MARHABA "\xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7"

static char *heb(const char *t) { return assrender_marcar_escrita(t, assrender_eh_hebraico, "{\\fnH}", "{\\fn}"); }
static char *ara(const char *t) { return assrender_marcar_escrita(t, assrender_eh_arabe, "{\\fnA}", "{\\fn}"); }

int main(void) {
  char *m;
  assert(!heb("Hello world") && !ara("Hello world"));
  assert(!heb(MARHABA) && !ara(SHALOM));
  m = heb(SHALOM " " OLAM "!"); assert(m && !strcmp(m, "{\\fnH}" SHALOM " " OLAM "{\\fn}!")); free(m);
  m = heb("Hi " SHALOM " there"); assert(m && !strcmp(m, "Hi {\\fnH}" SHALOM "{\\fn} there")); free(m);
  m = heb("{\\i1}" SHALOM "\\N" OLAM); assert(m && !strcmp(m, "{\\i1}{\\fnH}" SHALOM "{\\fn}\\N{\\fnH}" OLAM "{\\fn}")); free(m);
  m = heb("{\\fn" SHALOM "}x"); assert(!m || !strcmp(m, "{\\fn" SHALOM "}x")); free(m);   // inside braces: untouched
  m = ara(MARHABA "  " MARHABA); assert(m && !strcmp(m, "{\\fnA}" MARHABA "  " MARHABA "{\\fn}")); free(m);
  m = ara("\xd8"); assert(!m);                  // truncated UTF-8 is left alone
  puts("escritas_ass: Hebrew/Arabic runs marked ok");
  return 0;
}
