// Source-failure classifier: who owns the problem (add-on/debrid, connection, app/TV).
#include <stdio.h>
#include <string.h>
#include "fontesonda.h"
static int falhas;
#define OK(c) do { if (!(c)) { printf("FALHOU %d: %s\n", __LINE__, #c); falhas++; } } while (0)
static int causa(const char *t, int *h) { return fontesonda_causa(t, h); }
int main(void) {
  int h;
  OK(causa("40404 server error:40404", &h) == FS_CAUSA_HTTP && h == 404);
  OK(causa("40403 server error:40403", &h) == FS_CAUSA_HTTP && h == 403);
  OK(causa("40500 server error:40500", &h) == FS_CAUSA_HTTP && h == 500);
  OK(causa("Response HTTP 410 Gone", &h) == FS_CAUSA_HTTP && h == 410);
  OK(causa("2004 player error:1", &h) == FS_CAUSA_HTTP && h == 0);     // Media3 bad status
  OK(causa("2005 player error:1", &h) == FS_CAUSA_HTTP && h == 404);
  OK(causa("2002 player error:1", &h) == FS_CAUSA_REDE);               // timeout
  OK(causa("[host] erro ConnectionFailed", &h) == FS_CAUSA_REDE);
  OK(causa("open: unreachable", &h) == FS_CAUSA_REDE);
  OK(causa("3003 player error:2", &h) == FS_CAUSA_FORMATO);
  OK(causa("4004 player error:2", &h) == FS_CAUSA_FORMATO);
  OK(causa("decoder: unsupported codec", &h) == FS_CAUSA_FORMATO);
  OK(causa("100 Playing error", &h) == FS_CAUSA_NENHUMA);
  OK(causa("", &h) == FS_CAUSA_NENHUMA && causa(NULL, &h) == FS_CAUSA_NENHUMA);
  OK(causa("40404 server error:40404", NULL) == FS_CAUSA_HTTP);
  if (!falhas) puts("ok fontesonda_causa");
  return falhas != 0;
}
