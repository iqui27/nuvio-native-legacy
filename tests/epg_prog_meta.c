#include "../src/xmltv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char gotDesc[512], gotIcon[256], gotTit[128];

static void onProg(const char *canal, const char *ini, const char *fim,
                   const char *titulo, const char *desc, const char *icon, void *u) {
  (void)canal; (void)ini; (void)fim; (void)u;
  snprintf(gotTit, sizeof gotTit, "%s", titulo ? titulo : "");
  snprintf(gotDesc, sizeof gotDesc, "%s", desc ? desc : "");
  snprintf(gotIcon, sizeof gotIcon, "%s", icon ? icon : "");
}

int main(void) {
  Xmltv x;
  const char *buf =
      "<?xml version=\"1.0\"?><tv>"
      "<programme channel=\"c7\" start=\"20260914223000 +0000\" stop=\"20260915010000 +0000\">"
      "<title>Prog dia 7</title>"
      "<desc>Sinopse exclusiva do programa</desc>"
      "<icon src=\"http://epg/icon7.png\"/>"
      "</programme></tv>";
  memset(&x, 0, sizeof x);
  x.programa = onProg;
  assert(xmltv_iniciar(&x));
  assert(xmltv_alimentar(&x, buf, strlen(buf)));
  xmltv_fim(&x);
  assert(!strcmp(gotTit, "Prog dia 7"));
  assert(!strcmp(gotDesc, "Sinopse exclusiva do programa"));
  assert(!strcmp(gotIcon, "http://epg/icon7.png"));
  puts("PASS: xmltv programa desc/icon");
  return 0;
}
