// #458: duplicate torrent rows collapse in the sheet.
#include "streams.h"
#include <assert.h>
#include <string.h>
void wsenvio_podar(void) {}
#include "../src/streams.c"
int main(void) {
  Stream s[3];
  memset(s, 0, sizeof s);
  snprintf(s[0].infoHash, sizeof s[0].infoHash, "%s", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  snprintf(s[1].infoHash, sizeof s[1].infoHash, "%s", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  s[0].fileIdx = s[1].fileIdx = 0;
  snprintf(s[0].provedor, sizeof s[0].provedor, "A");
  snprintf(s[1].provedor, sizeof s[1].provedor, "B");
  snprintf(s[0].rotulo, sizeof s[0].rotulo, "x");
  snprintf(s[1].rotulo, sizeof s[1].rotulo, "x");
  snprintf(s[2].rotulo, sizeof s[2].rotulo, "y");
  stream_definir_lista(s, 3);
  assert(vt_copias_mesmas(0, 1));
  assert(vt_copia_rep(1) == 0);
  assert(vt_copia_rep(0) == 0);
  assert(vt_copias_n(0) == 1);
  puts("d1_fontes_duplicadas: ok");
  return 0;
}
